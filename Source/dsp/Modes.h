#pragma once

#include <cmath>
#include <algorithm>
#include "CircularBuffer.h"
#include "AntiClick.h"
#include "../automation/Curve.h"
#include "../params/Parameters.h"

namespace chrona::dsp
{
    // Deterministic LCG — reproducible "randomness" for glitch/humanize without
    // touching the C library RNG on the audio thread.
    struct Lcg
    {
        uint32_t s = 0x9E3779B9u;
        void seed (uint32_t v) { s = v ? v : 0x9E3779B9u; }
        inline uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
        inline float unipolar() { return (float) (next() >> 8) / 16777216.0f; } // 0..1
        inline int   range (int n) { return n > 0 ? (int) (next() % (uint32_t) n) : 0; }
    };

    // Per-sample context handed to a mode. `read()` fetches from the shared
    // ring by ABSOLUTE source index (in total-written units), clamped to the
    // readable window — modes think in terms of "where on the timeline".
    struct ModeContext
    {
        const CircularBuffer* buffer = nullptr;
        long long totalWritten = 0;    // includes the sample just written
        long long anchorAbs    = 0;    // live-edge index captured at loop start
        double    localSamples = 0.0;  // samples elapsed since loop start
        double    loopLenSamples = 0.0;
        bool      loopReset    = false;
        double    phase        = 0.0;  // pattern phase 0..1 (Custom mode)
        float     time = 0.5f, depth = 1.0f;
        float     smartFade = 0.5f;    // slice edge fade shape
        double    sampleRate = 44100.0;
        double    samplesPerBeat = 22050.0;
        double    windowSamples = 0.0;
        params::Quality quality = params::Quality::Hermite;

        inline float read (int ch, double srcAbs) const
        {
            double delay = (double) (totalWritten - 1) - srcAbs;
            delay = std::clamp (delay, 0.0, windowSamples);
            return buffer->read (ch, delay, quality);
        }
    };

    // ------------------------------------------------------------------------
    struct IMode
    {
        virtual ~IMode() = default;
        virtual void prepare (double sr, int ch) { sampleRate = sr; channels = ch; }
        virtual void reset() {}
        virtual const char* name() const = 0;
        // Fill out[0..channels-1] with the wet frame.
        virtual void process (const ModeContext& ctx, float* out) = 0;
    protected:
        double sampleRate = 44100.0;
        int channels = 2;
    };

    // ===== Speed-warp modes (Half / Double / Reverse) =======================
    class SpeedMode : public IMode
    {
    public:
        enum Kind { Half, Double, Reverse };
        explicit SpeedMode (Kind k) : kind (k) {}
        const char* name() const override
        {
            return kind == Half ? "Half" : kind == Double ? "Double" : "Reverse";
        }

