#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "Theme.h"
#include "../params/Parameters.h"

namespace chrona::ui
{
    // ========================================================================
    //  ModeSelector — a grid of large, touch-sized buttons, one per mode. The
    //  active mode glows in the accent colour. Bound to the "mode" choice
    //  parameter so host automation and the buttons stay in sync.
    // ========================================================================
    class ModeSelector : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        explicit ModeSelector (juce::AudioProcessorValueTreeState& s) : state (s)
        {
            const auto names = params::modeNames();
            for (int i = 0; i < names.size(); ++i)
            {
                auto* b = buttons.add (new juce::TextButton (names[i]));
                b->setClickingTogglesState (false);
                b->setRadioGroupId (1001);
                b->onClick = [this, i] { selectMode (i); };
                addAndMakeVisible (b);
            }
            state.addParameterListener (params::id::mode, this);
            syncFromParam();
        }

        ~ModeSelector() override { state.removeParameterListener (params::id::mode, this); }

        void resized() override
        {
            // square-ish grid so the mode count always lays out evenly
            // (9 modes → a clean 3×3).
            auto b = getLocalBounds().reduced (2);
            const int cols = juce::jmax (1, (int) std::ceil (std::sqrt ((double) buttons.size())));
            const int rows = (buttons.size() + cols - 1) / cols;
            const int cw = b.getWidth() / cols;
            const int ch = b.getHeight() / juce::jmax (1, rows);
            for (int i = 0; i < buttons.size(); ++i)
            {
                const int r = i / cols, c = i % cols;
                buttons[i]->setBounds (juce::Rectangle<int> (b.getX() + c * cw, b.getY() + r * ch, cw, ch).reduced (3));
            }
        }

    private:
        void selectMode (int index)
        {
            if (auto* p = state.getParameter (params::id::mode))
            {
                const float norm = p->convertTo0to1 ((float) index);
                p->setValueNotifyingHost (norm);
            }
        }

        void parameterChanged (const juce::String&, float) override
        {
            // SafePointer so a queued callback can't run on a destroyed editor
            // (removeParameterListener stops new events but can't unqueue this).
            juce::Component::SafePointer<ModeSelector> safe (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->syncFromParam(); });
        }

        void syncFromParam()
        {
            int idx = 0;
            if (auto* p = state.getParameter (params::id::mode))
                idx = (int) p->convertFrom0to1 (p->getValue());
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
        }

        juce::AudioProcessorValueTreeState& state;
        juce::OwnedArray<juce::TextButton> buttons;
    };
}
