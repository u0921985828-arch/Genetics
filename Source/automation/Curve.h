#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

namespace chrona::automation
{
    // ========================================================================
    //  Curve — an ordered set of breakpoints over the normalised pattern
    //  phase [0,1]. Each point has an x (phase), y (value) and a per-segment
    //  tension/curvature. Shared shape for both the Time curve and the Volume
    //  curve; the DSP samples value(phase) once per audio sample.
    //
    //  Thread model: the editor (message thread) mutates a Curve and publishes
    //  an immutable snapshot; the audio thread only ever reads a snapshot. See
    //  AutomationEngine for the lock-free hand-off.
    // ========================================================================
    struct Point
    {
        float x = 0.0f;   // phase 0..1
        float y = 0.0f;   // value 0..1
        float curve = 0.0f; // -1..1 segment shaping toward the NEXT point
    };

    class Curve
    {
    public:
        Curve() = default;

        void clear() { points.clear(); }

        void setLinearRamp (float y0, float y1)
        {
            points = { { 0.0f, y0, 0.0f }, { 1.0f, y1, 0.0f } };
        }

        void setFlat (float y)
        {
            points = { { 0.0f, y, 0.0f }, { 1.0f, y, 0.0f } };
        }

        void addPoint (Point p)
        {
            p.x = juce::jlimit (0.0f, 1.0f, p.x);
            p.y = juce::jlimit (0.0f, 1.0f, p.y);
            points.push_back (p);
            sort();
        }

        void setPoints (std::vector<Point> pts) { points = std::move (pts); sort(); }
        const std::vector<Point>& getPoints() const { return points; }
        std::vector<Point>& getPointsMutable() { return points; }

        int   size()  const { return (int) points.size(); }
        bool  empty() const { return points.empty(); }

        // Sample the curve at normalised phase, with segment curvature applied.
        float value (float phase) const
        {
            if (points.empty()) return 0.0f;
            if (points.size() == 1) return points.front().y;

            phase = juce::jlimit (0.0f, 1.0f, phase);

            if (phase <= points.front().x) return points.front().y;
            if (phase >= points.back().x)  return points.back().y;

            // binary search for the segment containing phase
            int lo = 0, hi = (int) points.size() - 1;
            while (hi - lo > 1)
            {
                const int mid = (lo + hi) / 2;
                if (points[(size_t) mid].x <= phase) lo = mid; else hi = mid;
            }

            const auto& a = points[(size_t) lo];
            const auto& b = points[(size_t) hi];
            const float span = b.x - a.x;
            float t = span > 1.0e-9f ? (phase - a.x) / span : 0.0f;

            // segment curvature: warp t before the linear blend
            t = applyCurve (t, a.curve);
            return a.y + (b.y - a.y) * t;
        }

        // Grid snap for the editor. `divisions` is the number of vertical grid
        // lines; snaps x. y snapping is optional (steps).
        static float snapPhase (float x, int divisions)
        {
            if (divisions <= 0) return x;
            const float step = 1.0f / (float) divisions;
            return juce::jlimit (0.0f, 1.0f, std::round (x / step) * step);
        }

        juce::ValueTree toValueTree (const juce::Identifier& tag) const
        {
            juce::ValueTree vt (tag);
            for (const auto& p : points)
            {
                juce::ValueTree pt ("p");
                pt.setProperty ("x", p.x, nullptr);
                pt.setProperty ("y", p.y, nullptr);
                pt.setProperty ("c", p.curve, nullptr);
                vt.appendChild (pt, nullptr);
            }
            return vt;
        }

        void fromValueTree (const juce::ValueTree& vt)
        {
            points.clear();
            for (int i = 0; i < vt.getNumChildren(); ++i)
            {
                const auto pt = vt.getChild (i);
                points.push_back ({ (float) pt.getProperty ("x"),
                                    (float) pt.getProperty ("y"),
                                    (float) pt.getProperty ("c") });
            }
            sort();
            if (points.empty()) setFlat (0.0f);
        }

    private:
        static float applyCurve (float t, float curve)
        {
            if (std::abs (curve) < 1.0e-4f) return t;
            // exponential-ish warp; curve<0 ease-in, curve>0 ease-out
            const float k = curve * 4.0f;
            return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
        }

        void sort()
        {
            std::sort (points.begin(), points.end(),
                       [] (const Point& l, const Point& r) { return l.x < r.x; });
        }

        std::vector<Point> points { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    };
}
