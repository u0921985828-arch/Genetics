#pragma once

#include <array>
#include <vector>
#include <cmath>

namespace chrona::dsp
{
    // ========================================================================
    //  Space — a compact Schroeder/Freeverb-style stereo ambience driving the
    //  "Space" macro. Deliberately small (4 combs + 2 allpass per channel) so
    //  it adds a sense of room without pulling focus from the time effect.
    // ========================================================================
    class Space
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = numChannels < 2 ? 1 : 2;

            static const int combTune[kNumCombs]   = { 1116, 1188, 1277, 1356 };
            static const int apTune  [kNumAllpass]  = { 556, 441 };
            const double scale = sr / 44100.0;
            const int stereoSpread = 23;

            for (int ch = 0; ch < channels; ++ch)
            {
                for (int i = 0; i < kNumCombs; ++i)
                {
                    const int len = (int) ((combTune[i] + (ch == 1 ? stereoSpread : 0)) * scale);
                    combs[(size_t) ch][(size_t) i].resize (len);
                }
                for (int i = 0; i < kNumAllpass; ++i)
                {
                    const int len = (int) ((apTune[i] + (ch == 1 ? stereoSpread : 0)) * scale);
                    allpass[(size_t) ch][(size_t) i].resize (len);
                }
            }
            reset();
        }

        void reset()
        {
            for (auto& ch : combs)   for (auto& c : ch) c.clear();
            for (auto& ch : allpass) for (auto& a : ch) a.clear();
        }

        // amount 0..1 drives both send level and tail length.
        void process (float* frame, float amount)
        {
            if (amount <= 0.0001f) return;

            const float feedback = 0.7f + 0.28f * amount;
            const float damp     = 0.2f;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float in = frame[ch] * 0.015f;
                float out = 0.0f;
                for (int i = 0; i < kNumCombs; ++i)
                    out += combs[(size_t) ch][(size_t) i].process (in, feedback, damp);
                for (int i = 0; i < kNumAllpass; ++i)
                    out = allpass[(size_t) ch][(size_t) i].process (out);

                frame[ch] += out * amount;
            }
        }

    private:
        struct Comb
        {
            void resize (int n) { buf.assign ((size_t) std::max (1, n), 0.0f); idx = 0; store = 0.0f; }
            void clear() { std::fill (buf.begin(), buf.end(), 0.0f); store = 0.0f; }
            inline float process (float in, float fb, float damp)
            {
                float y = buf[(size_t) idx];
                store = y * (1.0f - damp) + store * damp;
                buf[(size_t) idx] = in + store * fb;
                if (++idx >= (int) buf.size()) idx = 0;
                return y;
            }
            std::vector<float> buf; int idx = 0; float store = 0.0f;
        };

        struct Allpass
        {
            void resize (int n) { buf.assign ((size_t) std::max (1, n), 0.0f); idx = 0; }
            void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
            inline float process (float in)
            {
                float y = buf[(size_t) idx];
                float out = -in + y;
                buf[(size_t) idx] = in + y * 0.5f;
                if (++idx >= (int) buf.size()) idx = 0;
                return out;
            }
            std::vector<float> buf; int idx = 0;
        };

        static constexpr int kNumCombs = 4;
        static constexpr int kNumAllpass = 2;

        double sr = 44100.0;
        int channels = 2;
        std::array<std::array<Comb, kNumCombs>, 2> combs;
        std::array<std::array<Allpass, kNumAllpass>, 2> allpass;
    };
}
