#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <utility>
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
            root.setProperty ("version", kStateVersion, nullptr);
            root.appendChild (apvts.copyState(), nullptr);
            root.appendChild (automation.snapshotTimeCurve().toValueTree ("TimeCurve"), nullptr);
            root.appendChild (automation.snapshotVolumeCurve().toValueTree ("VolCurve"), nullptr);
            return root;
        }

        static constexpr int kStateVersion = 1;

        void applyState (const juce::ValueTree& root)
        {
            if (! root.isValid() || root.getType() != juce::Identifier ("CHRONA"))
                return;

            // Don't silently half-apply a newer/unknown format written by a
            // future build — leave the current state untouched instead.
            if ((int) root.getProperty ("version", kStateVersion) > kStateVersion)
                return;

            // Reset everything to defaults FIRST so a preset that omits a value
            // (e.g. an older file predating a newly-added parameter, or a missing
            // curve) recalls the default rather than keeping the pre-load value.
            for (int i = 0; i < apvts.state.getNumChildren(); ++i)
            {
                const auto child = apvts.state.getChild (i);
                if (child.hasType ("PARAM"))
                    if (auto* p = apvts.getParameter (child.getProperty ("id").toString()))
                        p->setValueNotifyingHost (p->getDefaultValue());
            }
            { automation::Curve t; t.setFlat (0.0f); automation.publishTimeCurve   (t); }
            { automation::Curve v; v.setFlat (1.0f); automation.publishVolumeCurve (v); }

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
            // user / bank preset first (matched by display name), then factory
            for (const auto& e : userPresets())
                if (e.first == name)
                    if (auto xml = juce::XmlDocument::parse (e.second))
                    {
                        applyState (juce::ValueTree::fromXml (*xml));
                        currentName = name;
                        return true;
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

        // User + factory-bank presets. Banks are immediate subfolders of the
        // preset directory, e.g. "Halftime Heavy / Amber Signal". Root-level
        // user presets keep their plain name.
        juce::StringArray getUserPresetNames() const
        {
            juce::StringArray names;
            for (const auto& e : userPresets()) names.add (e.first);
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

        // Invalidate the on-disk preset cache (rebuilt lazily on next access).
        void refresh() { userCacheValid = false; }

        // Display name for a preset file: "Bank / Name" for a file inside a
        // bank subfolder, plain "Name" for a file at the preset-dir root.
        juce::String displayName (const juce::File& f) const
        {
            const auto base   = f.getFileNameWithoutExtension();
            const auto parent = f.getParentDirectory();
            return (parent == userDir) ? base : (parent.getFileName() + " / " + base);
        }

        // Lazily-built, sorted cache of every on-disk preset (recursive, so it
        // includes factory banks shipped as subfolders). Avoids re-walking the
        // tree on every preset-browser interaction.
        const std::vector<std::pair<juce::String, juce::File>>& userPresets() const
        {
            if (! userCacheValid)
            {
                userCache.clear();
                for (const auto& f : userDir.findChildFiles (juce::File::findFiles, true, "*.chrona"))
                    userCache.emplace_back (displayName (f), f);
                std::sort (userCache.begin(), userCache.end(),
                           [] (const auto& a, const auto& b) { return a.first.compareNatural (b.first) < 0; });
                userCacheValid = true;
            }
            return userCache;
        }

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
        mutable std::vector<std::pair<juce::String, juce::File>> userCache;
        mutable bool userCacheValid = false;
        std::vector<FactoryPreset> factory;
    };
}
