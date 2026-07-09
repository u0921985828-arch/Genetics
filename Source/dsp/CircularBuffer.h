#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include "Interpolation.h"
#include "../params/Parameters.h"

namespace chrona::dsp
{
    // ========================================================================
    //  CircularBuffer — the shared recording ring every mode reads from.
    //
    //  The write head always advances with the incoming stream. Modes derive a
    //  fractional read position (samples behind the write head) and pull audio
    //  through the selected interpolator. The buffer is sized to hold the
    //  largest musical window we support (2 bars) plus interpolation guard
    //  samples, at the current sample rate.
    //
    //  Real-time safe after prepare(): no allocation on the audio thread.
    // ========================================================================
    class CircularBuffer
    {
    public:
        void prepare (double sampleRate, int numChannels, double maxBarSeconds)
        {
            ready.store (false, std::memory_order_release); // block visualiser reads while we reallocate
            sr        = sampleRate;
            channels  = juce::jlimit (1, 2, numChannels);

            // 2 bars max window + generous guard for interpolation & slow tempo.
            const int windowSamples = (int) std::ceil (maxBarSeconds * 2.0 * sampleRate);
            capacity = juce::nextPowerOfTwo (windowSamples + kGuard);
            mask     = capacity - 1;

            data.assign ((size_t) channels, std::vector<float> ((size_t) capacity, 0.0f));
            writeIndex = 0;
            totalWritten = 0;
            ready.store (true, std::memory_order_release);
        }

        // True once storage is allocated; the visualiser skips reads until then
        // so a concurrent prepare() reallocation can't be read mid-flight.
        bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }

        void reset()
        {
            for (auto& ch : data)
                std::fill (ch.begin(), ch.end(), 0.0f);
            writeIndex = 0;
            totalWritten = 0;
        }

        // Push one interleaved-by-channel frame.
        inline void write (const float* frame)
        {
            for (int c = 0; c < channels; ++c)
                data[(size_t) c][(size_t) writeIndex] = frame[c];

            writeIndex = (writeIndex + 1) & mask;
            ++totalWritten;
        }

        // Read channel `c` at `delaySamples` behind the *current* write head,
        // using the requested interpolation quality. delaySamples may be
        // fractional; 0 == the sample just written.
        inline float read (int c, double delaySamples, params::Quality q) const
        {
            // Position of the just-written sample is (writeIndex - 1).
            const double pos = (double) (writeIndex - 1) - delaySamples;
            const auto& buf = data[(size_t) c];
            const int m = mask;

            auto at = [&buf, m] (long i) -> float
            {
                return buf[(size_t) (((int) i) & m)];
            };

            switch (q)
            {
                case params::Quality::Linear:  return interp::linear  (at, wrapPos (pos));
                case params::Quality::Sinc:    return interp::sinc8   (at, wrapPos (pos));
                case params::Quality::Hermite:
                default:                       return interp::hermite (at, wrapPos (pos));
            }
        }

        int    getCapacity()   const noexcept { return capacity; }
        int    getWriteIndex() const noexcept { return writeIndex; }
        int    getChannels()   const noexcept { return channels; }
        double getSampleRate() const noexcept { return sr; }
        long long getTotalWritten() const noexcept { return totalWritten; }

        // Snapshot for the visualiser (message thread). This reads the sample
        // arrays and writeIndex while the audio thread may be writing them — an
        // INTENTIONAL, benign data race: the worst case is a torn/stale float in
        // a waveform scope (inaudible, invisible in practice). Making the whole
        // sample buffer atomic would be absurd for a display; the reallocation
        // hazard (the only unsafe one) is handled separately by isReady().
        float peekMono (double delaySamples) const
        {
            float s = 0.0f;
            for (int c = 0; c < channels; ++c)
                s += read (c, delaySamples, params::Quality::Linear);
            return s / (float) channels;
        }

    private:
        // Keep pos within a large positive range before masking so the floor
        // in the interpolators never goes negative modulo.
        inline double wrapPos (double pos) const
        {
            while (pos < 0.0)   pos += (double) capacity;
            while (pos >= (double) capacity) pos -= (double) capacity;
            return pos;
        }

        static constexpr int kGuard = 16;

        double sr = 44100.0;
        int    channels = 2;
        int    capacity = 0;
        int    mask = 0;
        int    writeIndex = 0;
        long long totalWritten = 0;
        std::vector<std::vector<float>> data;
        std::atomic<bool> ready { false };
    };
}
