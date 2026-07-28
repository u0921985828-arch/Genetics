#include "PresetManager.h"
#include "../params/Parameters.h"

namespace chrona::presets
{
    using chrona::automation::Curve;
    namespace pid = chrona::params::id;

    // Set a single parameter value inside a copied APVTS state tree.
    static void setParam (juce::ValueTree& params, const char* id, float value)
    {
        auto child = params.getChildWithProperty ("id", juce::String (id));
        if (child.isValid())
            child.setProperty ("value", value, nullptr);
    }

    juce::ValueTree PresetManager::makeState (const std::function<void (juce::ValueTree&)>& setParams,
                                              const Curve& timeC, const Curve& volC) const
    {
        juce::ValueTree root ("CHRONA");
        root.setProperty ("version", kStateVersion, nullptr);

        auto params = apvts.copyState();   // current values as a starting point
        setParams (params);
        root.appendChild (params, nullptr);
        root.appendChild (timeC.toValueTree ("TimeCurve"), nullptr);
        root.appendChild (volC.toValueTree ("VolCurve"), nullptr);
        return root;
    }

    void PresetManager::buildFactoryBank()
    {
        factory.clear();

        auto flatTime = [] { Curve c; c.setFlat (0.0f); return c; };
        auto flatVol  = [] { Curve c; c.setFlat (1.0f); return c; };

        auto add = [&] (const juce::String& name, params::Mode mode,
                        std::function<void (juce::ValueTree&)> extra,
                        Curve tc, Curve vc)
        {
            auto setter = [mode, extra] (juce::ValueTree& p)
            {
                setParam (p, pid::mode, (float) (int) mode);
                if (extra) extra (p);
            };
            factory.push_back ({ name, makeState (setter, tc, vc) });
        };

        // --- instant, no-edit results (the <5s UX bank) --------------------
        add ("Init", params::Mode::Half, [] (juce::ValueTree& p)
        {
            setParam (p, pid::mix, 1.0f); setParam (p, pid::depth, 1.0f);
        }, flatTime(), flatVol());

        add ("Half Time Anthem", params::Mode::Half, [] (juce::ValueTree& p)
        {
            setParam (p, pid::sync, 2.0f); setParam (p, pid::texture, 0.35f);
            setParam (p, pid::space, 0.2f); setParam (p, pid::depth, 1.0f);
        }, flatTime(), flatVol());

        add ("Double Drop", params::Mode::Double, [] (juce::ValueTree& p)
        {
            setParam (p, pid::sync, 3.0f); setParam (p, pid::depth, 1.0f);
        }, flatTime(), flatVol());

        add ("Reverse Sweep", params::Mode::Reverse, [] (juce::ValueTree& p)
        {
            setParam (p, pid::sync, 1.0f); setParam (p, pid::space, 0.4f);
        }, flatTime(), flatVol());

        add ("Tape Killer", params::Mode::TapeStop, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.6f); setParam (p, pid::depth, 0.8f);
            setParam (p, pid::texture, 0.5f);
        }, flatTime(), flatVol());

        add ("Machine Gun", params::Mode::Stutter, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.8f); setParam (p, pid::sync, 4.0f);
            setParam (p, pid::gateAmount, 0.3f);
        }, flatTime(), flatVol());

        add ("Beat Roll", params::Mode::BeatRepeat, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.4f); setParam (p, pid::depth, 0.5f);
        }, flatTime(), flatVol());

        add ("Dusty Vinyl", params::Mode::Vinyl, [] (juce::ValueTree& p)
        {
            setParam (p, pid::depth, 0.6f); setParam (p, pid::texture, 0.7f);
            setParam (p, pid::space, 0.25f);
        }, flatTime(), flatVol());

        add ("Data Corrupt", params::Mode::Glitch, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.7f); setParam (p, pid::depth, 0.8f);
            setParam (p, pid::humanize, 0.3f);
        }, flatTime(), flatVol());

        // --- Time-Warp (curve) showcases -----------------------------------
        {
            // Gated freeze: hold time on a step, chop the volume.
            Curve tc;
            tc.setPoints ({ { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.0f, 0.0f },
                            { 0.25f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.0f },
                            { 0.5f, 0.0f, 0.0f },  { 1.0f, 0.0f, 0.0f } });
            Curve vc;
            vc.setPoints ({ { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f },
                            { 0.5f, 0.0f, 0.0f }, { 0.75f, 0.0f, 0.0f },
                            { 0.75f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });
            add ("Warp: Gated Freeze", params::Mode::Custom, [] (juce::ValueTree& p)
            {
                setParam (p, pid::bufferBars, 1.0f);
            }, tc, vc);
        }
        {
            // Time-Warp sweep: dive back into the buffer and rush back to live.
            Curve tc;
            tc.setPoints ({ { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.85f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
            add ("Warp: Rewind Sweep", params::Mode::Custom, [] (juce::ValueTree& p)
            {
                setParam (p, pid::texture, 0.3f); setParam (p, pid::space, 0.25f);
            }, tc, flatVol());
        }
        {
            // Virtual scratch: rapid forward/back zig-zag on the time axis.
            Curve tc;
            tc.setPoints ({ { 0.0f, 0.0f, 0.0f }, { 0.125f, 0.35f, 0.0f },
                            { 0.25f, 0.0f, 0.0f }, { 0.375f, 0.35f, 0.0f },
                            { 0.5f, 0.0f, 0.0f },  { 0.625f, 0.35f, 0.0f },
                            { 0.75f, 0.0f, 0.0f }, { 0.875f, 0.35f, 0.0f },
                            { 1.0f, 0.0f, 0.0f } });
            add ("Warp: Vinyl Scratch", params::Mode::Custom, [] (juce::ValueTree& p)
            {
                setParam (p, pid::bufferBars, 1.0f);
                setParam (p, pid::texture, 0.55f);
            }, tc, flatVol());
        }
        {
            // Half-speed glide over a whole bar (shows off the Warp RATE control).
            Curve tc; tc.setPoints ({ { 0.0f, 0.0f, 0.4f }, { 1.0f, 0.7f, 0.0f } });
            add ("Warp: Bar Glide", params::Mode::Custom, [] (juce::ValueTree& p)
            {
                setParam (p, pid::warpRate, 2.0f);  // 1 Bar cycle
                setParam (p, pid::space, 0.25f);
            }, tc, flatVol());
        }

        // --- new-engine showcases (Freeze / Granular) ----------------------
        add ("Frozen Bloom", params::Mode::Freeze, [] (juce::ValueTree& p)
        {
            setParam (p, pid::depth, 0.85f); setParam (p, pid::space, 0.45f);
            setParam (p, pid::width, 0.75f); setParam (p, pid::mix, 1.0f);
        }, flatTime(), flatVol());

        add ("Grain Cloud", params::Mode::Granular, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.5f);  setParam (p, pid::depth, 0.85f);
            setParam (p, pid::space, 0.35f); setParam (p, pid::width, 0.8f);
        }, flatTime(), flatVol());

        add ("Granular Freeze Pad", params::Mode::Granular, [] (juce::ValueTree& p)
        {
            setParam (p, pid::time, 0.75f); setParam (p, pid::depth, 0.6f);
            setParam (p, pid::space, 0.6f);  setParam (p, pid::texture, 0.2f);
            setParam (p, pid::width, 0.9f);
        }, flatTime(), flatVol());
    }
}