        void process (const ModeContext& ctx, float* out) override
        {
            const double L = ctx.loopLenSamples;
            const double local = ctx.localSamples;
            const double a = (double) ctx.anchorAbs;

            double srcAbs;
            switch (kind)
            {
                case Half:    srcAbs = (a - L)       + 0.5 * local; break;
                case Double:  srcAbs = (a - 2.0 * L) + 2.0 * local; break;
                case Reverse: default: srcAbs = a - local;          break;
            }
            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs);
        }
    private:
        Kind kind;
    };

    // ===== Tape Stop ========================================================
    class TapeStopMode : public IMode
    {
    public:
        const char* name() const override { return "Tape Stop"; }
        void reset() override { srcAbs = 0.0; primed = false; }

        void process (const ModeContext& ctx, float* out) override
        {
            if (ctx.loopReset || ! primed)
            {
                srcAbs = (double) ctx.anchorAbs; // start at loop-start live edge
                primed = true;
            }
            // Stop time scales with Time (short..long); Depth scales the curve.
            const double stopSamples = ctx.loopLenSamples * (0.15 + 0.85 * ctx.time);
            const double t = std::clamp (ctx.localSamples / std::max (1.0, stopSamples), 0.0, 1.0);
            // exponential deceleration reads more like a real motor spin-down
            const double speed = std::pow (1.0 - t, 1.0 + 2.0 * ctx.depth);

            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs);

            srcAbs += speed; // advance once per frame
        }
    private:
        double srcAbs = 0.0;
        bool primed = false;
    };

    // ===== Slice-repeat family (Stutter / Beat Repeat) ======================
    class SliceMode : public IMode
    {
    public:
        enum Kind { Stutter, BeatRepeat };
        explicit SliceMode (Kind k) : kind (k) {}
        const char* name() const override { return kind == Stutter ? "Stutter" : "Beat Repeat"; }

        void process (const ModeContext& ctx, float* out) override
        {
            double sliceLen;
            if (kind == Stutter)
            {
                const int subdiv = 1 + (int) std::round (ctx.time * 7.0);       // 1..8
                sliceLen = std::max (32.0, ctx.loopLenSamples / (double) subdiv);
            }
            else // BeatRepeat: slice ~ one beat, scaled by Time
            {
                const double beats = 0.25 + 0.75 * (double) (1 + (int) std::round (ctx.time * 3.0));
                sliceLen = std::max (64.0, ctx.samplesPerBeat * (beats * 0.25));
            }
            // Never let the slice exceed the readable window (guards degenerate
            // sub-one-beat meters where a beat is longer than the buffer).
            if (ctx.windowSamples > 64.0)
                sliceLen = std::min (sliceLen, ctx.windowSamples * 0.5);

            const double posInSlice = std::fmod (ctx.localSamples, sliceLen);
            const double sliceStart = (double) ctx.anchorAbs - sliceLen;
            const double srcAbs = sliceStart + posInSlice;

            const float win = SmartFade::gain ((int) posInSlice, (int) sliceLen, ctx.smartFade);

            // Beat Repeat fades successive repeats by Depth.
            float rg = 1.0f;
            if (kind == BeatRepeat && ctx.depth > 0.0f)
            {
                const int rep = (int) (ctx.localSamples / sliceLen);
                rg = std::pow (1.0f - 0.18f * ctx.depth, (float) rep);
            }

            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs) * win * rg;
        }
    private:
        Kind kind;
    };

    // ===== Vinyl (wow/flutter + near-live playback) =========================
    class VinylMode : public IMode
    {
    public:
        const char* name() const override { return "Vinyl"; }
        void prepare (double sr, int ch) override { IMode::prepare (sr, ch); reset(); }
        void reset() override { srcAbs = 0.0; primed = false; phaseWow = 0.0; phaseFlut = 0.0; }

        void process (const ModeContext& ctx, float* out) override
        {
            if (! primed) { srcAbs = (double) (ctx.totalWritten - 1) - ctx.loopLenSamples; primed = true; }

            // wow (~0.8Hz) + flutter (~7Hz); depth scales pitch deviation.
            const double wow  = std::sin (phaseWow)  * (0.006 * ctx.depth);
            const double flut = std::sin (phaseFlut) * (0.0025 * ctx.depth);
            const double speed = 1.0 + wow + flut;

            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs);

            srcAbs += speed;
            // keep close to live so it never runs dry
            const double liveEdge = (double) (ctx.totalWritten - 1) - ctx.loopLenSamples * 0.25;
            if (srcAbs > liveEdge) srcAbs = liveEdge;

            constexpr double kTwoPi = 6.283185307179586;
            phaseWow  += kTwoPi * 0.8 / sampleRate;
            phaseFlut += kTwoPi * 7.0 / sampleRate;
            if (phaseWow  > kTwoPi) phaseWow  -= kTwoPi;
            if (phaseFlut > kTwoPi) phaseFlut -= kTwoPi;
        }
    private:
        double srcAbs = 0.0, phaseWow = 0.0, phaseFlut = 0.0;
        bool primed = false;
    };

    // ===== Glitch (stochastic slice/pitch/reverse/gate) =====================
    class GlitchMode : public IMode
    {
    public:
        const char* name() const override { return "Glitch"; }
        void reset() override { sliceIndex = -1; }

        void process (const ModeContext& ctx, float* out) override
        {
            // base slice grid from Time
            const int subdiv = 2 + (int) std::round (ctx.time * 14.0);        // 2..16
            const double baseSlice = std::max (32.0, ctx.loopLenSamples / (double) subdiv);
            const int idx = (int) (ctx.localSamples / baseSlice);

            if (idx != sliceIndex)
            {
                sliceIndex = idx;
                rng.seed ((uint32_t) (ctx.anchorAbs / 64 + idx * 2654435761u));
                // choose behaviour for this slice
                curReverse = rng.unipolar() < (0.25f + 0.4f * ctx.depth);
                const float r = rng.unipolar();
                curSpeed = r < 0.4f ? 1.0f : (r < 0.7f ? 0.5f : 2.0f);
                curGate  = rng.unipolar() < (0.15f * ctx.depth) ? 0.0f : 1.0f;
                // keep the whole slice within the recorded window (origin +
                // baseSlice*speed must not pass the loop-start live edge).
                const int maxOffset = (int) std::max (0.0, ctx.loopLenSamples - baseSlice * curSpeed);
                curOffset = (double) rng.range (maxOffset);
            }

            const double posInSlice = std::fmod (ctx.localSamples, baseSlice);
            const double sliceOriginAbs = (double) ctx.anchorAbs - ctx.loopLenSamples + curOffset;
            double srcAbs;
            if (curReverse)
                srcAbs = sliceOriginAbs + (baseSlice - posInSlice) * curSpeed;
            else
                srcAbs = sliceOriginAbs + posInSlice * curSpeed;

            const float win = SmartFade::gain ((int) posInSlice, (int) baseSlice,
                                               0.15f + 0.5f * ctx.smartFade);
            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs) * win * curGate;
        }
    private:
        Lcg rng;
        int sliceIndex = -1;
        bool curReverse = false;
        float curSpeed = 1.0f, curGate = 1.0f;
        double curOffset = 0.0;
    };

    // ===== Custom (PAT) — driven by the editable time/volume curves =========
    class CustomMode : public IMode
    {
    public:
        const char* name() const override { return "Time Warp"; }
        void setCurves (const automation::Curve* timeC, const automation::Curve* volC)
        {
            timeCurve = timeC; volCurve = volC;
        }
        void process (const ModeContext& ctx, float* out) override
        {
            const float ty = timeCurve ? timeCurve->value ((float) ctx.phase) : 0.0f;
            const float vy = volCurve  ? volCurve->value  ((float) ctx.phase) : 1.0f;
            const double delay = (double) ty * ctx.windowSamples;
            const double srcAbs = (double) (ctx.totalWritten - 1) - delay;
            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs) * vy;
        }
    private:
        const automation::Curve* timeCurve = nullptr;
        const automation::Curve* volCurve  = nullptr;
    };
}
