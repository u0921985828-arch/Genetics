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

    // ------------------------------------------------------------------
    //  Targeted audio-quality checks (UA lens): dry-path transparency when
    //  bypassed, full state round-trip, and silence-in → silence-out
    //  stability (no self-oscillation / denormal latch).
    // ------------------------------------------------------------------
    auto setParam = [&] (const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (v);
    };

    // 1) Bypass transparency: bypassed output must equal the dry input once the
    //    engage crossfade has settled (the dry path is zero-latency).
    {
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        setParam (chrona::params::id::bypass, 1.0f);
        setParam (chrona::params::id::mix, 1.0f);
        setParam (chrona::params::id::space, 0.0f);
        setParam (chrona::params::id::texture, 0.0f);

        juce::MidiBuffer midi;
        float maxDiff = 0.0f;
        for (int blk = 0; blk < 24; ++blk)
        {
            juce::AudioBuffer<float> buf (2, 512), in (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 512; ++n)
                {
                    const float s = 0.5f * std::sin (2.0f * 3.14159265f * 220.0f * (float) (blk * 512 + n) / 48000.0f);
                    buf.setSample (c, n, s); in.setSample (c, n, s);
                }
            proc.processBlock (buf, midi);
            if (blk >= 20) // after the ~6 ms engage smoother has settled
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < 512; ++n)
                        maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (c, n) - in.getSample (c, n)));
        }
        if (maxDiff > 1.0e-3f) { std::printf ("FAIL [bypass]: dry path not transparent, maxDiff=%g\n", (double) maxDiff); ++failures; }
        setParam (chrona::params::id::bypass, 0.0f);
    }

    // 2) State round-trip: perturb → capture → zero → restore → re-capture must
    //    be byte-identical (compares the serialised state, so bool/choice
    //    quantisation is handled by the format itself).
    {
        Rng r2;
        const auto& params = proc.getParameters();
        for (int i = 0; i < params.size(); ++i) params[i]->setValueNotifyingHost (r2.uni());
        juce::MemoryBlock mb1; proc.getStateInformation (mb1);
        for (int i = 0; i < params.size(); ++i) params[i]->setValueNotifyingHost (0.0f);
        proc.setStateInformation (mb1.getData(), (int) mb1.getSize());
        juce::MemoryBlock mb2; proc.getStateInformation (mb2);
        if (mb1 != mb2) { std::printf ("FAIL [state]: round-trip not byte-identical (%d vs %d bytes)\n",
                                       (int) mb1.getSize(), (int) mb2.getSize()); ++failures; }
    }

    // 3) Silence in → silence out, with worst-case tail settings, across modes.
    {
        proc.prepareToPlay (48000.0, 256);
        setParam (chrona::params::id::mix, 1.0f);
        setParam (chrona::params::id::space, 1.0f);
        setParam (chrona::params::id::texture, 0.0f);
        juce::MidiBuffer midi;
        for (int mode = 0; mode < (int) chrona::params::Mode::NumModes; ++mode)
        {
            if (auto* mp = proc.apvts.getParameter (chrona::params::id::mode))
                mp->setValueNotifyingHost (mp->convertTo0to1 ((float) mode));
            float tail = 0.0f;
            for (int blk = 0; blk < 40; ++blk)
            {
                juce::AudioBuffer<float> buf (2, 256); buf.clear();
                proc.processBlock (buf, midi);
                if (blk >= 36)
                    for (int c = 0; c < 2; ++c)
                        for (int n = 0; n < 256; ++n) tail = juce::jmax (tail, std::abs (buf.getSample (c, n)));
            }
            if (tail > 1.0e-3f) { std::printf ("FAIL [silence]: mode %d rings on silence, tail=%g\n", mode, (double) tail); ++failures; }
        }
    }

    // 4) Mono layout + external-sidechain paths (previously untested).
    {
        // -- mono in/out, sidechain disabled --
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add  (juce::AudioChannelSet::mono());
        mono.inputBuses.add  (juce::AudioChannelSet::disabled());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        if (proc.setBusesLayout (mono))
        {
            proc.prepareToPlay (48000.0, 256);
            juce::MidiBuffer midi;
            for (int mode = 0; mode < (int) chrona::params::Mode::NumModes; ++mode)
            {
                if (auto* mp = proc.apvts.getParameter (chrona::params::id::mode))
                    mp->setValueNotifyingHost (mp->convertTo0to1 ((float) mode));
                for (int blk = 0; blk < 8; ++blk)
                {
                    juce::AudioBuffer<float> buf (1, 256);
                    for (int n = 0; n < 256; ++n) buf.setSample (0, n, rng.bip() * 0.5f);
                    proc.processBlock (buf, midi);
                    checkFinite (buf, "mono");
                }
            }
        }
        else std::printf ("NOTE [mono]: layout not accepted, skipped\n");

        // -- stereo main + external stereo sidechain, ducker engaged --
        juce::AudioProcessor::BusesLayout sc;
        sc.inputBuses.add  (juce::AudioChannelSet::stereo());
        sc.inputBuses.add  (juce::AudioChannelSet::stereo());
        sc.outputBuses.add (juce::AudioChannelSet::stereo());
        if (proc.setBusesLayout (sc))
        {
            proc.prepareToPlay (48000.0, 256);
            setParam (chrona::params::id::scSource,   1.0f);  // choice 3 = External
            setParam (chrona::params::id::duckAmount, 0.6f);
            juce::MidiBuffer midi;
            const int totalCh = juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
            for (int blk = 0; blk < 16; ++blk)
            {
                juce::AudioBuffer<float> buf (totalCh, 256);
                for (int c = 0; c < totalCh; ++c)
                    for (int n = 0; n < 256; ++n) buf.setSample (c, n, rng.bip() * 0.5f);
                proc.processBlock (buf, midi);
                checkFinite (buf, "sidechain");
            }
            setParam (chrona::params::id::scSource,   0.0f);
            setParam (chrona::params::id::duckAmount, 0.0f);
        }
        else std::printf ("NOTE [sidechain]: layout not accepted, skipped\n");
    }

    if (failures == 0) { std::printf ("ALL TESTS PASSED\n"); return 0; }
    std::printf ("%d FAILURES\n", failures);
    return 1;
}
