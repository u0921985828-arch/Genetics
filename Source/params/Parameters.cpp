#include "Parameters.h"

namespace chrona::params
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using APInt = juce::AudioParameterInt;
    using Range = juce::NormalisableRange<float>;

    using Attr  = juce::AudioParameterFloatAttributes;

    static Range pct()            { return Range (0.0f, 1.0f, 0.0001f); }
    static Range ms (float lo, float hi)
    {
        Range r (lo, hi);
        r.setSkewForCentre (juce::jlimit (lo + 1.0f, hi - 1.0f, (lo + hi) * 0.25f));
        return r;
    }

    // Host-readable value text (so DAW generic panels show "50 %" / "3.0 ms",
    // not a raw 0.50). The plugin's own GUI is unaffected.
    static Attr pctText()
    {
        return Attr().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() * 0.01f; });
    }
    static Attr swingText()
    {
        return Attr().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { const int p = juce::roundToInt (v * 100.0f);
                return juce::String (p > 0 ? "+" : "") + juce::String (p) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() * 0.01f; });
    }
    static Attr msText()
    {
        return Attr().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, v >= 10.0f ? 1 : 2) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // ---- Level 1 macros ------------------------------------------------
        layout.add (std::make_unique<APF> (juce::ParameterID { id::time,    1 }, "Time",    pct(), 0.5f, pctText()));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::depth,   1 }, "Depth",   pct(), 1.0f, pctText()));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::mix,     1 }, "Mix",     pct(), 1.0f, pctText()));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::texture, 1 }, "Texture", pct(), 0.0f, pctText()));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::space,   1 }, "Space",   pct(), 0.0f, pctText()));
        layout.add (std::make_unique<APF> (juce::ParameterID { id::width,   1 }, "Width",   pct(), 0.5f, pctText()));

        layout.add (std::make_unique<APC> (juce::ParameterID { id::mode, 1 }, "Mode",
                                           modeNames(), 0));

        // ---- Level 2 engine ------------------------------------------------
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::sync, 1 }, "Sync",
                                             syncDivisions(), 2));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::bufferBars, 1 }, "Buffer",
                                             juce::StringArray { "1 Bar", "2 Bars" }, 1));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::warpRate, 1 }, "Warp Rate",
                                             warpRates(), 3));
        // Snap is an editor-only preference (curve X snapping); it changes no
        // audio, so keep it out of host automation lanes.
        layout.add (std::make_unique<APB>   (juce::ParameterID { id::snap, 1 }, "Snap", true,
                                             juce::AudioParameterBoolAttributes().withAutomatable (false)));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::quality, 1 }, "Quality",
                                             juce::StringArray { "Linear", "Hermite", "Sinc (HQ)" }, 1));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::antiClick, 1 }, "Anti-Click",
                                             ms (0.5f, 25.0f), 3.0f, msText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::swing, 1 }, "Swing",
                                             Range (-0.5f, 0.5f, 0.0001f), 0.0f, swingText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::humanize, 1 }, "Humanize", pct(), 0.0f, pctText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::smartFade, 1 }, "Smart Fade", pct(), 0.5f, pctText()));

        // ---- Level 2 MIDI --------------------------------------------------
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::triggerMode, 1 }, "Trigger",
                                             juce::StringArray { "Hold", "Latch", "Momentary" }, 0));
        layout.add (std::make_unique<APInt> (juce::ParameterID { id::triggerNote, 1 }, "Trig Note",
                                             0, 127, 60));

        // ---- Level 2 gate / duck / sidechain ------------------------------
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::gateAmount, 1 }, "Gate", pct(), 0.0f, pctText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckAmount, 1 }, "Duck", pct(), 0.0f, pctText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckAttack, 1 }, "Duck Atk",
                                             ms (0.5f, 200.0f), 5.0f, msText()));
        layout.add (std::make_unique<APF>   (juce::ParameterID { id::duckRelease, 1 }, "Duck Rel",
                                             ms (5.0f, 800.0f), 120.0f, msText()));
        layout.add (std::make_unique<APC>   (juce::ParameterID { id::scSource, 1 }, "SC Source",
                                             juce::StringArray { "Off", "Internal (Wet)", "Internal (Dry)", "External" }, 0));

        // Host soft-bypass parameter (reported via getBypassParameter()).
        layout.add (std::make_unique<APB>   (juce::ParameterID { id::bypass, 1 }, "Bypass", false));

        return layout;
    }
}
