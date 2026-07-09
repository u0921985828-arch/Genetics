#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "BigKnob.h"
#include "../params/Parameters.h"
#include "../presets/PresetManager.h"
#include "../visualizer/CurveEditor.h"
#include "../visualizer/WaveformDisplay.h"

namespace chrona::ui
{
    // ========================================================================
    //  Level 2 (Advanced) — the drop-down panel: curve editors, grid/snap,
    //  buffer size, MIDI trigger, dynamics (gate/duck/sidechain), timing feel
    //  (swing/humanize/smart fade), the live buffer visualiser and the preset
    //  browser. Everything binds to the same APVTS as Level 1.
    // ========================================================================
    class Level2Panel : public juce::Component,
                        private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        Level2Panel (juce::AudioProcessorValueTreeState& s,
                     automation::AutomationEngine& a,
                     const dsp::TemporalEngine& e,
                     presets::PresetManager& pm)
            : state (s), presetMgr (pm),
              timeCurve (a, e, visualizer::CurveEditor::Target::Time),
              volCurve  (a, e, visualizer::CurveEditor::Target::Volume),
              waveform  (e)
        {
            namespace P = params::id;

            addAndMakeVisible (waveform);

            // curve tabs
            curveTabTime.setButtonText ("TIME CURVE");
            curveTabVol .setButtonText ("VOLUME");
            curveTabTime.setClickingTogglesState (true);
            curveTabVol .setClickingTogglesState (true);
            curveTabTime.setRadioGroupId (77);
            curveTabVol .setRadioGroupId (77);
            curveTabTime.setToggleState (true, juce::dontSendNotification);
            curveTabTime.onClick = [this] { showTimeCurve (true); };
            curveTabVol .onClick = [this] { showTimeCurve (false); };
            addAndMakeVisible (curveTabTime);
            addAndMakeVisible (curveTabVol);
            addAndMakeVisible (timeCurve);
            addChildComponent (volCurve);

            // combos
            addCombo (syncBox,    P::sync,       params::syncDivisions(),                       "GRID");
            addCombo (bufferBox,  P::bufferBars, juce::StringArray { "1 Bar", "2 Bars" },        "BUFFER");
            addCombo (qualityBox, P::quality,    juce::StringArray { "Linear", "Hermite", "Sinc" }, "QUALITY");
            addCombo (trigBox,    P::triggerMode,juce::StringArray { "Hold", "Latch", "Momentary" }, "TRIGGER");
            addCombo (scBox,      P::scSource,   juce::StringArray { "Off", "Wet", "Dry" },      "SIDECHAIN");

            // snap toggle
            snapButton.setButtonText ("SNAP");
            snapButton.setClickingTogglesState (true);
            addAndMakeVisible (snapButton);
            snapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, P::snap, snapButton);

            // small knobs
            knobs.add (new BigKnob (s, P::swing,      "SWING"));
            knobs.add (new BigKnob (s, P::humanize,   "HUMANIZE"));
            knobs.add (new BigKnob (s, P::smartFade,  "SMART FADE"));
            knobs.add (new BigKnob (s, P::gateAmount, "GATE"));
            knobs.add (new BigKnob (s, P::duckAmount, "DUCK"));
            knobs.add (new BigKnob (s, P::antiClick,  "ANTI-CLICK"));
            knobs.getLast()->setValueSuffix (" ms");
            for (auto* k : knobs) addAndMakeVisible (k);

            // preset browser
            presetBox.setTextWhenNothingSelected ("Presets");
            rebuildPresetList();
            presetBox.onChange = [this] { onPresetChosen(); };
            addAndMakeVisible (presetBox);

            saveButton.setButtonText ("SAVE");
            saveButton.onClick = [this] { savePreset(); };
            addAndMakeVisible (saveButton);

