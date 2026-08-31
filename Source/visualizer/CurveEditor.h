#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../automation/AutomationEngine.h"
#include "../dsp/TemporalEngine.h"
#include "../ui/Theme.h"

namespace chrona::visualizer
{
    // ========================================================================
    //  CurveEditor — the Level-2 breakpoint editor used for both the Time and
    //  Volume curves. Click empty space to add a point, drag to move, double
    //  click a point to delete. Grid snap follows the Snap parameter. Edits are
    //  published lock-free to the AutomationEngine; a live playhead tracks the
    //  pattern phase.
    // ========================================================================
    class CurveEditor : public juce::Component,
                        private juce::Timer
    {
    public:
        enum class Target { Time, Volume };

        CurveEditor (automation::AutomationEngine& a, const dsp::TemporalEngine& e, Target t)
            : automation (a), engine (e), target (t)
        {
            curve = (target == Target::Time) ? automation.snapshotTimeCurve()
                                             : automation.snapshotVolumeCurve();
        }

        void setGrid (int divs, bool snapOn) { gridDivs = juce::jmax (1, divs); snap = snapOn; }

        // Reseed the editor from the engine (e.g. after preset load).
        void refreshFromEngine()
        {
            curve = (target == Target::Time) ? automation.snapshotTimeCurve()
                                             : automation.snapshotVolumeCurve();
            repaint();
        }

        // Also react to hierarchy changes so the playhead animates when the
        // ADVANCED panel (an ancestor) opens — visibilityChanged alone misses it.
        void visibilityChanged()      override { updateTimerState(); }
        void parentHierarchyChanged() override { updateTimerState(); }
        void updateTimerState()
        {
            if (isShowing()) { if (! isTimerRunning()) startTimerHz (60); }
            else             stopTimer();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace ui::theme;
            auto b = getLocalBounds().toFloat().reduced (6.0f);
            g.setColour (bg0);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), cornerRadius);

            // grid
            g.setColour (greyDark.withAlpha (0.6f));
            for (int i = 0; i <= gridDivs; ++i)
            {
                const float x = b.getX() + b.getWidth() * (float) i / (float) gridDivs;
                g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            }
            for (int i = 0; i <= 4; ++i)
            {
                const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
                g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
            }

            // curve line
            juce::Path p;
            const int steps = juce::jmax (32, (int) b.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const float ph = (float) i / (float) steps;
                const float v = curve.value (ph);
                const auto pt = toScreen (ph, v, b);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour (accent);
            g.strokePath (p, juce::PathStrokeType (2.0f));

            // fill under curve for readability
            juce::Path fill = p;
            fill.lineTo (b.getRight(), b.getBottom());
            fill.lineTo (b.getX(), b.getBottom());
            fill.closeSubPath();
            g.setColour (accent.withAlpha (0.10f));
            g.fillPath (fill);

            // points
            for (int i = 0; i < curve.size(); ++i)
            {
                const auto& pt = curve.getPoints()[(size_t) i];
                const auto s = toScreen (pt.x, pt.y, b);
                g.setColour (i == dragIndex ? white : led);
                g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (s));
                g.setColour (bg0);
                g.drawEllipse (juce::Rectangle<float> (9, 9).withCentre (s), 1.5f);
            }

            // playhead
            const float ph = (float) engine.getPlayheadPhase();
            const float px = b.getX() + ph * b.getWidth();
            g.setColour (white.withAlpha (0.7f));
            g.drawVerticalLine ((int) px, b.getY(), b.getBottom());
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto b = getLocalBounds().toFloat().reduced (6.0f);
            dragIndex = findPoint (e.position, b);

            if (e.mods.isRightButtonDown() || e.getNumberOfClicks() >= 2)
            {
                if (dragIndex >= 0 && curve.size() > 2)
                {
                    auto pts = curve.getPoints();
                    pts.erase (pts.begin() + dragIndex);
                    curve.setPoints (pts);
                    dragIndex = -1;
                    publish();
                }
                return;
            }

            if (dragIndex < 0)
            {
                auto sp = fromScreen (e.position, b);
                if (snap) sp.x = automation::Curve::snapPhase (sp.x, gridDivs);
                curve.addPoint ({ sp.x, sp.y, 0.0f });
                dragIndex = findPoint (toScreen (sp.x, sp.y, b), b);
                publish();
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragIndex < 0) return;
            auto b = getLocalBounds().toFloat().reduced (6.0f);
            auto sp = fromScreen (e.position, b);
            if (snap) sp.x = automation::Curve::snapPhase (sp.x, gridDivs);

            auto pts = curve.getPoints();
            // keep endpoints pinned to x=0 / x=1
            if (dragIndex == 0)                     sp.x = 0.0f;
            else if (dragIndex == curve.size() - 1) sp.x = 1.0f;
            pts[(size_t) dragIndex].x = juce::jlimit (0.0f, 1.0f, sp.x);
            pts[(size_t) dragIndex].y = juce::jlimit (0.0f, 1.0f, sp.y);
            curve.setPoints (pts);
            dragIndex = findPoint (toScreen (pts[(size_t) dragIndex].x, pts[(size_t) dragIndex].y, b), b);
            publish();
        }

        void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; repaint(); }

    private:
        void timerCallback() override { repaint(); }

        juce::Point<float> toScreen (float ph, float v, juce::Rectangle<float> b) const
        {
            // Time curve: y=0 (live) at top → invert. Volume: y=1 at top.
            const float yv = (target == Target::Time) ? v : (1.0f - v);
            return { b.getX() + ph * b.getWidth(), b.getY() + yv * b.getHeight() };
        }

        juce::Point<float> fromScreen (juce::Point<float> s, juce::Rectangle<float> b) const
        {
            const float ph = juce::jlimit (0.0f, 1.0f, (s.x - b.getX()) / b.getWidth());
            float yv = juce::jlimit (0.0f, 1.0f, (s.y - b.getY()) / b.getHeight());
            const float v = (target == Target::Time) ? yv : (1.0f - yv);
            return { ph, v };
        }

        int findPoint (juce::Point<float> s, juce::Rectangle<float> b) const
        {
            for (int i = 0; i < curve.size(); ++i)
            {
                const auto& pt = curve.getPoints()[(size_t) i];
                if (toScreen (pt.x, pt.y, b).getDistanceFrom (s) < 12.0f) return i;
            }
            return -1;
        }

        void publish()
        {
            if (target == Target::Time) automation.publishTimeCurve (curve);
            else                        automation.publishVolumeCurve (curve);
            repaint();
        }

        automation::AutomationEngine& automation;
        const dsp::TemporalEngine& engine;
        Target target;
        automation::Curve curve;
        int gridDivs = 8;
        bool snap = true;
        int dragIndex = -1;
    };
}
