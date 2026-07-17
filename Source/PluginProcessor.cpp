#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace chrona
{
    juce::AudioProcessor::BusesProperties ChronaProcessor::makeBuses()
    {
        return BusesProperties()
            .withInput  ("Input",      juce::AudioChannelSet::stereo(), true)
            .withInput  ("Sidechain",  juce::AudioChannelSet::stereo(), false) // optional external SC
            .withOutput ("Output",     juce::AudioChannelSet::stereo(), true);
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
        pDuckRel = get (params::id::duckRelease);
        pBypass  = get (params::id::bypass);
        bypassParam = apvts.getParameter (params::id::bypass);pScSource = get (params::id::scSource);
    }

    bool ChronaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;
        if (layouts.getMainInputChannelSet() != out)
            return false;

        // Optional sidechain: disabled, mono or stereo are all fine.
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
            && sc != juce::AudioChannelSet::mono()
            && sc != juce::AudioChannelSet::stereo())
            return false;
        return true;
    }

    void ChronaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        const int ch = juce::jmax (getMainBusNumInputChannels(), 1);
        automation.prepare (sampleRate);
        engine.prepare (sampleRate, ch, samplesPerBlock);
        engine.setAutomation (&automation);
        trigger.prepare (sampleRate);
        scMono.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
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
        const double spb = automation.getSamplesPerBeat();

        const bool bypassed = pBypass->load() > 0.5f;
        const int  numSamples = buffer.getNumSamples();

        // ---- external sidechain → mono scratch (only when SC Source = External
        //      and the optional SC bus is actually connected) ----
        const float* scPtr = nullptr;
        if ((int) pScSource->load() == 3)
        {
            if (auto* scBusObj = getBus (true, 1); scBusObj != nullptr && scBusObj->isEnabled())
            {
                auto sc = getBusBuffer (buffer, true, 1);
                const int scCh = sc.getNumChannels();
                if (scCh > 0)
                {
                    const int nn = juce::jmin (numSamples, sc.getNumSamples());
                    for (int n = 0; n < nn; ++n)
                    {
                        float s = 0.0f;
                        for (int c = 0; c < scCh; ++c) s += sc.getReadPointer (c)[n];
                        scMono[(size_t) n] = s / (float) scCh;
                    }
                    for (int n = nn; n < numSamples; ++n) scMono[(size_t) n] = 0.0f;
                    scPtr = scMono.data();
                }
            }
        }

        // ---- sample-accurate MIDI trigger: split the block at trigger-note
        //      events so engage toggles exactly on the event sample ----
        auto mainBuf = getBusBuffer (buffer, false, 0);
        auto* const* mainPtrs = mainBuf.getArrayOfWritePointers();
        const int mainCh = mainBuf.getNumChannels();

        auto runSeg = [&] (int start, int len)
        {
            if (len <= 0) return;
            juce::AudioBuffer<float> sub (mainPtrs, mainCh, start, len);
            for (int i = 0; i < len; ++i) trigger.tick();
            automation::TransportInfo ts = tinfo;
            ts.ppqPosition = tinfo.ppqPosition + (double) start / juce::jmax (1.0, spb);
            automation.updateTransport (ts);
            engine.process (sub, trigger.engaged() && ! bypassed,
                            scPtr != nullptr ? scPtr + start : nullptr, len);
        };

        int segStart = 0;
        for (const auto meta : midi)
        {
            const int pos = juce::jlimit (0, numSamples, meta.samplePosition);
            if (pos > segStart) runSeg (segStart, pos - segStart);
            trigger.handleMessage (meta.getMessage());   // applied exactly here
            segStart = pos;
        }
        runSeg (segStart, numSamples - segStart);
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