            state.addParameterListener (P::snap, this);
            state.addParameterListener (P::sync, this);
            updateGrid();
        }

        ~Level2Panel() override
        {
            state.removeParameterListener (params::id::snap, this);
            state.removeParameterListener (params::id::sync, this);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme::bg1);
            g.setColour (theme::greyDark);
            g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (12);

            // top: waveform strip
            waveform.setBounds (b.removeFromTop (juce::jmax (70, b.getHeight() / 5)));
            b.removeFromTop (8);

            // curve editor block with tabs
            auto curveArea = b.removeFromTop (juce::jmax (150, b.getHeight() / 2));
            auto tabs = curveArea.removeFromTop (26);
            curveTabTime.setBounds (tabs.removeFromLeft (120).reduced (2));
            curveTabVol .setBounds (tabs.removeFromLeft (120).reduced (2));
            snapButton  .setBounds (tabs.removeFromRight (80).reduced (2));
            timeCurve.setBounds (curveArea);
            volCurve .setBounds (curveArea);

            b.removeFromTop (8);

            // combos row
            auto combos = b.removeFromTop (46);
            auto placeCombo = [&combos] (LabeledCombo& lc, int w)
            {
                auto cell = combos.removeFromLeft (w).reduced (3);
                lc.label.setBounds (cell.removeFromTop (14));
                lc.box.setBounds (cell);
            };
            const int cw = combos.getWidth() / 5;
            placeCombo (syncBox, cw); placeCombo (bufferBox, cw); placeCombo (qualityBox, cw);
            placeCombo (trigBox, cw); placeCombo (scBox, cw);

            b.removeFromTop (8);

            // knobs row
            auto krow = b.removeFromTop (juce::jmax (80, b.getHeight() - 40));
            const int n = knobs.size();
            const int kw = krow.getWidth() / n;
            for (int i = 0; i < n; ++i)
                knobs[i]->setBounds (krow.removeFromLeft (kw).reduced (4));

            // preset browser
            auto pr = b.removeFromBottom (30);
            saveButton.setBounds (pr.removeFromRight (80).reduced (2));
            presetBox .setBounds (pr.reduced (2));
        }

        // Called after preset load so the curve editors reseed.
        void refreshCurves() { timeCurve.refreshFromEngine(); volCurve.refreshFromEngine(); rebuildPresetList(); }

    private:
        struct LabeledCombo { juce::ComboBox box; juce::Label label;
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; };

        void addCombo (LabeledCombo& lc, const char* id, const juce::StringArray& items, const juce::String& cap)
        {
            lc.box.addItemList (items, 1);
            addAndMakeVisible (lc.box);
            lc.label.setText (cap, juce::dontSendNotification);
            lc.label.setFont (theme::labelFont (11.0f));
            lc.label.setColour (juce::Label::textColourId, theme::greyMid);
            addAndMakeVisible (lc.label);
            lc.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, lc.box);
        }

        void showTimeCurve (bool showTime)
        {
            timeCurve.setVisible (showTime);
            volCurve .setVisible (! showTime);
        }

        void parameterChanged (const juce::String&, float) override
        {
            juce::MessageManager::callAsync ([this] { updateGrid(); });
        }

        void updateGrid()
        {
            const bool snapOn = state.getRawParameterValue (params::id::snap)->load() > 0.5f;
            int div = 8;
            if (auto* p = state.getParameter (params::id::sync))
            {
                const int idx = (int) p->convertFrom0to1 (p->getValue());
                const double beats = params::syncDivisionBeats (idx);
                div = juce::jlimit (2, 32, (int) std::round (8.0 / juce::jmax (0.125, beats / 4.0)));
            }
            timeCurve.setGrid (div, snapOn);
            volCurve .setGrid (div, snapOn);
        }

        void rebuildPresetList()
        {
            presetBox.clear (juce::dontSendNotification);
            factoryNames = presetMgr.getFactoryNames();
            userNames    = presetMgr.getUserPresetNames();
            int id = 1;
            presetBox.addSectionHeading ("Factory");
            for (const auto& n : factoryNames) presetBox.addItem (n, id++);
            if (! userNames.isEmpty())
            {
                presetBox.addSectionHeading ("User");
                for (const auto& n : userNames) presetBox.addItem (n, id++);
            }
        }

        void onPresetChosen()
        {
            const int idx = presetBox.getSelectedItemIndex();
            if (idx < 0) return;
            // section headings are not selectable, so index maps into names.
            if (idx < factoryNames.size())
                presetMgr.loadFactory (idx);
            else
                presetMgr.load (userNames[idx - factoryNames.size()]);
            refreshCurves();
        }

        void savePreset()
        {
            auto* aw = new juce::AlertWindow ("Save Preset", "Name your preset", juce::AlertWindow::NoIcon);
            aw->addTextEditor ("name", presetMgr.getCurrentName());
            aw->addButton ("Save", 1);
            aw->addButton ("Cancel", 0);
            aw->enterModalState (true, juce::ModalCallbackFunction::create (
                [this, aw] (int r)
                {
                    if (r == 1)
                    {
                        const auto name = aw->getTextEditorContents ("name");
                        if (name.isNotEmpty()) { presetMgr.save (name); rebuildPresetList(); }
                    }
                    delete aw;
                }));
        }

        juce::AudioProcessorValueTreeState& state;
        presets::PresetManager& presetMgr;

        visualizer::CurveEditor timeCurve, volCurve;
        visualizer::WaveformDisplay waveform;
        juce::TextButton curveTabTime, curveTabVol, snapButton, saveButton;

        LabeledCombo syncBox, bufferBox, qualityBox, trigBox, scBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAtt;

        juce::OwnedArray<BigKnob> knobs;
        juce::ComboBox presetBox;
        juce::StringArray factoryNames, userNames;
    };
}
