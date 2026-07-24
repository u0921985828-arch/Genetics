#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace chrona::dsp
{
    // ========================================================================
    //  Space — an 8-line modulated Feedback Delay Network (FDN) reverb.
    //
    //  Input diffusion (series allpasses) → 8 damped, individually modulated
    //  delay lines mixed through an orthonormal Hadamard matrix. This is a
    //  studio-grade topology: dense, smooth tail, no metallic ringing (thanks
    //  to per-line LFO modulation), decay set by the "Space" macro. RT-safe:
    //  all storage is allocated in prepare().
    // ========================================================================
    class Space
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = numChannels < 2 ? 1 : 2;
            const double scale = sr / 44100.0;

            // prime-ish base lengths (samples @44.1k) for a dense, uncorrelated tail
            static const int baseLen[kLines] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            for (int i = 0; i < kLines; ++i)
            {
                lineLen[i]  = (int) (baseLen[i] * scale);
                const int cap = lineLen[i] + kModDepth + 4;
                line[i].assign ((size_t) cap, 0.0f);
                lp[i] = 0.0f; lineIdx[i] = 0;
                lfoPhase[i] = (float) i * 0.7f;
                lfoInc[i]   = (float) ((0.6 + 0.05 * i) * 2.0 * kPi / sr); // ~0.6..0.95 Hz
            }

            static const int apLen[kAllpass] = { 225, 341, 441, 556 };
            for (int i = 0; i < kAllpass; ++i)
            {
                ap[i].assign ((size_t) std::max (1, (int) (apLen[i] * scale)), 0.0f);
                apIdx[i] = 0;
            }
            reset();
        }

        void reset()
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
            for (auto& a : ap)   std::fill (a.begin(), a.end(), 0.0f);
            lp.fill (0.0f);
        }

        // amount 0..1 → decay + wet send.
        void process (float* frame, float amount)
        {
            if (amount <= 0.0001f) return;

            const float g    = 0.5f + 0.48f * amount;  // feedback / decay
            const float damp = 0.25f;                   // HF damping in the tail
            const float inGain = 0.35f;

            // --- input: mono sum through series allpass diffusion ---
            float in = 0.0f;
            for (int c = 0; c < channels; ++c) in += frame[c];
            in *= (channels == 2 ? 0.5f : 1.0f) * inGain;
            for (int i = 0; i < kAllpass; ++i) in = allpass (i, in);

            // --- read + damp each line (with fractional, modulated tap) ---
            float d[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                const float mod = kModDepth * 0.5f * (1.0f + std::sin (lfoPhase[i]));
                d[i] = readFrac (i, (float) lineLen[i] - mod);
                lp[i] = d[i] * (1.0f - damp) + lp[i] * damp;  // one-pole LP in the loop
                d[i]  = lp[i];
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] > (float) (2.0 * kPi)) lfoPhase[i] -= (float) (2.0 * kPi);
            }

            // --- Hadamard mix (orthonormal) ---
            float v[kLines];
            hadamard8 (d, v);

            // --- write back with input inject + feedback ---
            for (int i = 0; i < kLines; ++i)
            {
                line[i][(size_t) lineIdx[i]] = in + v[i] * g;
                if (++lineIdx[i] >= (int) line[i].size()) lineIdx[i] = 0;
            }

            // --- output: half the lines to L, half to R ---
            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < kLines / 2; ++i)       outL += d[i];
            for (int i = kLines / 2; i < kLines; ++i)  outR += d[i];
            const float norm = 0.5f;
            frame[0] += outL * norm * amount;
            if (channels == 2) frame[1] += outR * norm * amount;
        }

    private:
        static constexpr int kLines = 8;
        static constexpr int kAllpass = 4;
        static constexpr int kModDepth = 12;
        static constexpr double kPi = 3.14159265358979323846;

        float readFrac (int i, float delaySamples) const
        {
            const int   len = (int) line[i].size();
            const float pos = (float) lineIdx[i] - delaySamples;
            float p = pos;
            while (p < 0.0f) p += (float) len;
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
        std::array<float, kLines> lfoPhase { {} };
        std::array<float, kLines> lfoInc { {} };
        std::array<std::vector<float>, kAllpass> ap;
        std::array<int, kAllpass> apIdx { {} };
    };
}
