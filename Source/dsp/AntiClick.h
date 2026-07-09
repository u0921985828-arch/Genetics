#pragma once

#include <cmath>
#include <algorithm>

namespace chrona::dsp
{
    // ========================================================================
    //  DeclickRamp — an equal-power crossfade applied whenever the read head
    //  jumps discontinuously (stutter slice restart, beat-repeat wrap, tape
    //  re-trigger). Modes call `arm()` on a jump; each processed sample then
    //  crossfades the newly-read value against the last pre-jump value until
    //  the ramp completes.
    // ========================================================================
    class DeclickRamp
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; setTimeMs (3.0); }

        void setTimeMs (double ms)
        {
            lengthSamples = std::max (1, (int) std::round (ms * 0.001 * sr));
        }

        // Call at a discontinuity, passing the last output value pre-jump.
        void arm (float lastValue) { held = lastValue; counter = lengthSamples; }

        // Blend the freshly read value against the held tail.
        inline float process (float freshValue)
        {
            if (counter <= 0 || lengthSamples <= 1) // ~0ms declick == off, no stale sample
                return freshValue;

            const float t = 1.0f - (float) counter / (float) lengthSamples; // 0..1
            // Equal-power crossfade for constant perceived loudness.
            const float a = std::sin (t * 1.5707963f);          // fresh gain
            const float b = std::cos (t * 1.5707963f);          // tail gain
            --counter;
            return freshValue * a + held * b;
        }

        bool active() const noexcept { return counter > 0; }

    private:
        double sr = 44100.0;
        int    lengthSamples = 128;
        int    counter = 0;
        float  held = 0.0f;
    };

    // ========================================================================
    //  SmartFade — window applied to a captured slice so its head/tail taper,
    //  removing edge clicks independent of where the slice boundary falls.
    //  `shape` (0..1) morphs from a short micro-fade to a longer musical fade.
    // ========================================================================
    struct SmartFade
    {
        // gain for sample `i` of a slice of `len` samples, given fade shape.
        static inline float gain (int i, int len, float shape) noexcept
        {
            if (len <= 1) return 1.0f;
            const int fade = std::max (1, (int) ((float) len * (0.02f + 0.23f * shape)));
            float g = 1.0f;
            if (i < fade)                 g = (float) i / (float) fade;
            else if (i > len - fade)      g = (float) (len - i) / (float) fade;
            // smootherstep for a click-free, musical contour
            g = std::clamp (g, 0.0f, 1.0f);
            return g * g * g * (g * (g * 6.0f - 15.0f) + 10.0f);
        }
    };
}
