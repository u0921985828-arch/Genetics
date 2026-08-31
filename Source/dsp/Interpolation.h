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

    // Blackman-windowed sinc, 8 taps, computed directly (no runtime table).
    //
    //  `readRate` is the instantaneous source-read speed: when a mode reads the
    //  buffer faster than 1× (pitch up), the naive kernel's stopband sits at the
    //  source Nyquist and everything above output-Nyquist/readRate folds back as
    //  aliasing. Scaling the sinc argument by cutoff = min(1, 1/readRate) turns
    //  the kernel into a decimation low-pass whose cutoff tracks the read rate,
    //  so 2× reads are band-limited to Nyquist/2 before resampling. At
    //  readRate ≤ 1 (cutoff 1) this is the exact original windowed sinc.
    template <typename Fetch>
    inline float sinc8 (Fetch&& at, double pos, double readRate = 1.0)
    {
        const auto center = (long) std::floor (pos);
        const double frac = pos - (double) center;
        const double cutoff = readRate > 1.0 ? 1.0 / readRate : 1.0;

        constexpr int taps = 8;
        constexpr int half = taps / 2;
        double acc = 0.0, wsum = 0.0;

        for (int k = -half + 1; k <= half; ++k)
        {
            const double x = (double) k - frac;
            const double t = cutoff * x;             // cutoff-scaled sinc argument
            double s;
            if (std::abs (t) < 1.0e-7)
                s = 1.0;
            else
            {
                const double pt = kPi * t;
                s = std::sin (pt) / pt;
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
