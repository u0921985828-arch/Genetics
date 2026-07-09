// ============================================================================
//  CHRONA offline stress test (headless, no GUI).
//
//  Instantiates the full ChronaProcessor and hammers processBlock across every
//  mode, sample rate, and block size, with randomised parameters, transport
//  (including locate/loop jumps), and MIDI. A background thread edits the
//  curves while Custom mode runs, to exercise the SpinLock hand-off.
//
//  Built only when -DCHRONA_BUILD_TESTS=ON, normally with ASan+UBSan. The test
//  asserts every output sample stays finite and bounded and that nothing
//  crashes / trips a sanitizer. It is NOT shipped in the plugin.
// ============================================================================

#include "../Source/PluginProcessor.h"

#include <atomic>
#include <thread>
#include <cstdio>
#include <cmath>

namespace
{
    // Deterministic RNG (no <random> nondeterminism across platforms).
    struct Rng
    {
        uint64_t s = 0x123456789abcdefULL;
        uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t) (s >> 32); }
        float uni() { return (float) (next() >> 8) / 16777216.0f; }
        float bip() { return uni() * 2.0f - 1.0f; }
        int   range (int n) { return n > 0 ? (int) (next() % (uint32_t) n) : 0; }
    };

    // Minimal host transport that can play, loop and locate.
    class MockPlayHead : public juce::AudioPlayHead
    {
    public:
        double bpm = 128.0, ppq = 0.0, sr = 48000.0;
        bool   playing = true;

        void advance (int numSamples)
        {
            if (playing)
                ppq += (numSamples / sr) * (bpm / 60.0);
        }
        void locate (double newPpq) { ppq = newPpq; }

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            p.setIsPlaying (playing);
            juce::AudioPlayHead::TimeSignature ts; ts.numerator = 4; ts.denominator = 4;
            p.setTimeSignature (ts);
            return p;
        }
    };

    int failures = 0;

    bool checkFinite (const juce::AudioBuffer<float>& b, const char* ctx)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int n = 0; n < b.getNumSamples(); ++n)
            {
                const float v = b.getSample (c, n);
                if (! std::isfinite (v) || std::abs (v) > 8.0f)
                {
                    std::printf ("FAIL [%s]: bad sample ch%d n%d = %g\n", ctx, c, n, (double) v);
                    ++failures;
                    return false;
                }
            }
        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit; // message manager + singletons

    Rng rng;
    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const int    blockSizes[]  = { 16, 64, 127, 512, 2048 };

    chrona::ChronaProcessor proc;

    // Background curve editor: stresses the audio-thread curve hand-off.
    std::atomic<bool> editing { true };
    std::thread editor ([&]
    {
        Rng er;
        while (editing.load())
        {
            chrona::automation::Curve c;
            const int pts = 2 + er.range (24);
            std::vector<chrona::automation::Point> v;
            for (int i = 0; i < pts; ++i)
                v.push_back ({ er.uni(), er.uni(), er.bip() });
            c.setPoints (v);
            proc.automation.publishTimeCurve (c);
            proc.automation.publishVolumeCurve (c);
        }
    });

    for (double sr : sampleRates)
    {
        for (int bs : blockSizes)
        {
            auto* playHead = new MockPlayHead();
            playHead->sr = sr;
            proc.setPlayHead (playHead);

            proc.setRateAndBufferSizeDetails (sr, bs);
            proc.prepareToPlay (sr, bs);

            const int numParams = proc.getParameters().size();

            for (int mode = 0; mode < (int) chrona::params::Mode::NumModes; ++mode)
            {
                // set mode
                if (auto* mp = proc.apvts.getParameter (chrona::params::id::mode))
                    mp->setValueNotifyingHost (mp->convertTo0to1 ((float) mode));

                for (int iter = 0; iter < 40; ++iter)
                {
                    // randomise every parameter occasionally
                    if (iter % 4 == 0)
                        for (int p = 0; p < numParams; ++p)
                            if (auto* param = proc.getParameters()[p])
                                param->setValueNotifyingHost (rng.uni());
                    // keep the mode pinned (the loop above may have changed it)
                    if (auto* mp = proc.apvts.getParameter (chrona::params::id::mode))
                        mp->setValueNotifyingHost (mp->convertTo0to1 ((float) mode));

                    juce::AudioBuffer<float> buf (2, bs);
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < bs; ++n)
                            buf.setSample (c, n, rng.bip() * 0.5f);

                    juce::MidiBuffer midi;
                    if (rng.uni() < 0.3f)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), rng.range (bs));
                    if (rng.uni() < 0.3f)
                        midi.addEvent (juce::MidiMessage::noteOff (1, 60), rng.range (bs));

                    // occasional transport play/stop + locate/loop jump
                    if (rng.uni() < 0.1f) playHead->playing = ! playHead->playing;
                    if (rng.uni() < 0.1f) playHead->locate (rng.uni() * 64.0);

                    proc.processBlock (buf, midi);
                    playHead->advance (bs);

                    checkFinite (buf, "processBlock");
                }
            }

            proc.setPlayHead (nullptr);
            delete playHead;
        }
    }

    editing.store (false);
    editor.join();

    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("%d FAILURES\n", failures);
    return 1;
}
