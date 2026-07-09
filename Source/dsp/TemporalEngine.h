#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>
#include <atomic>

#include "CircularBuffer.h"
#include "Modes.h"
#include "AntiClick.h"
#include "DspUtils.h"
#include "Space.h"
#include "../automation/AutomationEngine.h"
#include "../midi/MidiTriggerSystem.h"
#include "../params/Parameters.h"

namespace chrona::dsp
{
    // ========================================================================
    //  TemporalEngine — the DSP orchestrator. It owns the shared ring buffer,
    //  every mode, and the post-mode colour/dynamics chain, and it drives the
    //  musical loop clock (with swing + humanize) that the rhythmic modes ride.
    //
    //  It reads timing and curves from the AutomationEngine and the engage
    //  state from the MidiTriggerSystem, but owns neither — clean separation.
    // ========================================================================
    class TemporalEngine
    {
    public:
        struct Macros
        {
            float time = 0.5f, depth = 1.0f, mix = 1.0f;
            float texture = 0.0f, space = 0.0f, width = 0.5f;
        };

        struct Level2
        {
            int   syncIndex = 2;
            int   bufferBars = 2;
            params::Quality quality = params::Quality::Hermite;
            float antiClickMs = 3.0f;
            float swing = 0.0f;
            float humanize = 0.0f;
            float smartFade = 0.5f;
            float gate = 0.0f;
            float duck = 0.0f;
            float duckAtkMs = 5.0f, duckRelMs = 120.0f;
            int   scSource = 0;
        };

        TemporalEngine();

        void prepare (double sampleRate, int numChannels, int maxBlock);
        void reset();

        void setMode (params::Mode m) { requestedMode = m; }
        void setMacros (const Macros& m) { macros = m; }
        void setLevel2 (const Level2& l) { level2 = l; }

        // Called each block before process() to hand in timing + curves.
        void setAutomation (automation::AutomationEngine* a) { automation = a; }

        // Process one interleaved-by-JUCE buffer. `engaged` gates the effect.
        void process (juce::AudioBuffer<float>& buffer, bool engaged);

        // --- visualiser feed (read from message thread) ---------------------
        double getPlayheadPhase()  const { return visPhase.load (std::memory_order_relaxed); }
        double getReadDelayNorm()  const { return visDelay.load (std::memory_order_relaxed); }
        const CircularBuffer& getBuffer() const { return buffer; }

    private:
        IMode* activeMode();
        void   updateLoopClock (bool playing, double ppqSamples, bool forceResync);

        CircularBuffer buffer;
        automation::AutomationEngine* automation = nullptr;

        std::array<std::unique_ptr<IMode>, (size_t) params::Mode::NumModes> modes;
        params::Mode requestedMode = params::Mode::Half;

        Macros macros;
        Level2 level2;

        // loop clock state
        double loopPos = 0.0;
        long long loopIndex = 0;
        long long anchorAbs = 0;
        double currentLoopLen = 0.0;
        bool   wasPlaying = false;
        double windowSamples = 0.0;
        double barSamples = 0.0;
        float  humanizeGain = 1.0f;
        Lcg    humanRng;

        // locate/loop-jump detection + live mode-switch declick
        double lastPpqSamples = 0.0;
        int    lastBlockSamples = 0;
        bool   havePrevPpq = false;
        params::Mode lastProcessedMode = params::Mode::Half;

        // post chain
        std::array<DeclickRamp, 2> declick;
        std::array<TiltFilter, 2>  tilt;
        std::array<EnvelopeFollower, 2> scEnv;
        Space space;
        Lcg   crackleRng;

        OnePole engageSmoother;
        std::array<OnePole, 6> macroSmooth; // time,depth,mix,texture,space,width

        std::array<float, 2> lastWet { { 0.0f, 0.0f } };

        // block-local curve snapshots for Custom mode (avoids RT allocation)
        automation::Curve customTimeSnapshot, customVolSnapshot;

        double sampleRate = 44100.0;
        int    channels = 2;

        std::atomic<double> visPhase { 0.0 };
        std::atomic<double> visDelay { 0.0 };
    };
}
