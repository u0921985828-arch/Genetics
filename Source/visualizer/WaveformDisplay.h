#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/TemporalEngine.h"
#include "../ui/Theme.h"

namespace chrona::visualizer
{
    // ========================================================================
    //  WaveformDisplay — real-time view of the circular buffer contents, the
    //  read/playback position and the amount of "time behind live" currently
    //  in play. Refreshes at 60 FPS, and pauses itself when not showing (see
    //  visibilityChanged) to save CPU.
    // ========================================================================
    class WaveformDisplay : public juce::Component,
                            private juce::Timer
    {
    public:
        explicit WaveformDisplay (const dsp::TemporalEngine& e) : engine (e)
        {
            setOpaque (true);
        }

        // Drive the 60 Hz timer from BOTH visibility and hierarchy changes:
        // becoming visible via an ANCESTOR (e.g. the ADVANCED panel opening)
        // sends parentHierarchyChanged, not visibilityChanged — without this the
        // waveform never starts animating and shows a frozen frame.
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
            g.fillAll (bg0);

            auto b = getLocalBounds().toFloat().reduced (6.0f);

            // grid
            g.setColour (greyDark.withAlpha (0.5f));
            for (int i = 1; i < 8; ++i)
            {
                const float x = b.getX() + b.getWidth() * (float) i / 8.0f;
                g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            }
            g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());

            // Peak-envelope of the recorded window (oldest → newest, left →
            // right), read from the engine's lock-free bin snapshot — no data
            // race on the audio buffer. The oldest bin is the one about to be
            // overwritten (visWriteBin).
            const int   bins  = engine.getVisBinCount();
            const int   start = engine.getVisWriteBin();
            const int   pts   = juce::jlimit (64, bins, (int) b.getWidth());
            juce::Path top, bot;
            for (int i = 0; i < pts; ++i)
            {
                const double frac = (double) i / (double) (pts - 1);        // 0 oldest, 1 newest
                const int    bin  = (start + (int) (frac * (double) (bins - 1))) % bins;
                const float  peak = juce::jlimit (0.0f, 1.0f, engine.getVisBin (bin));
                const float  x    = b.getX() + (float) frac * b.getWidth();
                const float  h    = peak * b.getHeight() * 0.46f;
                if (i == 0) { top.startNewSubPath (x, b.getCentreY() - h); bot.startNewSubPath (x, b.getCentreY() + h); }
                else        { top.lineTo (x, b.getCentreY() - h);          bot.lineTo (x, b.getCentreY() + h); }
            }
            g.setColour (greyLight.withAlpha (0.85f));
            g.strokePath (top, juce::PathStrokeType (1.25f));
            g.strokePath (bot, juce::PathStrokeType (1.25f));

            // playback position marker (read delay maps to x)
            const double delayNorm = juce::jlimit (0.0, 1.0, engine.getReadDelayNorm());
            const float px = b.getRight() - (float) delayNorm * b.getWidth();
            g.setColour (accent);
            g.drawVerticalLine ((int) px, b.getY(), b.getBottom());
            g.setColour (ledGlow);
            g.fillRect (juce::Rectangle<float> (px - 2.0f, b.getY(), 4.0f, b.getHeight()));

            // "live" edge label
            g.setColour (greyMid);
            g.setFont (labelFont (11.0f));
            g.drawText ("LIVE", juce::Rectangle<float> (b.getRight() - 44, b.getY(), 40, 14),
                        juce::Justification::right);
            g.drawText ("-2 BARS", juce::Rectangle<float> (b.getX(), b.getY(), 60, 14),
                        juce::Justification::left);
        }

    private:
        void timerCallback() override { repaint(); }
        const dsp::TemporalEngine& engine;
    };
}
