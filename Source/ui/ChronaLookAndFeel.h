#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace chrona::ui
{
    // ========================================================================
    //  ChronaLookAndFeel — the single visual language for the plugin. Large
    //  rotary knobs with an LED value arc, flat combo boxes and buttons, and
    //  the dark three-tone palette. All drawing is vector + resolution
    //  independent so the UI stays crisp from 100% to 300% on HiDPI.
    // ========================================================================
    class ChronaLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ChronaLookAndFeel()
        {
            using namespace theme;
            setColour (juce::ResizableWindow::backgroundColourId, bg0);
            setColour (juce::Slider::rotarySliderFillColourId, accent);
            setColour (juce::Slider::textBoxTextColourId, white);
            setColour (juce::Slider::textBoxBackgroundColourId, bg1);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId, greyLight);
            setColour (juce::ComboBox::backgroundColourId, bg2);
            setColour (juce::ComboBox::textColourId, white);
            setColour (juce::ComboBox::outlineColourId, greyDark);
            setColour (juce::ComboBox::arrowColourId, accent);
            setColour (juce::PopupMenu::backgroundColourId, bg1);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
            setColour (juce::PopupMenu::textColourId, white);
            setColour (juce::TextButton::buttonColourId, bg2);
            setColour (juce::TextButton::buttonOnColourId, accentDim);
            setColour (juce::TextButton::textColourOnId, white);
            setColour (juce::TextButton::textColourOffId, greyLight);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float pos, float startAngle, float endAngle,
                               juce::Slider& slider) override
        {
            using namespace theme;
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const float track = juce::jmax (2.0f, radius * 0.14f);
            const float angle = startAngle + pos * (endAngle - startAngle);

            // recessed body
            g.setColour (bg2);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            g.setColour (greyDark);
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);

            const float arcR = radius - track * 0.5f - 2.0f;

            // background arc
            juce::Path bgArc;
            bgArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour (greyDark);
            g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            // value arc (LED)
            juce::Path valArc;
            valArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour (ledGlow);
            g.strokePath (valArc, juce::PathStrokeType (track + 3.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour (led);
            g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // pointer
            juce::Path ptr;
            const float ptrLen = radius * 0.62f;
            ptr.startNewSubPath (0.0f, -radius * 0.18f);
            ptr.lineTo (0.0f, -ptrLen);
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
            g.setColour (white);
            g.strokePath (ptr, juce::PathStrokeType (juce::jmax (2.0f, radius * 0.08f),
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.restoreState();

            // centre LED dot
            g.setColour (slider.isMouseOverOrDragging() ? accent : greyMid);
            const float dot = radius * 0.14f;
            g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre (centre));
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox& box) override
        {
            using namespace theme;
            auto b = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
            g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (b, cornerRadius);
            g.setColour (box.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (b, cornerRadius, 1.0f);

            juce::Path arrow;
            const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
            arrow.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
            g.setColour (accent);
            g.fillPath (arrow);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override
        {
            using namespace theme;
            auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
            auto c = backgroundColour;
            if (shouldDrawButtonAsDown)          c = accentDim;
            else if (shouldDrawButtonAsHighlighted) c = c.brighter (0.15f);

            g.setColour (c);
            g.fillRoundedRectangle (bounds, cornerRadius);

            if (b.getToggleState())
            {
                g.setColour (accent);
                g.drawRoundedRectangle (bounds, cornerRadius, 1.5f);
            }
            else
            {
                g.setColour (greyDark);
                g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
            }
        }
    };
}
