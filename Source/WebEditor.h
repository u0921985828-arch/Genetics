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
    //  visualiser + I/O levels are pushed to the page as "vis" events. The
    //  preset browser and A/B compare are wired through native functions to the
    //  real PresetManager, so the WebUI is a full front-end, not a mockup.
    // ========================================================================
    class WebEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
    {
    public:
        explicit WebEditor (ChronaProcessor& p)
            : juce::AudioProcessorEditor (p), proc (p)
        {
            using WB = juce::WebBrowserComponent;
            auto options = WB::Options{}
                .withNativeIntegrationEnabled()
                .withResourceProvider ([this] (const auto& url) { return provide (url); })
                .withOptionsFrom (timeR).withOptionsFrom (depthR).withOptionsFrom (mixR)
                .withOptionsFrom (textureR).withOptionsFrom (spaceR).withOptionsFrom (widthR)
                .withOptionsFrom (modeR).withOptionsFrom (bypassR).withOptionsFrom (warpRateR)
                .withNativeFunction (juce::Identifier ("uiReady"),
                    [this] (const juce::Array<juce::var>&, auto complete) { pushPreset(); pushAB(); complete (juce::var()); })
                .withNativeFunction (juce::Identifier ("presetNext"),
                    [this] (const juce::Array<juce::var>&, auto complete) { stepPreset (1);  complete (juce::var()); })
                .withNativeFunction (juce::Identifier ("presetPrev"),
                    [this] (const juce::Array<juce::var>&, auto complete) { stepPreset (-1); complete (juce::var()); })
                .withNativeFunction (juce::Identifier ("abSelect"),
                    [this] (const juce::Array<juce::var>& a, auto complete)
                    { abSelect (a.isEmpty() ? 0 : (int) a[0]); complete (juce::var()); })
                .withNativeFunction (juce::Identifier ("getPresetList"),
                    [this] (const juce::Array<juce::var>&, auto complete)
                    { juce::Array<juce::var> arr; for (const auto& n : allPresetNames()) arr.add (n); complete (arr); })
                .withNativeFunction (juce::Identifier ("presetLoad"),
                    [this] (const juce::Array<juce::var>& a, auto complete)
                    { if (! a.isEmpty()) loadPresetByIndex ((int) a[0]); complete (juce::var()); })
                .withNativeFunction (juce::Identifier ("getCurves"),
                    [this] (const juce::Array<juce::var>&, auto complete) { complete (curvesVar()); })
                .withNativeFunction (juce::Identifier ("setCurve"),
                    [this] (const juce::Array<juce::var>& a, auto complete)
                    { if (a.size() >= 2) applyCurveFromJs (a[0].toString(), a[1]); complete (juce::var()); });

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
            warpA    = std::make_unique<juce::WebComboBoxParameterAttachment>   (*param (params::id::warpRate),warpRateR);
            bypassA  = std::make_unique<juce::WebToggleButtonParameterAttachment>(*param (params::id::bypass), bypassR);

            // A/B slots start as two copies of the current state.
            slot[0] = proc.presets.captureState();
            slot[1] = slot[0];

            web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "index.html");

            setResizable (true, true);
            if (auto* c = getConstrainer())
                c->setSizeLimits (780, 780, 1600, 1320);
            setSize (880, 890);
            startTimerHz (30);
        }

        ~WebEditor() override { stopTimer(); }

        void resized() override { if (web) web->setBounds (getLocalBounds()); }

        // After a host state load, tell the page to re-pull preset + curves (the
        // "preset" event handler in app.js reloads the curve editor).
        void refreshAfterStateLoad() { pushPreset(); pushAB(); }

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

        // --- preset browser -------------------------------------------------
        juce::StringArray allPresetNames() const
        {
            juce::StringArray n = proc.presets.getFactoryNames();
            n.addArray (proc.presets.getUserPresetNames());
            return n;
        }
        void loadPresetByIndex (int i)
        {
            const auto names = allPresetNames();
            if (names.isEmpty()) return;
            const int count = names.size();
            presetIndex = ((i % count) + count) % count;
            const int nFactory = proc.presets.getFactoryNames().size();
            if (presetIndex < nFactory) proc.presets.loadFactory (presetIndex);
            else                        proc.presets.load (names[presetIndex]);
            pushPreset();
        }
        void stepPreset (int d) { loadPresetByIndex (presetIndex + d); }
        void pushPreset()
        {
            if (web == nullptr) return;
            auto* o = new juce::DynamicObject();
            o->setProperty ("name",  proc.presets.getCurrentName());
            o->setProperty ("index", presetIndex);
            o->setProperty ("count", allPresetNames().size());
            web->emitEventIfBrowserIsVisible ("preset", juce::var (o));
        }

        // --- A/B compare ----------------------------------------------------
        void abSelect (int s)
        {
            s = juce::jlimit (0, 1, s);
            if (s != activeSlot)
            {
                slot[(size_t) activeSlot] = proc.presets.captureState(); // stash current
                proc.presets.applyState (slot[(size_t) s]);              // recall the other
                activeSlot = s;
            }
            pushAB();
        }
        void pushAB()
        {
            if (web == nullptr) return;
            auto* o = new juce::DynamicObject();
            o->setProperty ("slot", activeSlot);
            web->emitEventIfBrowserIsVisible ("ab", juce::var (o));
        }

        // --- Time / Volume curve editor bridge -----------------------------
        static juce::var curveToVar (const automation::Curve& c)
        {
            juce::Array<juce::var> arr;
            for (const auto& p : c.getPoints())
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("x", p.x); o->setProperty ("y", p.y); o->setProperty ("c", p.curve);
                arr.add (juce::var (o));
            }
            return arr;
        }
        juce::var curvesVar()
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("time", curveToVar (proc.automation.snapshotTimeCurve()));
            o->setProperty ("vol",  curveToVar (proc.automation.snapshotVolumeCurve()));
            return juce::var (o);
        }
        void applyCurveFromJs (const juce::String& which, const juce::var& pts)
        {
            std::vector<automation::Point> v;
            if (auto* arr = pts.getArray())
            {
                v.reserve ((size_t) arr->size());
                for (const auto& e : *arr)
                    v.push_back ({ (float) (double) e.getProperty ("x", 0.0),
                                   (float) (double) e.getProperty ("y", 0.0),
                                   (float) (double) e.getProperty ("c", 0.0) });
            }
            if (v.size() < 2) return;                       // need at least the two endpoints
            automation::Curve c;
            c.reserve (automation::Curve::kMaxPoints);
            c.setPoints (std::move (v));
            if (which == "vol") proc.automation.publishVolumeCurve (c);
            else                proc.automation.publishTimeCurve (c);
        }

        void timerCallback() override
        {
            if (web == nullptr) return;
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("phase", proc.engine.getPlayheadPhase());
            obj->setProperty ("delay", proc.engine.getReadDelayNorm());
            obj->setProperty ("inLvl",  proc.engine.getInLevel());
            obj->setProperty ("outLvl", proc.engine.getOutLevel());

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
        juce::WebComboBoxRelay    modeR { "mode" }, warpRateR { "warpRate" };
        juce::WebToggleButtonRelay bypassR { "bypass" };
        std::unique_ptr<juce::WebBrowserComponent> web;
        std::unique_ptr<juce::WebSliderParameterAttachment>      timeA, depthA, mixA, textureA, spaceA, widthA;
        std::unique_ptr<juce::WebComboBoxParameterAttachment>    modeA, warpA;
        std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassA;

        int presetIndex = 0;
        juce::ValueTree slot[2];
        int activeSlot = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebEditor)
    };
}

#endif // CHRONA_WEBVIEW
