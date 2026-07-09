#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/ChronaLookAndFeel.h"
#include "ui/Level1Panel.h"
#include "ui/Level2Panel.h"
#include "ui/Theme.h"

namespace chrona
{
    // ========================================================================
    //  ChronaEditor — a HiDPI, zoomable (100–300%) window. Level 1 is always
    //  visible; Level 2 drops down when ADVANCED is toggled. All layout is done
    //  in logical coordinates on an inner Content component that is scaled by
    //  an AffineTransform, so the UI stays razor-sharp at any zoom.
    // ========================================================================
    class ChronaEditor : public juce::AudioProcessorEditor
    {
    public:
        explicit ChronaEditor (ChronaProcessor& p)
            : juce::AudioProcessorEditor (p), proc (p)
        {
            setLookAndFeel (&laf);
            content = std::make_unique<Content> (p);
            addAndMakeVisible (content.get());

            setResizable (true, true);
            applyZoom (zoom);
        }

        ~ChronaEditor() override { setLookAndFeel (nullptr); }

        void resized() override
        {
            // Derive zoom from the actual editor width vs the logical width so
            // host-driven resizes map cleanly to a zoom factor.
            const float logicalW = (float) Content::kLogicalW;
            const float z = juce::jlimit (1.0f, 3.0f, (float) getWidth() / logicalW);
            zoom = z; // keep the member in sync with host-driven resizes
            content->setTransform (juce::AffineTransform::scale (z));
            content->setBounds (0, 0, Content::kLogicalW, content->logicalHeight());
        }

    private:
        // ---- inner content laid out in logical (unscaled) pixels -----------
        class Content : public juce::Component
        {
        public:
            static constexpr int kLogicalW = 760;
            static constexpr int kHeaderH  = 48;
            static constexpr int kLevel1H  = 372;
            static constexpr int kLevel2H  = 430;

            explicit Content (ChronaProcessor& p)
                : level1 (p.apvts),
                  level2 (p.apvts, p.automation, p.engine, p.presets)
            {
                title.setText ("CHRONA", juce::dontSendNotification);
                title.setFont (ui::theme::valueFont (22.0f));
                title.setColour (juce::Label::textColourId, ui::theme::white);
                addAndMakeVisible (title);

                subtitle.setText ("temporal manipulation", juce::dontSendNotification);
                subtitle.setFont (ui::theme::labelFont (12.0f));
                subtitle.setColour (juce::Label::textColourId, ui::theme::accent);
                addAndMakeVisible (subtitle);

                advancedButton.setButtonText ("ADVANCED");
                advancedButton.setClickingTogglesState (true);
                advancedButton.onClick = [this]
                {
                    advancedOpen = advancedButton.getToggleState();
                    if (auto* pc = findParentComponentOfClass<ChronaEditor>())
                        pc->applyZoom (pc->currentZoom());
                    resized();
                    // fluid reveal (60fps) rather than a hard pop-in
                    auto& animator = juce::Desktop::getInstance().getAnimator();
                    if (advancedOpen) animator.fadeIn  (&level2, 220);
                    else              animator.fadeOut (&level2, 160);
                };
                addAndMakeVisible (advancedButton);

                addAndMakeVisible (level1);
                addChildComponent (level2);
            }

            int logicalHeight() const
            {
                return kHeaderH + kLevel1H + (advancedOpen ? kLevel2H : 0);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (ui::theme::bg0);
                auto header = getLocalBounds().removeFromTop (kHeaderH).toFloat();
                g.setColour (ui::theme::bg1);
                g.fillRect (header);
                g.setColour (ui::theme::greyDark);
                g.drawHorizontalLine (kHeaderH, 0.0f, (float) getWidth());
            }

            void resized() override
            {
                auto b = getLocalBounds();
                auto header = b.removeFromTop (kHeaderH);
                title.setBounds (header.removeFromLeft (150).withTrimmedLeft (16).withTrimmedTop (6).withHeight (26));
                subtitle.setBounds (16, 30, 200, 14);
                advancedButton.setBounds (header.removeFromRight (130).reduced (14, 10));

                level1.setBounds (b.removeFromTop (kLevel1H));
                if (advancedOpen)
                    level2.setBounds (b.removeFromTop (kLevel2H));
            }

            bool advancedIsOpen() const { return advancedOpen; }

        private:
            ui::Level1Panel level1;
            ui::Level2Panel level2;
            juce::Label title, subtitle;
            juce::TextButton advancedButton;
            bool advancedOpen = false;
        };

        void applyZoom (float z)
        {
            zoom = juce::jlimit (1.0f, 3.0f, z);
            const int h = content->logicalHeight();
            constrainer.setFixedAspectRatio ((double) Content::kLogicalW / (double) juce::jmax (1, h));
            constrainer.setSizeLimits (Content::kLogicalW, h,
                                       (int) (Content::kLogicalW * 3.0f), (int) (h * 3.0f));
            setConstrainer (&constrainer);
            setSize ((int) (Content::kLogicalW * zoom), (int) (h * zoom));
        }

        float currentZoom() const { return zoom; }

        ChronaProcessor& proc;
        ui::ChronaLookAndFeel laf;
        juce::ComponentBoundsConstrainer constrainer;
        std::unique_ptr<Content> content;
        float zoom = 1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChronaEditor)
    };
}
