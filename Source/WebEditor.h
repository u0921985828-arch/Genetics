#pragma once

#if CHRONA_WEBVIEW

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "BinaryData.h"

namespace chrona
{
    // ========================================================================
    //  WebEditor — optional HTML/JS front-end (JUCE 8 WebView).
    //
    //  Enabled with -DCHRONA_WEBVIEW=ON. The six macros, the mode selector and
    //  bypass bind to the same APVTS parameters through JUCE relays; the buffer
    //  visualiser is pushed to the page as "vis" events. The native editor
    //  remains the default (dependency-free) UI.
    // ========================================================================
    class WebEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
    {
    public:
        explicit WebEditor (ChronaProcessor& p)
            : juce::AudioProcessorEditor (p), proc (p)
        {
            auto options = juce::WebBrowserComponent::Options{}
                .withNativeIntegrationEnabled()
                .withResourceProvider ([this] (const auto& url) { return provide (url); })
                .withOptionsFrom (timeR).withOptionsFrom (depthR).withOptionsFrom (mixR)
                .withOptionsFrom (textureR).withOptionsFrom (spaceR).withOptionsFrom (widthR)
                .withOptionsFrom (modeR).withOptionsFrom (bypassR);

            web = std::make_unique<juce::WebBrowserComponent> (options);
            addAndMakeVisible (*web);

            auto param = [&] (const char* id) { return proc.apvts.getParameter (id); };
            timeA    = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::time),    timeR);
            depthA   = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::depth),   depthR);
            mixA     = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::mix),     mixR);
            textureA = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::texture), textureR);
            spaceA   = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::space),   spaceR);
            widthA   = std::make_unique<juce::WebSliderParameterAttachment>     (*param (params::id::width),   widthR);
            modeA    = std::make_unique<juce::WebComboBoxParameterAttachment>   (*param (params::id::mode),    modeR);
            bypassA  = std::make_unique<juce::WebToggleButtonParameterAttachment>(*param (params::id::bypass), bypassR);

            web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "index.html");

            setResizable (true, true);
            setSize (760, 420);
            startTimerHz (30);
        }

        ~WebEditor() override { stopTimer(); }

        void resized() override { if (web) web->setBounds (getLocalBounds()); }

    private:
        std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url)
        {
            juce::String path = url.startsWith ("/") ? url.substring (1) : url;
            if (path.isEmpty()) path = "index.html";

            auto res = [] (const char* data, int size, const char* mime)
            {
                std::vector<std::byte> bytes ((size_t) size);
                std::memcpy (bytes.data(), data, (size_t) size);
                return juce::WebBrowserComponent::Resource { std::move (bytes), juce::String (mime) };
            };

            if (path == "index.html")    return res (BinaryData::index_html, BinaryData::index_htmlSize, "text/html");
            if (path == "style.css")     return res (BinaryData::style_css,  BinaryData::style_cssSize,  "text/css");
            if (path == "app.js")        return res (BinaryData::app_js,      BinaryData::app_jsSize,     "text/javascript");
            if (path == "juce/index.js") return res (BinaryData::index_js,    BinaryData::index_jsSize,   "text/javascript");
            return std::nullopt;
        }

        void timerCallback() override
        {
            if (web == nullptr) return;
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("phase", proc.engine.getPlayheadPhase());
            obj->setProperty ("delay", proc.engine.getReadDelayNorm());

            const int total = proc.engine.getVisBinCount();
            const int wb    = proc.engine.getVisWriteBin();
            constexpr int N = 128;
            juce::Array<juce::var> bins;
            bins.ensureStorageAllocated (N);
            for (int i = 0; i < N; ++i)
                bins.add (proc.engine.getVisBin ((wb + i * total / N) % total));
            obj->setProperty ("bins", bins);

            web->emitEventIfBrowserIsVisible ("vis", juce::var (obj));
        }

        ChronaProcessor& proc;
        juce::WebSliderRelay      timeR { "time" }, depthR { "depth" }, mixR { "mix" },
                                  textureR { "texture" }, spaceR { "space" }, widthR { "width" };
        juce::WebComboBoxRelay    modeR { "mode" };
        juce::WebToggleButtonRelay bypassR { "bypass" };
        std::unique_ptr<juce::WebBrowserComponent> web;
        std::unique_ptr<juce::WebSliderParameterAttachment>      timeA, depthA, mixA, textureA, spaceA, widthA;
        std::unique_ptr<juce::WebComboBoxParameterAttachment>    modeA;
        std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassA;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebEditor)
    };
}

#endif // CHRONA_WEBVIEW
