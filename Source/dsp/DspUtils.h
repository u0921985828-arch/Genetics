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

    // Texture: drive-dependent asymmetric soft saturation. At texture=0 this is
    // transparent; higher values add harmonic grit for the vinyl/tape flavour.
    inline float saturate (float x, float amount)
    {
        if (amount <= 0.0f) return x;
        const float drive = 1.0f + amount * 6.0f;
        const float y = std::tanh (x * drive) / std::tanh (drive);
        return x + amount * (y - x);
    }

    // Simple state-variable-ish tilt filter used by Texture to darken/brighten.
    class TiltFilter
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; reset(); }
        void reset() { lp = 0.0f; }
        // tilt in [-1,1]: negative darkens, positive brightens.
        inline float process (float x, float tilt)
        {
            const float cutoff = 0.15f; // fixed pivot, cheap
            lp += cutoff * (x - lp);
            const float hp = x - lp;
            return lp * (1.0f - tilt) + hp * (1.0f + tilt) + lp * tilt;
        }
    private:
        double sr = 44100.0;
        float  lp = 0.0f;
    };
}
