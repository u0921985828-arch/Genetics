#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>
#include "../automation/AutomationEngine.h"

namespace chrona::presets
{
    // ========================================================================
    //  PresetManager — save/recall of the full plugin state (parameters + the
    //  two internal curves) to user files, plus an in-memory factory bank.
    //
    //  State layout (ValueTree):
    //    <CHRONA version="1">
    //      <PARAMS>            (APVTS state)
    //      <TimeCurve>...</>   (breakpoints)
    //      <VolCurve>...</>    (breakpoints)
    //      presetName=".."     (attribute on the root)
    // ========================================================================
    class PresetManager
    {
    public:
        PresetManager (juce::AudioProcessorValueTreeState& s, automation::AutomationEngine& a)
            : apvts (s), automation (a)
        {
            userDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("CHRONA").getChildFile ("Presets");
            userDir.createDirectory();
            buildFactoryBank();
            refresh();
        }

        // --- full-state (de)serialisation used by the processor -------------
        juce::ValueTree captureState() const
        {
            juce::ValueTree root ("CHRONA");
            root.setProperty ("version", 1, nullptr);
            root.appendChild (apvts.copyState(), nullptr);
            root.appendChild (automation.snapshotTimeCurve().toValueTree ("TimeCurve"), nullptr);
            root.appendChild (automation.snapshotVolumeCurve().toValueTree ("VolCurve"), nullptr);
            return root;
        }

        void applyState (const juce::ValueTree& root)
        {
            if (! root.isValid() || root.getType() != juce::Identifier ("CHRONA"))
                return;

            // APVTS is the first non-curve child.
            for (int i = 0; i < root.getNumChildren(); ++i)
            {
                const auto child = root.getChild (i);
                if (child.getType() == apvts.state.getType())
                    apvts.replaceState (child);
                else if (child.getType() == juce::Identifier ("TimeCurve"))
                {
                    automation::Curve c; c.fromValueTree (child);
                    automation.publishTimeCurve (c);
                }
                else if (child.getType() == juce::Identifier ("VolCurve"))
                {
                    automation::Curve c; c.fromValueTree (child);
                    automation.publishVolumeCurve (c);
                }
            }
        }

        // --- user preset files ----------------------------------------------
        bool save (const juce::String& name)
        {
            auto file = userDir.getChildFile (juce::File::createLegalFileName (name) + ".chrona");
            auto state = captureState();
            state.setProperty ("presetName", name, nullptr);
            if (auto xml = state.createXml())
            {
                const bool ok = xml->writeTo (file);
                refresh();
                return ok;
            }
            return false;
        }

        bool load (const juce::String& name)
        {
            // user preset first, then factory
            auto file = userDir.getChildFile (juce::File::createLegalFileName (name) + ".chrona");
            if (file.existsAsFile())
            {
                if (auto xml = juce::XmlDocument::parse (file))
                {
                    applyState (juce::ValueTree::fromXml (*xml));
                    currentName = name;
                    return true;
                }
            }
            for (const auto& f : factory)
                if (f.name == name) { applyState (f.state); currentName = name; return true; }
            return false;
        }

        void loadFactory (int index)
        {
            if (juce::isPositiveAndBelow (index, (int) factory.size()))
            {
                applyState (factory[(size_t) index].state);
                currentName = factory[(size_t) index].name;
            }
        }

        juce::StringArray getUserPresetNames() const
        {
            juce::StringArray names;
            for (const auto& f : userDir.findChildFiles (juce::File::findFiles, false, "*.chrona"))
                names.add (f.getFileNameWithoutExtension());
            names.sort (true);
            return names;
        }

        juce::StringArray getFactoryNames() const
        {
            juce::StringArray names;
            for (const auto& f : factory) names.add (f.name);
            return names;
        }

        const juce::String& getCurrentName() const { return currentName; }
        juce::File getUserDirectory() const { return userDir; }

    private:
        struct FactoryPreset { juce::String name; juce::ValueTree state; };

        void refresh() { /* hook for caching directory listings if needed */ }

        // Build factory presets purely from parameter tweaks so they always
        // stay valid against the current parameter layout.
        void buildFactoryBank();

        juce::ValueTree makeState (const std::function<void (juce::ValueTree&)>& setParams,
                                   const automation::Curve& timeC,
                                   const automation::Curve& volC) const;

        juce::AudioProcessorValueTreeState& apvts;
        automation::AutomationEngine& automation;
        juce::File userDir;
        juce::String currentName { "Init" };
        std::vector<FactoryPreset> factory;
    };
}
