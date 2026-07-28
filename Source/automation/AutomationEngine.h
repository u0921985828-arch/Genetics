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
            // sensible defaults + pre-grown storage so the audio-thread copy
            // reuses capacity instead of allocating.
            timeMaster.reserve (kMaxPoints); timeMaster.setLinearRamp (0.0f, 0.0f); // live
            volMaster .reserve (kMaxPoints); volMaster .setFlat (1.0f);
        }

        static constexpr int kMaxPoints = 512;
        // The audio thread copies a published curve into a snapshot reserved to
        // kMaxPoints, so the master's hard cap must not exceed it — else the RT
        // copy could reallocate.
        static_assert (Curve::kMaxPoints <= kMaxPoints,
                       "Curve point cap must not exceed the RT snapshot reserve");

        void prepare (double sr)
        {
            sampleRate = sr;
            freePhase  = 0.0;
        }

        void setBufferBars (int bars) { bufferBars = juce::jlimit (1, 2, bars); }
        // Length of one Time-Warp curve cycle in bars (decoupled from the buffer
        // window) — this is what TimeShaper calls the envelope RATE.
        void setWarpBars (double bars) { warpBars = juce::jlimit (0.125, 8.0, bars); }

        // --- phase clock -----------------------------------------------------
        void updateTransport (const TransportInfo& t)
        {
            transport = t;
            sampleRate = t.sampleRate;

            beatsPerBar   = (double) t.numerator * (4.0 / (double) juce::jmax (1, t.denominator));
            patternBeats  = beatsPerBar * (double) bufferBars;
            warpBeats     = beatsPerBar * warpBars;                 // Time-Warp cycle
            samplesPerBeat = (60.0 / juce::jmax (1.0, t.bpm)) * sampleRate;
        }

        // Phase at the START of the current block, over one WARP cycle. Per-sample
        // advance uses phaseIncrementPerSample() so blocks stay sample-accurate.
        double blockStartPhase() const
        {
            const double wb = warpBeats > 0.0 ? warpBeats : patternBeats;
            if (transport.isPlaying && wb > 0.0)
                return std::fmod (transport.ppqPosition / wb, 1.0);
            return freePhase;
        }

        double phaseIncrementPerSample() const
        {
            const double wb = warpBeats > 0.0 ? warpBeats : patternBeats;
            const double samplesPerPattern = wb * samplesPerBeat;
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

        // --- curve hand-off (audio thread never blocks) ---------------------
        //
        // A single spinlock guards each master curve. The message thread takes
        // the full lock to publish or snapshot (it may spin briefly). The audio
        // thread only ever *tries* the lock: on the rare block where the editor
        // holds it, the audio thread keeps its previous snapshot instead of
        // blocking — so there is no priority inversion and, crucially, the
        // editor can never free/realloc a vector the audio thread is copying.

        // Audio thread: refresh `dest` from the master if the lock is free.
        bool tryCopyTimeCurve (Curve& dest) const
        {
            const juce::SpinLock::ScopedTryLockType l (timeLock);
            if (l.isLocked()) { dest = timeMaster; return true; }
            return false;
        }
        bool tryCopyVolumeCurve (Curve& dest) const
        {
            const juce::SpinLock::ScopedTryLockType l (volLock);
            if (l.isLocked()) { dest = volMaster; return true; }
            return false;
        }

        // Message thread: publish an edited curve.
        void publishTimeCurve (const Curve& edited)
        {
            const juce::SpinLock::ScopedLockType l (timeLock);
            timeMaster = edited;
        }
        void publishVolumeCurve (const Curve& edited)
        {
            const juce::SpinLock::ScopedLockType l (volLock);
            volMaster = edited;
        }

        // Message thread: read the currently-published curve to seed an editor.
        Curve snapshotTimeCurve()   const { const juce::SpinLock::ScopedLockType l (timeLock); return timeMaster; }
        Curve snapshotVolumeCurve() const { const juce::SpinLock::ScopedLockType l (volLock);  return volMaster; }

    private:
        double sampleRate    = 44100.0;
        double samplesPerBeat = 22050.0;
        double beatsPerBar   = 4.0;
        double patternBeats  = 8.0;   // 2 bars in 4/4
        double freePhase     = 0.0;
        double warpBars      = 2.0;
        double warpBeats     = 8.0;   // 2 bars in 4/4
        int    bufferBars    = 2;

        TransportInfo transport;

        Curve timeMaster, volMaster;
        mutable juce::SpinLock timeLock, volLock;
    };
}
