#pragma once

#include <cmath>
#include <array>

// ============================================================================
//  Interpolation — fractional-index sample reads for the circular buffer.
//
//  Three qualities matched to params::Quality:
//    Linear  — cheapest, some HF loss on pitch shifts
//    Hermite — 4-point cubic, the default; clean and cheap
//    Sinc    — 8-point windowed sinc, highest quality for extreme warps
//
//  All readers take a lambda `at(i)` returning the sample at integer index i,
//  so they are agnostic to the buffer's wrap/layout.
// ============================================================================

namespace chrona::dsp::interp
{
    static constexpr double kPi = 3.14159265358979323846;

    template <typename Fetch>
    inline float linear (Fetch&& at, double pos)
    {
        const auto i0 = (long) std::floor (pos);
        const float f = (float) (pos - (double) i0);
        return at (i0) * (1.0f - f) + at (i0 + 1) * f;
    }

    // 4-point, 3rd-order Hermite (Catmull-Rom style).
    template <typename Fetch>
    inline float hermite (Fetch&& at, double pos)
    {
        const auto i1 = (long) std::floor (pos);
        const float f = (float) (pos - (double) i1);

        const float xm1 = at (i1 - 1);
        const float x0  = at (i1);
        const float x1  = at (i1 + 1);
        const float x2  = at (i1 + 2);

        const float c   = (x1 - xm1) * 0.5f;
        const float v   = x0 - x1;
        const float w   = c + v;
        const float a   = w + v + (x2 - x0) * 0.5f;
        const float b   = w + a;
        return ((a * f - b) * f + c) * f + x0;
    }

    // Precomputed Blackman-windowed sinc, 8 taps, no oversampling table needed
    // at runtime — computed directly. Reserved for the HQ path.
    template <typename Fetch>
    inline float sinc8 (Fetch&& at, double pos)
    {
        const auto center = (long) std::floor (pos);
        const double frac = pos - (double) center;

        constexpr int taps = 8;
        constexpr int half = taps / 2;
        double acc = 0.0, wsum = 0.0;

        for (int k = -half + 1; k <= half; ++k)
        {
            const double x = (double) k - frac;
            double s;
            if (std::abs (x) < 1.0e-7)
                s = 1.0;
            else
            {
                const double px = kPi * x;
                s = std::sin (px) / px;
            }
            // Blackman window across the tap span.
            const double n = (double) (k + half - 1);
            const double N = (double) (taps - 1);
            const double win = 0.42 - 0.5 * std::cos (2.0 * kPi * n / N)
                                    + 0.08 * std::cos (4.0 * kPi * n / N);
            const double w = s * win;
            acc  += (double) at (center + k) * w;
            wsum += w;
        }
        return (float) (wsum != 0.0 ? acc / wsum : acc);
    }
}
