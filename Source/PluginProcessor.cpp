#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace chrona
{
    juce::AudioProcessor::BusesProperties ChronaProcessor::makeBuses()
    {
        return BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }

    ChronaProcessor::ChronaProcessor()
        : juce::AudioProcessor (makeBuses()),
          apvts (*this, nullptr, "STATE", params::createLayout()),
          presets (apvts, automation)
    {
        auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
        pTime = get (params::id::time);          pDepth = get (params::id::depth);
        pMix = get (params::id::mix);            pTexture = get (params::id::texture);
        pSpace = get (params::id::space);        pWidth = get (params::id::width);
        pMode = get (params::id::mode);          pSync = get (params::id::sync);
        pBuffer = get (params::id::bufferBars);
        pQuality = get (params::id::quality);    pAntiClick = get (params::id::antiClick);
        pSwing = get (params::id::swing);        pHumanize = get (params::id::humanize);
        pSmartFade = get (params::id::smartFade);pTrigMode = get (params::id::triggerMode);
        pTrigNote = get (params::id::triggerNote);pGate = get (params::id::gateAmount);
        pDuck = get (params::id::duckAmount);    pDuckAtk = get (params::id::duckAttack);
        pDuckRel = get (params::id::duckRelease);pScSource = get (params::id::scSource);
    }

    bool ChronaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;
        return layouts.getMainInputChannelSet() == out;
    }

    void ChronaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        const int ch = juce::jmax (getMainBusNumInputChannels(), 1);
        automation.prepare (sampleRate);
        engine.prepare (sampleRate, ch, samplesPerBlock);
        engine.setAutomation (&automation);
        trigger.prepare (sampleRate);
    }

    void ChronaProcessor::pullParameters()
    {
        dsp::TemporalEngine::Macros m;
        m.time = pTime->load();   m.depth = pDepth->load(); m.mix = pMix->load();
        m.texture = pTexture->load(); m.space = pSpace->load(); m.width = pWidth->load();
        engine.setMacros (m);

        dsp::TemporalEngine::Level2 l;
        l.syncIndex   = (int) pSync->load();
        l.bufferBars  = (int) pBuffer->load() + 1;      // choice 0/1 -> 1/2 bars
        l.quality     = (params::Quality) (int) pQuality->load();
        l.antiClickMs = pAntiClick->load();
        l.swing       = pSwing->load();
        l.humanize    = pHumanize->load();
        l.smartFade   = pSmartFade->load();
        l.gate        = pGate->load();
        l.duck        = pDuck->load();
        l.duckAtkMs   = pDuckAtk->load();
        l.duckRelMs   = pDuckRel->load();
        l.scSource    = (int) pScSource->load();
        engine.setLevel2 (l);
        engine.setMode ((params::Mode) (int) pMode->load());

        automation.setBufferBars (l.bufferBars);

        trigger.setTriggerNote ((int) pTrigNote->load());
        trigger.setMode ((params::TriggerMode) (int) pTrigMode->load());
    }

    void ChronaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals noDenormals;

        // clear any output channels beyond the inputs
        for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        pullParameters();

        // ---- transport ----
        automation::TransportInfo tinfo;
        tinfo.sampleRate = getSampleRate();
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto bpm = pos->getBpm())               tinfo.bpm = *bpm;
                if (auto ppq = pos->getPpqPosition())        tinfo.ppqPosition = *ppq;
                tinfo.isPlaying = pos->getIsPlaying();
                if (auto sig = pos->getTimeSignature())
                {
                    tinfo.numerator = sig->numerator;
                    tinfo.denominator = sig->denominator;
                }
            }
        }
        automation.updateTransport (tinfo);

        // ---- MIDI trigger: engage flag evaluated per block (event-accurate
        //      enough for a performance effect; sample-accurate arm is applied
        //      inside the engine's engage smoother).
        for (const auto meta : midi)
            trigger.handleMessage (meta.getMessage());
        for (int i = 0; i < buffer.getNumSamples(); ++i) trigger.tick();

        engine.process (buffer, trigger.engaged());
    }

    juce::AudioProcessorEditor* ChronaProcessor::createEditor()
    {
        return new ChronaEditor (*this);
    }

    void ChronaProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        auto state = presets.captureState();
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, dest);
    }

    void ChronaProcessor::setStateInformation (const void* data, int size)
    {
        if (auto xml = getXmlFromBinary (data, size))
            presets.applyState (juce::ValueTree::fromXml (*xml));
    }
}

// The plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new chrona::ChronaProcessor();
}
