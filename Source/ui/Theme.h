#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace chrona::ui::theme
{
    // ------------------------------------------------------------------------
    //  Palette (from the product brief). Dark, minimal, one electric-blue
    //  accent. No skeuomorphism — flat surfaces, thin strokes, LED indicators.
    // ------------------------------------------------------------------------
    inline const juce::Colour bg0      { 0xff171717 }; // deepest background
    inline const juce::Colour bg1      { 0xff222222 }; // panels
    inline const juce::Colour bg2      { 0xff2e2e2e }; // raised controls / tracks

    inline const juce::Colour accent   { 0xff2f7bff }; // electric blue
    inline const juce::Colour accentDim{ 0xff1c4aa0 };
    inline const juce::Colour white    { 0xfff2f4f8 };
    inline const juce::Colour greyLight{ 0xffb6b9c0 };
    inline const juce::Colour greyMid   { 0xff6c6f77 };
    inline const juce::Colour greyDark  { 0xff3a3c40 };

    inline const juce::Colour led       { 0xff5c96ff };
    inline const juce::Colour ledGlow   { 0x552f7bff };

    // Corner radius scaled by the UI zoom is applied where drawn.
    inline constexpr float cornerRadius = 6.0f;

    inline juce::Font labelFont (float h) { return juce::Font (juce::FontOptions ((float) h, juce::Font::plain)); }
    inline juce::Font valueFont (float h) { return juce::Font (juce::FontOptions ((float) h, juce::Font::bold)); }
}
