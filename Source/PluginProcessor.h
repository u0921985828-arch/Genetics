#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/Parameters.h"
#include "dsp/TemporalEngine.h"
#include "automation/AutomationEngine.h"
#include "midi/MidiTriggerSystem.h"
#include "presets/PresetManager.h"

namespace chrona
{
    // ========================================================================
    //  ChronaProcessor — the audio host boundary. It owns the subsystems and
    //  does nothing but marshal data between them each block:
    //    host transport  -> AutomationEngine
    //    APVTS values     -> TemporalEngine macros / advanced settings
    //    MIDI             -> MidiTriggerSystem -> engage flag
    //    audio            -> TemporalEngine::process
    //  All DSP lives in the engine; all state (de)serialisation in the preset
    //  manager. This class stays thin on purpose.
    // ========================================================================
    class ChronaProcessor : public juce::AudioProcessor
    {
    public:
        ChronaProcessor();
        ~ChronaProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

        const juce::String getName() const override { return "CHRONA"; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 2.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        // exposed to the editor
        juce::AudioProcessorValueTreeState apvts;
        automation::AutomationEngine automation;
        dsp::TemporalEngine engine;
        midi::MidiTriggerSystem trigger;
        presets::PresetManager presets;

    private:
        static BusesProperties makeBuses();
        void pullParameters();

        std::vector<float> scMono; // mono external-sidechain scratch (per block)

        // cached atomic pointers to avoid string lookups on the audio thread
        std::atomic<float>* pTime = nullptr; std::atomic<float>* pDepth = nullptr;
        std::atomic<float>* pMix = nullptr;  std::atomic<float>* pTexture = nullptr;
        std::atomic<float>* pSpace = nullptr; std::atomic<float>* pWidth = nullptr;
        std::atomic<float>* pMode = nullptr;  std::atomic<float>* pSync = nullptr;
        std::atomic<float>* pBuffer = nullptr;
        std::atomic<float>* pQuality = nullptr; std::atomic<float>* pAntiClick = nullptr;
        std::atomic<float>* pSwing = nullptr; std::atomic<float>* pHumanize = nullptr;
        std::atomic<float>* pSmartFade = nullptr; std::atomic<float>* pTrigMode = nullptr;
        std::atomic<float>* pTrigNote = nullptr; std::atomic<float>* pGate = nullptr;
        std::atomic<float>* pDuck = nullptr; std::atomic<float>* pDuckAtk = nullptr;
        std::atomic<float>* pDuckRel = nullptr; std::atomic<float>* pScSource = nullptr;
        std::atomic<float>* pBypass = nullptr;
        juce::AudioProcessorParameter* bypassParam = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChronaProcessor)
    };
}
