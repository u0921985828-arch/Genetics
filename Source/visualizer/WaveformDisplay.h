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

        void visibilityChanged() override
        {
            if (isShowing()) startTimerHz (60);
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

            // waveform of the recorded window (oldest → newest, left → right)
            const auto& buf = engine.getBuffer();
            const int   pts = juce::jlimit (64, 1024, (int) b.getWidth());
            const double window = juce::jmin ((double) buf.getCapacity() - 32.0,
                                              buf.getSampleRate() * 4.0); // ~2 bars @120
            juce::Path wave;
            for (int i = 0; i < pts; ++i)
            {
                const double frac = (double) i / (double) (pts - 1);   // 0 oldest, 1 newest
                const double delay = (1.0 - frac) * window;
                const float s = juce::jlimit (-1.0f, 1.0f, buf.peekMono (delay));
                const float x = b.getX() + (float) frac * b.getWidth();
                const float y = b.getCentreY() - s * b.getHeight() * 0.46f;
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            g.setColour (greyLight.withAlpha (0.85f));
            g.strokePath (wave, juce::PathStrokeType (1.25f));

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
