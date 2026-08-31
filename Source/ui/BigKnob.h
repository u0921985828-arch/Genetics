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
            if (auto* param = state.getParameter (paramID))
                slider.setDoubleClickReturnValue (true,
                    (double) param->convertFrom0to1 (param->getDefaultValue()));
            slider.setWantsKeyboardFocus (true);
            slider.setTitle (caption);                 // accessible name (screen readers)
            slider.setHelpText (caption + " control");
            setTitle (caption);
            addAndMakeVisible (slider);

            // Auto-pick a readout style from the parameter's range: a 0..1 (or
            // ±1) control reads as a percentage like the reference plugins;
            // anything wider reads as a plain value plus an optional unit.
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (paramID)))
            {
                const auto r = rp->getNormalisableRange();
                if (r.end <= 1.0001f && r.start >= -1.0001f && (r.end - r.start) <= 2.0001f)
                {
                    percentMode = true;
                    signedPercent = (r.start < -0.0001f); // e.g. Swing ±50%
                }
            }

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
            const double v = slider.getValue();
            juce::String txt;
            if (percentMode && suffix.isEmpty())
            {
                const int pct = juce::roundToInt (v * 100.0);
                txt = (signedPercent && pct > 0 ? "+" : "") + juce::String (pct) + "%";
            }
            else
            {
                const int dp = std::abs (v) >= 10.0 ? 1 : 2;
                txt = juce::String (v, dp) + suffix;
            }
            value.setText (txt, juce::dontSendNotification);
        }

        bool percentMode = false;
        bool signedPercent = false;
        juce::String name, suffix;
        juce::Slider slider;
        juce::Label  ca, value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
}
