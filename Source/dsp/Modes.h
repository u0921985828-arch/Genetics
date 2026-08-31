#pragma once

#include <cmath>
#include <algorithm>
#include <array>
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
        inline float bip() { return unipolar() * 2.0f - 1.0f; }                 // -1..1
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
        double    phaseInc     = 0.0;  // phase advance per sample (Custom mode)
        float     time = 0.5f, depth = 1.0f;
        float     smartFade = 0.5f;    // slice edge fade shape
        double    sampleRate = 44100.0;
        double    samplesPerBeat = 22050.0;
        double    windowSamples = 0.0;
        params::Quality quality = params::Quality::Hermite;

        // Right-hand interpolation reach: sinc fetches up to center+4, Hermite
        // up to +2. Holding every read at least this far behind the write head
        // guarantees no tap ever crosses the live edge into stale, pre-wrap
        // audio (which would splice previous-lap samples into the newest frame).
        static constexpr double kReadGuard = 4.0;

        inline float read (int ch, double srcAbs, double readRate = 1.0) const
        {
            double delay = (double) (totalWritten - 1) - srcAbs;
            delay = std::clamp (delay, kReadGuard, std::max (kReadGuard, windowSamples)); // hi>=lo always
            return buffer->read (ch, delay, quality, readRate);
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

            double srcAbs, rate;
            switch (kind)
            {
                case Half:    srcAbs = (a - L)       + 0.5 * local; rate = 0.5; break;
                case Double:  srcAbs = (a - 2.0 * L) + 2.0 * local; rate = 2.0; break;
                case Reverse: default: srcAbs = a - local;          rate = 1.0; break;
            }
            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs, rate);
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
                out[c] = ctx.read (c, srcAbs, juce::jmax (1.0, speed));

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
        void reset() override { lastRep = -1; repRate = 1.0; repRev = false; repRg = 1.0f; }

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
            const float win = SmartFade::gain ((int) posInSlice, (int) sliceLen, ctx.smartFade);

            // Per-repeat variation makes Beat Repeat evolve instead of hammering
            // one identical slice: Depth fades each repeat and, stochastically,
            // re-pitches (½× / 2×) or reverses it. Stutter stays clean (rate 1).
            double rate = 1.0; bool rev = false; float rg = 1.0f;
            if (kind == BeatRepeat && ctx.depth > 0.0f)
            {
                // Decide the per-repeat variation once per repeat (not once per
                // sample) — same deterministic result, far less work. GlitchMode
                // uses the same slice-index gate.
                const int rep = (int) (ctx.localSamples / sliceLen);
                if (rep != lastRep)
                {
                    lastRep = rep;
                    repRg = std::pow (1.0f - 0.18f * ctx.depth, (float) rep);
                    rng.seed ((uint32_t) (ctx.anchorAbs / 64 + rep * 2654435761u));
                    const float rr = rng.unipolar();
                    repRate = (rr < 0.15f * ctx.depth) ? 2.0 : (rr < 0.28f * ctx.depth ? 0.5 : 1.0);
                    repRev  = rng.unipolar() < 0.20f * ctx.depth;
                }
                rate = repRate; rev = repRev; rg = repRg;
            }

            // Origin sits far enough back that a pitched-up (rate>1) read stays
            // inside the recorded window instead of running into the live edge.
            const double base = (double) ctx.anchorAbs - sliceLen * rate;
            const double f = rev ? (sliceLen - posInSlice) : posInSlice;
            const double srcAbs = base + f * rate;

            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs, rate) * win * rg;
        }
    private:
        Kind kind;
        Lcg  rng;
        int    lastRep = -1;                 // per-repeat cache (Beat Repeat)
        double repRate = 1.0;
        bool   repRev  = false;
        float  repRg   = 1.0f;
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
                out[c] = ctx.read (c, srcAbs, (double) curSpeed) * win * curGate;
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

            // Instantaneous read rate = |d(srcAbs)/dn| = |1 - window·dty/dn|.
            // A steep downward time-curve reads the buffer fast (pitch up) and
            // would alias — feed the rate to the sinc reader so it band-limits.
            double rate = 1.0;
            if (timeCurve && ctx.phaseInc > 0.0)
            {
                const float ty2 = timeCurve->value ((float) std::fmod (ctx.phase + ctx.phaseInc, 1.0));
                rate = std::abs (1.0 - ctx.windowSamples * (double) (ty2 - ty));
            }
            for (int c = 0; c < channels; ++c)
                out[c] = ctx.read (c, srcAbs, rate) * vy;
        }
    private:
        const automation::Curve* timeCurve = nullptr;
        const automation::Curve* volCurve  = nullptr;
    };

    // ===== Freeze — grab a region into a private buffer and loop it =========
    //  The shared ring only holds ~2 bars, so a fixed anchor would slide out of
    //  the window and clamp. Freeze instead captures its slice into its OWN
    //  buffer (incrementally, O(1)/sample) and loops that indefinitely.
    class FreezeMode : public IMode
    {
    public:
        const char* name() const override { return "Freeze"; }

        void prepare (double sr, int ch) override
        {
            IMode::prepare (sr, ch);
            capacity = juce::jmax (1024, (int) (sr * 6.0)); // up to ~1 bar @ 40 BPM
            buf.assign ((size_t) channels, std::vector<float> ((size_t) capacity, 0.0f));
            reset();
        }
        void reset() override
        {
            primed = false; filling = false; fillPos = 0; len = 0;
            posA = 0.0; posB = 0.0; drift = 0.0;
        }

        void process (const ModeContext& ctx, float* out) override
        {
            if (! primed)
            {
                len = juce::jlimit (256, capacity, (int) juce::jmax (256.0, ctx.loopLenSamples));
                filling = true; fillPos = 0; primed = true;
                posA = 0.0; posB = len * 0.5;   // second player half a period out
                drift = 0.0;
            }

            if (filling)
            {
                // still capturing — pass the captured sample through with edge fade
                const float win = SmartFade::gain (fillPos, len, 0.25f + 0.5f * ctx.smartFade);
                for (int c = 0; c < channels; ++c)
                {
                    buf[(size_t) c][(size_t) fillPos] = ctx.read (c, (double) (ctx.totalWritten - 1));
                    out[c] = buf[(size_t) c][(size_t) fillPos] * win;
                }
                if (++fillPos >= len) filling = false;
                return;
            }

            // --- bloom playback: two Hann-windowed players offset half a period
            //     (constant-power overlap → no seam) with a tiny detune + slow
            //     drift so the frozen region evolves instead of combing. ---
            const float detune = 0.002f * ctx.depth;        // ±0.2% * Depth
            const double rA = 1.0 - (double) detune;
            const double rB = 1.0 + (double) detune;
            const double dl = (double) len;
            const double slow = 0.5 + 0.5 * std::sin (drift); // 0..1 slow crossfade tilt

            for (int c = 0; c < channels; ++c)
            {
                const float a = fracRead (c, posA) * hannPos (posA, dl);
                const float b = fracRead (c, posB) * hannPos (posB, dl);
                // slow tilt subtly favours one layer then the other for movement
                out[c] = a * (float) (0.6 + 0.4 * slow) + b * (float) (0.6 + 0.4 * (1.0 - slow));
            }

            posA += rA; if (posA >= dl) posA -= dl;
            posB += rB; if (posB >= dl) posB -= dl;
            drift += 0.15 / juce::jmax (1.0, ctx.sampleRate) * 6.2831853; // ~0.15 Hz
            if (drift > 6.2831853) drift -= 6.2831853;
        }
    private:
        float fracRead (int c, double p) const
        {
            const auto& b = buf[(size_t) c];
            const int L = len;
            int i0 = (int) p; if (i0 >= L) i0 -= L; if (i0 < 0) i0 += L;
            int i1 = i0 + 1; if (i1 >= L) i1 -= L;
            const float f = (float) (p - std::floor (p));
            return b[(size_t) i0] * (1.0f - f) + b[(size_t) i1] * f;
        }
        static float hannPos (double p, double L)
        {
            constexpr double kTwoPi = 6.283185307179586;
            return 0.5f - 0.5f * (float) std::cos (kTwoPi * juce::jlimit (0.0, 1.0, p / juce::jmax (1.0, L)));
        }
        std::vector<std::vector<float>> buf;
        int capacity = 0, len = 0, fillPos = 0;
        double posA = 0.0, posB = 0.0, drift = 0.0;
        bool primed = false, filling = false;
    };

    // ===== Granular — a cloud of overlapping windowed grains ================
    class GranularMode : public IMode
    {
    public:
        const char* name() const override { return "Granular"; }
        void reset() override
        {
            for (auto& g : grains) g.active = false;
            spawnCounter = 0.0;
            rng.seed (0x6C7261);
        }

        void process (const ModeContext& ctx, float* out) override
        {
            const double L = juce::jmax (1024.0, ctx.windowSamples);
            // Time → grain size (more Time = finer grains); Depth → density + spread.
            const double grainLen = juce::jlimit (64.0, L * 0.5,
                                    (0.01 + 0.14 * (1.0 - (double) ctx.time)) * ctx.sampleRate);
            const double spawnEvery = juce::jmax (48.0, grainLen * (0.55 - 0.4 * (double) ctx.depth));
            const double spread = 0.15 + 0.85 * (double) ctx.depth;

            // spawn a grain when due, into a free voice
            if (spawnCounter <= 0.0)
            {
                for (auto& g : grains)
                {
                    if (! g.active)
                    {
                        // per-grain pitch (±½ octave × depth), stereo pan, reverse
                        const double detune = rng.bip() * spread * 0.5;
                        g.rate = std::pow (2.0, detune);
                        const double span = grainLen * g.rate;
                        const double off = rng.unipolar() * spread * juce::jmax (1.0, L - span - 4.0);
                        g.active = true; g.pos = 0.0; g.len = grainLen; g.span = span;
                        g.srcStart = (double) (ctx.totalWritten - 1) - span - off;
                        g.dir = (rng.unipolar() < 0.25 * (double) ctx.depth) ? -1.0 : 1.0;
                        const float pan = 0.25f * juce::MathConstants<float>::pi * (rng.bip() * (float) spread + 1.0f);
                        g.gL = std::cos (pan); g.gR = std::sin (pan);
                        break;
                    }
                }
                spawnCounter += spawnEvery;
            }
            spawnCounter -= 1.0;

            float acc[2] = { 0.0f, 0.0f };
            float wsum = 0.0f;    // summed grain-window energy for overlap normalisation
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float env = hann (g.pos / g.len);
                const double srcPos = (g.dir > 0.0) ? (g.srcStart + g.pos * g.rate)
                                                    : (g.srcStart + g.span - g.pos * g.rate);
                const float s = (channels == 2) ? 0.5f * (ctx.read (0, srcPos, g.rate) + ctx.read (1, srcPos, g.rate))
                                                : ctx.read (0, srcPos, g.rate);
                if (channels == 2) { acc[0] += s * env * g.gL; acc[1] += s * env * g.gR; }
                else                 acc[0] += s * env;
                wsum += env;
                g.pos += 1.0;
                if (g.pos >= g.len) g.active = false;
            }
            // Normalise by the active overlap (sqrt keeps a fuller-than-RMS cloud)
            // so dense/sparse settings hold a steady level instead of pumping.
            const float norm = 1.0f / std::max (1.0f, std::sqrt (wsum));
            for (int c = 0; c < channels; ++c) out[c] = acc[c] * norm;
        }
    private:
        static float hann (double x)
        {
            constexpr double kTwoPi = 6.283185307179586;
            return 0.5f - 0.5f * (float) std::cos (kTwoPi * juce::jlimit (0.0, 1.0, x));
        }
        struct Grain { bool active = false; double pos = 0.0, len = 0.0, srcStart = 0.0;
                       double rate = 1.0, span = 0.0, dir = 1.0; float gL = 0.707f, gR = 0.707f; };
        static constexpr int kVoices = 24;   // dense enough for a lush cloud
        std::array<Grain, kVoices> grains {};
        double spawnCounter = 0.0;
        Lcg rng;
    };
}
