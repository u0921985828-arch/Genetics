#pragma once

#include <cmath>
#include <algorithm>

namespace chrona::dsp
{
    // One-pole smoother for click-free parameter changes.
    class OnePole
    {
    public:
        void prepare (double sr, double ms = 20.0) { setTime (sr, ms); }
        void setTime (double sr, double ms)
        {
            const double tc = std::max (0.001, ms) * 0.001;
            a = std::exp (-1.0 / (tc * sr));
        }
        inline float process (float x) { y = (float) (x + a * (y - x)); return y; }
        void  reset (float v = 0.0f) { y = v; }
        float value() const { return y; }
    private:
        double a = 0.0;
        float  y = 0.0f;
    };

    // Peak/RMS envelope follower with separate attack/release — used by the
    // ducker and the internal sidechain.
    class EnvelopeFollower
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; setTimes (5.0, 120.0); }
        void setTimes (double attackMs, double releaseMs)
        {
            atk = std::exp (-1.0 / (std::max (0.01, attackMs)  * 0.001 * sr));
            rel = std::exp (-1.0 / (std::max (0.01, releaseMs) * 0.001 * sr));
        }
        inline float process (float x)
        {
            const float r = std::abs (x);
            const float c = (r > env) ? atk : rel;
            env = r + c * (env - r);
            return env;
        }
        void reset() { env = 0.0f; }
        float value() const { return env; }
    private:
        double sr = 44100.0, atk = 0.0, rel = 0.0;
        float  env = 0.0f;
    };

    // Texture saturation with first-order antiderivative anti-aliasing (ADAA,
    // Parker/Zavalishin): the tanh nonlinearity is evaluated through its
    // antiderivative so most of the harmonic aliasing is removed WITHOUT
    // oversampling (O(1)/sample, no sample-rate change). Stateful (needs the
    // previous input), so one instance per channel.
    class ADAASaturator
    {
    public:
        void reset() { x1 = 0.0f; dcX1 = 0.0f; dcY1 = 0.0f; }
        inline float process (float x, float amount)
        {
            if (amount <= 0.0f) { x1 = x; dcX1 = x; dcY1 = x; return x; }
            const float a = 1.0f + amount * 6.0f;      // drive
            const float b = 0.15f * amount;            // asymmetry bias → even harmonics (tube-like)

            const float d = x - x1;
            float y;
            if (std::abs (d) > 1.0e-4f) y = (antideriv (x, a, b) - antideriv (x1, a, b)) / d;
            else                        y = shape (0.5f * (x + x1), a, b);
            x1 = x;

            const float sat = x + amount * (y - x);
            // DC blocker — the asymmetry bias adds a small offset; remove it.
            const float out = sat - dcX1 + 0.9995f * dcY1;
            dcX1 = sat; dcY1 = out;
            return out;
        }
    private:
        // asymmetric soft clip: g(x)=tanh(a·(x+b))/tanh(a) ; G=∫g=ln(cosh(a·(x+b)))/(a·tanh(a))
        static inline float shape (float x, float a, float b)     { return std::tanh (a * (x + b)) / std::tanh (a); }
        static inline float antideriv (float x, float a, float b) { return std::log (std::cosh (a * (x + b))) / (a * std::tanh (a)); }
        float x1 = 0.0f, dcX1 = 0.0f, dcY1 = 0.0f;
    };

    // Gentle tilt filter used by Texture to darken/brighten around a fixed
    // ~700 Hz pivot. The pivot is sample-rate-independent (was a raw 0.15
    // coefficient that drifted up in frequency at higher SR), and the tilt gain
    // is halved and capped so brightening can't add a harsh, unbounded top.
    class TiltFilter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            constexpr double kPivotHz = 700.0, kTwoPi = 6.283185307179586;
            coeff = (float) (1.0 - std::exp (-kTwoPi * kPivotHz / std::max (8000.0, sr)));
            reset();
        }
        void reset() { lp = 0.0f; }
        // tilt in [-1,1]: negative darkens (boosts lows), positive brightens.
        inline float process (float x, float tilt)
        {
            const float g = std::clamp (tilt * 0.5f, -0.5f, 0.5f); // tamed
            lp += coeff * (x - lp);
            const float hp = x - lp;
            return lp * (1.0f - g) + hp * (1.0f + g);
        }
    private:
        double sr = 44100.0;
        float  coeff = 0.1f;
        float  lp = 0.0f;
    };
}
