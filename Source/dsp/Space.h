#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace chrona::dsp
{
    // Tiny deterministic RNG for per-line modulation spread (no libc RNG on the
    // audio thread; seeded once in prepare()).
    struct SpaceRng
    {
        uint32_t s = 0x2545F491u;
        inline float uni() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; }
        inline float bip() { return uni() * 2.0f - 1.0f; }
    };

    // ========================================================================
    //  Space — an 8-line modulated Feedback Delay Network (FDN) reverb.
    //
    //  Stereo-in / stereo-out: L and R are diffused down independent allpass
    //  chains and injected into alternate lines, so the tail stays 3D instead of
    //  collapsing to mono. A short pre-delay sets the room back from the dry
    //  signal; each line is modulated by its OWN randomised LFO (a few ms, deep
    //  enough to decorrelate the network and kill metallic ringing); damping is
    //  frequency-split so lows ring longer than highs (a lush, natural decay).
    //  RT-safe: all storage is allocated in prepare().
    // ========================================================================
    class Space
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = numChannels < 2 ? 1 : 2;
            const double scale = sr / 44100.0;

            SpaceRng r; r.s = 0x9E3779B9u;

            // prime-ish base lengths (samples @44.1k) for a dense, uncorrelated tail
            static const int baseLen[kLines] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            for (int i = 0; i < kLines; ++i)
            {
                lineLen[i]  = (int) (baseLen[i] * scale);
                const int cap = lineLen[i] + kModDepth + 8;
                line[i].assign ((size_t) cap, 0.0f);
                lp[i] = 0.0f; bp[i] = 0.0f; lineIdx[i] = 0;
                // per-line randomised LFO: rate 0.4..1.4 Hz, random start phase,
                // random depth 60..100% of kModDepth → a decorrelated shimmer.
                lfoPhase[i] = r.uni() * 6.2831853f;
                lfoInc[i]   = (float) ((0.4 + 0.9 * r.uni()) * 2.0 * kPi / sr);
                modDepth[i] = kModDepth * (0.6f + 0.4f * r.uni());
            }

            static const int apLen[kAllpass] = { 225, 341, 441, 556, 193, 389 };
            for (int i = 0; i < kAllpass * 2; ++i)
            {
                ap[i].assign ((size_t) std::max (1, (int) (apLen[i % kAllpass] * scale
                                                           * (1.0 + 0.05 * (i / kAllpass)))), 0.0f);
                apIdx[i] = 0;
            }

            // pre-delay ring (up to ~50 ms), per channel.
            preCap = std::max (1, (int) (0.05 * sr));
            for (int c = 0; c < 2; ++c) { pre[c].assign ((size_t) preCap, 0.0f); preIdx[c] = 0; }

            reset();
        }

        void reset()
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
            for (auto& a : ap)   std::fill (a.begin(), a.end(), 0.0f);
            for (auto& p : pre)  std::fill (p.begin(), p.end(), 0.0f);
            lp.fill (0.0f); bp.fill (0.0f);
        }

        // amount 0..1 → decay + wet send.
        void process (float* frame, float amount)
        {
            if (amount <= 0.0001f) return;

            const float g       = 0.5f + 0.42f * amount;   // feedback / decay (max 0.92)
            const float damp    = 0.18f + 0.14f * amount;  // HF damping in the tail
            const float bassBoost = 0.25f;                 // extra low-freq decay
            const float inGain  = 0.35f;

            // --- pre-delay: set the room back from the dry hit ---
            const int pd = juce::jlimit (1, preCap - 1, (int) ((0.008 + 0.030 * amount) * sr));
            float xin[2] = { frame[0], channels == 2 ? frame[1] : frame[0] };
            float pin[2];
            for (int c = 0; c < 2; ++c)
            {
                pre[c][(size_t) preIdx[c]] = xin[c];
                int rp = preIdx[c] - pd; if (rp < 0) rp += preCap;
                pin[c] = pre[c][(size_t) rp];
                if (++preIdx[c] >= preCap) preIdx[c] = 0;
            }

            // --- input diffusion: independent L / R allpass chains ---
            float inL = pin[0] * inGain, inR = pin[1] * inGain;
            for (int i = 0; i < kAllpass; ++i)          inL = allpass (i, inL);
            for (int i = 0; i < kAllpass; ++i)          inR = allpass (kAllpass + i, inR);

            // --- read + damp each line (fractional, per-line modulated tap) ---
            float d[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                const float mod = modDepth[i] * std::sin (lfoPhase[i]); // bipolar, deep
                d[i] = readFrac (i, (float) lineLen[i] + mod);
                // frequency-split damping: HF loss + a little extra LF sustain.
                lp[i] = d[i] * (1.0f - damp) + lp[i] * damp;
                bp[i] = lp[i] + bassBoost * (bp[i] - lp[i]);   // gentle low shelf
                d[i]  = bp[i];
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] > (float) (2.0 * kPi)) lfoPhase[i] -= (float) (2.0 * kPi);
            }

            // --- Hadamard mix (orthonormal) ---
            float v[kLines];
            hadamard8 (d, v);

            // --- write back: L into even lines, R into odd → stereo field ---
            for (int i = 0; i < kLines; ++i)
            {
                const float inj = (i & 1) ? inR : inL;
                line[i][(size_t) lineIdx[i]] = inj + v[i] * g;
                if (++lineIdx[i] >= (int) line[i].size()) lineIdx[i] = 0;
            }

            // --- output: even lines → L, odd lines → R (decorrelated) ---
            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < kLines; ++i) ((i & 1) ? outR : outL) += d[i];
            const float norm = 0.5f;
            frame[0] += outL * norm * amount;
            if (channels == 2) frame[1] += outR * norm * amount;
        }

    private:
        static constexpr int kLines = 8;
        static constexpr int kAllpass = 6;      // per side
        static constexpr int kModDepth = 80;    // ~1.8 ms @44.1k, per line scaled
        static constexpr double kPi = 3.14159265358979323846;

        float readFrac (int i, float delaySamples) const
        {
            const int   len = (int) line[i].size();
            const float pos = (float) lineIdx[i] - delaySamples;
            float p = pos;
            while (p < 0.0f) p += (float) len;
            while (p >= (float) len) p -= (float) len;
            const int i0 = (int) p;
            const int i1 = (i0 + 1) % len;
            const float f = p - (float) i0;
            return line[i][(size_t) i0] * (1.0f - f) + line[i][(size_t) i1] * f;
        }

        float allpass (int i, float x)
        {
            const float bufv = ap[i][(size_t) apIdx[i]];
            const float y = -x + bufv;
            ap[i][(size_t) apIdx[i]] = x + bufv * 0.5f;
            if (++apIdx[i] >= (int) ap[i].size()) apIdx[i] = 0;
            return y;
        }

        // fast in-place 8-point Walsh-Hadamard, orthonormal (×1/√8).
        static void hadamard8 (const float* in, float* out)
        {
            float a[kLines];
            for (int i = 0; i < kLines; ++i) a[i] = in[i];
            for (int len = 1; len < kLines; len <<= 1)
                for (int i = 0; i < kLines; i += (len << 1))
                    for (int j = i; j < i + len; ++j)
                    {
                        const float u = a[j], v = a[j + len];
                        a[j] = u + v; a[j + len] = u - v;
                    }
            const float s = 0.35355339f; // 1/sqrt(8)
            for (int i = 0; i < kLines; ++i) out[i] = a[i] * s;
        }

        double sr = 44100.0;
        int channels = 2;
        std::array<std::vector<float>, kLines> line;
        std::array<int, kLines>   lineLen { {} };
        std::array<int, kLines>   lineIdx { {} };
        std::array<float, kLines> lp { {} };
        std::array<float, kLines> bp { {} };
        std::array<float, kLines> lfoPhase { {} };
        std::array<float, kLines> lfoInc { {} };
        std::array<float, kLines> modDepth { {} };
        std::array<std::vector<float>, kAllpass * 2> ap;
        std::array<int, kAllpass * 2> apIdx { {} };
        std::array<std::vector<float>, 2> pre;
        std::array<int, 2> preIdx { {} };
        int preCap = 1;
    };
}
