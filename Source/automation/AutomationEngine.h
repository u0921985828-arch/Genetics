#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include "Curve.h"

namespace chrona::automation
{
    // Timing snapshot derived from the host each block.
    struct TransportInfo
    {
        double bpm         = 120.0;
        double ppqPosition = 0.0;     // quarter notes since song start
        double sampleRate  = 44100.0;
        int    numerator   = 4;
        int    denominator = 4;
        bool   isPlaying   = false;
    };

    // ========================================================================
    //  AutomationEngine — owns musical time and the two internal curves.
    //
    //  Responsibilities:
    //    * Convert host transport into a normalised pattern phase [0,1) that
    //      the Custom mode and the visualiser follow.
    //    * Hold the Time and Volume curves with a lock-free double-buffered
    //      hand-off: the editor edits a staging copy and publish()es it; the
    //      audio thread reads whichever slot is currently active.
    //
    //  It does NOT touch host-parameter automation — that is APVTS's job and is
    //  read directly by the processor. This class is only the *internal*
    //  time-based automation (the curves + phase clock).
    // ========================================================================
    class AutomationEngine
    {
    public:
        AutomationEngine()
        {
            // sensible defaults
            for (auto& c : timeSlots)   c.setLinearRamp (0.0f, 0.0f); // no delay = live
            for (auto& c : volSlots)    c.setFlat (1.0f);
        }

        void prepare (double sr)
        {
            sampleRate = sr;
            freePhase  = 0.0;
        }

        void setBufferBars (int bars) { bufferBars = juce::jlimit (1, 2, bars); }

        // --- phase clock -----------------------------------------------------
        void updateTransport (const TransportInfo& t)
        {
            transport = t;
            sampleRate = t.sampleRate;

            beatsPerBar   = (double) t.numerator * (4.0 / (double) t.denominator);
            patternBeats  = beatsPerBar * (double) bufferBars;
            samplesPerBeat = (60.0 / juce::jmax (1.0, t.bpm)) * sampleRate;
        }

        // Phase at the START of the current block. Per-sample advance uses
        // advancePhase() so blocks stay sample-accurate.
        double blockStartPhase() const
        {
            if (transport.isPlaying && patternBeats > 0.0)
                return std::fmod (transport.ppqPosition / patternBeats, 1.0);
            return freePhase;
        }

        double phaseIncrementPerSample() const
        {
            const double samplesPerPattern = patternBeats * samplesPerBeat;
            return samplesPerPattern > 0.0 ? 1.0 / samplesPerPattern : 0.0;
        }

        // Called once per processed sample when the host is NOT playing so the
        // effect still free-runs (useful for standalone / preview).
        void advanceFreePhase()
        {
            freePhase += phaseIncrementPerSample();
            if (freePhase >= 1.0) freePhase -= 1.0;
        }

        double getSamplesPerBeat() const { return samplesPerBeat; }
        double getPatternBeats()   const { return patternBeats; }
        const TransportInfo& getTransport() const { return transport; }

        // --- lock-free curve hand-off ---------------------------------------
        // Audio thread: get the live curves for this block.
        const Curve& liveTimeCurve()   const { return timeSlots[(size_t) timeIdx.load (std::memory_order_acquire)]; }
        const Curve& liveVolumeCurve() const { return volSlots [(size_t) volIdx.load  (std::memory_order_acquire)]; }

        // Message thread: publish an edited curve.
        void publishTimeCurve (const Curve& edited)
        {
            const int next = 1 - timeIdx.load (std::memory_order_relaxed);
            timeSlots[(size_t) next] = edited;
            timeIdx.store (next, std::memory_order_release);
        }
        void publishVolumeCurve (const Curve& edited)
        {
            const int next = 1 - volIdx.load (std::memory_order_relaxed);
            volSlots[(size_t) next] = edited;
            volIdx.store (next, std::memory_order_release);
        }

        // Message thread: read the currently-published curve to seed an editor.
        Curve snapshotTimeCurve()   const { return timeSlots[(size_t) timeIdx.load (std::memory_order_acquire)]; }
        Curve snapshotVolumeCurve() const { return volSlots [(size_t) volIdx.load  (std::memory_order_acquire)]; }

    private:
        double sampleRate    = 44100.0;
        double samplesPerBeat = 22050.0;
        double beatsPerBar   = 4.0;
        double patternBeats  = 8.0;   // 2 bars in 4/4
        double freePhase     = 0.0;
        int    bufferBars    = 2;

        TransportInfo transport;

        std::array<Curve, 2> timeSlots;
        std::array<Curve, 2> volSlots;
        std::atomic<int> timeIdx { 0 };
        std::atomic<int> volIdx  { 0 };
    };
}
