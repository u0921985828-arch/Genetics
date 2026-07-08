#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace chrona::ui
{
    // ========================================================================
    //  BigKnob — a large rotary + caption + live value readout, bound to an
    //  APVTS parameter. Touch friendly (large hit area, velocity-based drag).
    // ========================================================================
    class BigKnob : public juce::Component
    {
    public:
        BigKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                 const juce::String& caption)
            : name (caption)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setVelocityBasedMode (true);
            slider.setVelocityModeParameters (0.7, 1, 0.09, false);
            slider.setDoubleClickReturnValue (true, state.getParameter (paramID) != nullptr
                                                    ? state.getParameter (paramID)->getDefaultValue() : 0.5);
            slider.setWantsKeyboardFocus (false);
            addAndMakeVisible (slider);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                             (state, paramID, slider);

            ca.setText (caption, juce::dontSendNotification);
            ca.setJustificationType (juce::Justification::centred);
            ca.setColour (juce::Label::textColourId, theme::greyLight);
            addAndMakeVisible (ca );

            value.setJustificationType (juce::Justification::centred);
            value.setColour (juce::Label::textColourId, theme::white);
            addAndMakeVisible (value);

            slider.onValueChange = [this] { updateValueText(); };
            updateValueText();
        }

        void setValueSuffix (const juce::String& s) { suffix = s; updateValueText(); }

        void resized() override
        {
            auto b = getLocalBounds();
            const int capH = juce::jmax (14, b.getHeight() / 8);
            ca .setBounds (b.removeFromTop (capH));
            value.setBounds (b.removeFromBottom (capH));
            slider.setBounds (b.reduced (2));

            const float fh = juce::jlimit (11.0f, 22.0f, (float) capH * 0.8f);
            ca .setFont (theme::labelFont (fh));
            value.setFont (theme::valueFont (fh));
        }

        juce::Slider& getSlider() { return slider; }

    private:
        void updateValueText()
        {
            value.setText (juce::String (slider.getValue(), 2) + suffix, juce::dontSendNotification);
        }

        juce::String name, suffix;
        juce::Slider slider;
        juce::Label  ca, value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
}
