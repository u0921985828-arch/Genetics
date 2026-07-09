#include "Parameters.h"

namespace chrona::params
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using APInt = juce::AudioParameterInt;
    using Range = juce::NormalisableRange<float>;

    static Range pct()            { return Range (0.0f, 1.0f, 0.0001f); }
    static Range ms (float lo, float hi)
    {
        Range r (lo, hi);
        r.setSkewForCentre (juce::jlimit (lo + 1.0f, hi - 1.0f, (lo + hi) * 0.25f));
        return r;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // ---- Level 1 macros ------------------------------------------------
        layout.add (std::make_unique<APF> (juce::ParameterID { id::time,    1 }, "Time",    pct(), 0.5f));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::depth,   1 }, "Depth",   pct(), 1.0f));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::mix,     1 }, "Mix",     pct(), 1.0f));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::texture, 1 }, "Texture", pct(), 0.0f));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::space,   1 }, "Space",   pct(), 0.0f));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::width,   1 }, "Width",   pct(), 0.5f));

        layout.add (std::make_unique<APC> (juce::ParameterID { id::mode, 1 }, "Mode",
                                           modeNames(), 0));

        // ---- Level 2 engine ------------------------------------------------
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::sync, 1 }, "Sync",
                                             syncDivisions(), 2));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::bufferBars, 1 }, "Buffer",
                                             juce::StringArray { "1 Bar", "2 Bars" }, 1));
        layout.add (std::make_unique<APB>   (juce::ParameterID { id::snap, 1 }, "Snap", true));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::quality, 1 }, "Quality",
                                             juce::StringArray { "Linear", "Hermite", "Sinc (HQ)" }, 1));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::antiClick, 1 }, "Anti-Click",
                                             ms (0.5f, 25.0f), 3.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::swing, 1 }, "Swing",
                                             Range (-0.5f, 0.5f, 0.0001f), 0.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::humanize, 1 }, "Humanize", pct(), 0.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::smartFade, 1 }, "Smart Fade", pct(), 0.5f));

        // ---- Level 2 MIDI --------------------------------------------------
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::triggerMode, 1 }, "Trigger",
                                             juce::StringArray { "Hold", "Latch", "Momentary" }, 0));
        layout.add (std::make_unique<APInt> (juce::ParameterID { id::triggerNote, 1 }, "Trig Note",
                                             0, 127, 60));

        // ---- Level 2 gate / duck / sidechain ------------------------------
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::gateAmount, 1 }, "Gate", pct(), 0.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckAmount, 1 }, "Duck", pct(), 0.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckAttack, 1 }, "Duck Atk",
                                             ms (0.5f, 200.0f), 5.0f));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckRelease, 1 }, "Duck Rel",
                                             ms (5.0f, 800.0f), 120.0f));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::scSource, 1 }, "SC Source",
                                             juce::StringArray { "Off", "Internal (Wet)", "Internal (Dry)" }, 0));

        return layout;
    }
}
