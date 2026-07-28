#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  Parameters — the single source of truth for every automatable value.
//
//  The plugin state lives in one AudioProcessorValueTreeState (APVTS). Both UI
//  levels bind to the SAME parameters; Level 1 exposes the six macro knobs and
//  the mode selector, Level 2 exposes the rest. Nothing in the DSP or UI reads
//  a raw value directly — everything goes through these IDs so host automation
//  and preset recall stay consistent.
// ============================================================================

namespace chrona::params
{
    // -- IDs (use these constants everywhere; never hard-code the strings) ---
    namespace id
    {
        // Level 1 — macro knobs
        inline constexpr auto time    = "time";      // musical rate / warp amount
        inline constexpr auto depth   = "depth";     // effect intensity
        inline constexpr auto mix     = "mix";       // dry/wet
        inline constexpr auto texture = "texture";   // saturation / vinyl grit / filter tilt
        inline constexpr auto space   = "space";     // ambience / tail
        inline constexpr auto width   = "width";     // stereo width

        // Level 1 — mode selector
        inline constexpr auto mode    = "mode";

        // Level 2 — engine
        inline constexpr auto sync        = "sync";          // grid division (choice)
        inline constexpr auto bufferBars  = "bufferBars";    // 1 or 2 bars (choice)
        inline constexpr auto warpRate    = "warpRate";      // Time-Warp curve rate (choice)
        inline constexpr auto snap        = "snap";          // curve snap on/off
        inline constexpr auto quality     = "quality";       // interpolation quality (choice)
        inline constexpr auto antiClick   = "antiClick";     // fade length ms
        inline constexpr auto swing       = "swing";         // -0.5..0.5
        inline constexpr auto humanize    = "humanize";      // 0..1
        inline constexpr auto smartFade   = "smartFade";     // 0..1

        // Level 2 — MIDI trigger
        inline constexpr auto triggerMode = "triggerMode";   // Hold / Latch / Momentary (choice)
        inline constexpr auto triggerNote = "triggerNote";   // MIDI note that arms the effect

        // Level 2 — gating / ducking / internal sidechain
        inline constexpr auto gateAmount  = "gateAmount";    // 0..1
        inline constexpr auto duckAmount  = "duckAmount";    // 0..1
        inline constexpr auto duckAttack  = "duckAttack";    // ms
        inline constexpr auto duckRelease = "duckRelease";   // ms
        inline constexpr auto scSource    = "scSource";      // internal SC source (choice)

        // Host soft-bypass (exposed via AudioProcessor::getBypassParameter).
        inline constexpr auto bypass = "bypass";

        // Which curve/preset drives PAT (custom) mode. Stored separately in
        // the preset ValueTree, not as an automatable float.
    }

    // -- Enumerations mirrored by the DSP dispatcher ------------------------
    enum class Mode
    {
        // Freeze/Granular are APPENDED after Custom to keep existing choice
        // indices (0..8) stable for backward-compatible session recall.
        Half = 0, Double, Reverse, TapeStop, Stutter, BeatRepeat, Vinyl, Glitch, Custom,
        Freeze, Granular,
        NumModes
    };

    inline juce::StringArray modeNames()
    {
        return { "Half", "Double", "Reverse", "Tape Stop",
                 "Stutter", "Beat Repeat", "Vinyl", "Glitch", "Time Warp",
                 "Freeze", "Granular" };
    }

    inline juce::StringArray syncDivisions()
    {
        return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T", "1/4D", "1/8D" };
    }

    // Beat length of each division (in quarter notes).
    inline double syncDivisionBeats (int index)
    {
        switch (index)
        {
            case 0: return 4.0;            // 1/1
            case 1: return 2.0;            // 1/2
            case 2: return 1.0;            // 1/4
            case 3: return 0.5;            // 1/8
            case 4: return 0.25;           // 1/16
            case 5: return 1.0 / 3.0;      // 1/8T
            case 6: return 1.0 / 6.0;      // 1/16T
            case 7: return 1.5;            // 1/4 dotted
            case 8: return 0.75;           // 1/8 dotted
            default: return 1.0;
        }
    }

    enum class TriggerMode { Hold = 0, Latch, Momentary };
    enum class Quality     { Linear = 0, Hermite, Sinc /* highest */ };

    inline juce::StringArray warpRates()
    {
        return { "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" };
    }
    // Length of one Time-Warp curve cycle, in BARS (independent of the buffer).
    inline double warpRateBars (int index)
    {
        switch (index) { case 0: return 0.25; case 1: return 0.5; case 2: return 1.0;
                         case 4: return 4.0;  default: return 2.0; }
    }

    // -- Layout --------------------------------------------------------------
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
