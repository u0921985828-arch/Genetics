#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BigKnob.h"
#include "ModeSelector.h"
#include "Theme.h"

namespace chrona::ui
{
    // ========================================================================
    //  Level 1 (Simple) — six macro knobs + the mode grid. This is the whole
    //  "musical result in <5s" surface: pick a mode, turn a knob, done.
    // ========================================================================
    class Level1Panel : public juce::Component
    {
    public:
        explicit Level1Panel (juce::AudioProcessorValueTreeState& s)
            : modeSelector (s)
        {
            namespace P = params::id;
            knobs.add (new BigKnob (s, P::time,    "TIME"));
            knobs.add (new BigKnob (s, P::depth,   "DEPTH"));
            knobs.add (new BigKnob (s, P::mix,     "MIX"));
            knobs.add (new BigKnob (s, P::texture, "TEXTURE"));
            knobs.add (new BigKnob (s, P::space,   "SPACE"));
            knobs.add (new BigKnob (s, P::width,   "WIDTH"));
            for (auto* k : knobs) addAndMakeVisible (k);

            addAndMakeVisible (modeSelector);
        }

        void paint (juce::Graphics& g) override { g.fillAll (theme::bg0); }

        void resized() override
        {
            auto b = getLocalBounds().reduced (12);
            auto modeArea = b.removeFromBottom (juce::jmax (90, b.getHeight() / 3));
            modeSelector.setBounds (modeArea.reduced (0, 6));

            // six knobs in a responsive row (wraps to 3×2 when narrow)
            const int n = knobs.size();
            const bool wide = b.getWidth() > 640;
            const int cols = wide ? 6 : 3;
            const int rows = (n + cols - 1) / cols;
            const int cw = b.getWidth() / cols;
            const int ch = b.getHeight() / rows;
            for (int i = 0; i < n; ++i)
            {
                const int r = i / cols, c = i % cols;
                knobs[i]->setBounds (juce::Rectangle<int> (b.getX() + c * cw, b.getY() + r * ch, cw, ch).reduced (6));
            }
        }

    private:
        juce::OwnedArray<BigKnob> knobs;
        ModeSelector modeSelector;
    };
}
