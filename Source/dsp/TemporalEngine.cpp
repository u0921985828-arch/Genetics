#include "TemporalEngine.h"

namespace chrona::dsp
{
    TemporalEngine::TemporalEngine()
    {
        using M = params::Mode;
        modes[(size_t) M::Half]      = std::make_unique<SpeedMode> (SpeedMode::Half);
        modes[(size_t) M::Double]    = std::make_unique<SpeedMode> (SpeedMode::Double);
        modes[(size_t) M::Reverse]   = std::make_unique<SpeedMode> (SpeedMode::Reverse);
        modes[(size_t) M::TapeStop]  = std::make_unique<TapeStopMode>();
        modes[(size_t) M::Stutter]   = std::make_unique<SliceMode> (SliceMode::Stutter);
        modes[(size_t) M::BeatRepeat]= std::make_unique<SliceMode> (SliceMode::BeatRepeat);
        modes[(size_t) M::Vinyl]     = std::make_unique<VinylMode>();
        modes[(size_t) M::Glitch]    = std::make_unique<GlitchMode>();
        modes[(size_t) M::Custom]    = std::make_unique<CustomMode>();
        modes[(size_t) M::Freeze]    = std::make_unique<FreezeMode>();
        modes[(size_t) M::Granular]  = std::make_unique<GranularMode>();
    }

    void TemporalEngine::prepare (double sr, int numChannels, int /*maxBlock*/)
    {
        sampleRate = sr;
        channels   = juce::jlimit (1, 2, numChannels);

        // 2 bars at the slowest tempo we care about (down to 40 BPM in 4/4).
        const double maxBarSeconds = (60.0 / 40.0) * 4.0;
        buffer.prepare (sr, channels, maxBarSeconds);

        for (auto& m : modes)
            if (m) m->prepare (sr, channels);

        for (auto& d : declick) { d.prepare (sr); d.setTimeMs (level2.antiClickMs); }
        for (auto& t : tilt)    t.prepare (sr);
        for (auto& e : scEnv)   e.prepare (sr);
        space.prepare (sr, channels);

        engageSmoother.prepare (sr, 6.0);
        gateSmooth.prepare (sr, 3.0);
        gateSmooth.reset (1.0f);
        for (auto& s : macroSmooth) s.prepare (sr, 25.0);

        crackleRng.seed (0x1234567u);
        humanRng.seed (0xC0FFEEu);

        // Pre-grow + seed the Custom-mode snapshot storage so copying the live
        // curve on the audio thread never allocates, and a first-block lock
        // miss yields musical defaults (live playback, unity gain) not silence.
        customTimeSnapshot.reserve (automation::AutomationEngine::kMaxPoints);
        customVolSnapshot .reserve (automation::AutomationEngine::kMaxPoints);
        customTimeSnapshot.setFlat (0.0f);
        customVolSnapshot .setFlat (1.0f);

        reset();
    }

    void TemporalEngine::reset()
    {
        buffer.reset();
        for (auto& m : modes) if (m) m->reset();
        for (auto& d : declick) d.arm (0.0f);
        for (auto& t : tilt) t.reset();
        for (auto& e : scEnv) e.reset();
        space.reset();
        loopPos = 0.0; loopIndex = 0; anchorAbs = 0; currentLoopLen = 0.0;
        wasPlaying = false; humanizeGain = 1.0f;
        lastWet = { { 0.0f, 0.0f } };
        engageSmoother.reset (0.0f);
        lastPpqSamples = 0.0; lastBlockSamples = 0; havePrevPpq = false;
        lastProcessedMode = requestedMode;
        for (auto& b : visWave) b.store (0.0f, std::memory_order_relaxed);
        visWriteBin.store (0, std::memory_order_relaxed);
        visBinPeak = 0.0f; visBinCount = 0;
    }

    IMode* TemporalEngine::activeMode()
    {
        auto idx = (size_t) requestedMode;
        if (idx >= modes.size() || modes[idx] == nullptr)
            idx = 0;
        return modes[idx].get();
    }

    void TemporalEngine::updateLoopClock (bool playing, double ppqSamples, bool forceResync)
    {
        // Base division length, bounded so speed modes (needing up to 2×) and
        // reverse never read past the recorded window.
        const double spb = automation->getSamplesPerBeat();
        const double divBeats = params::syncDivisionBeats (level2.syncIndex);
        double L = divBeats * spb;
        // Upper bound can dip below 256 at pathological tempo/SR — keep it >= the
        // lower bound so jlimit never asserts (lower > upper).
        const double hiBound = juce::jmax (256.0, juce::jmin (barSamples, windowSamples * 0.5));
        L = juce::jlimit (256.0, hiBound, L);

        // swing skews alternate loops; humanize adds subtle length jitter.
        const double swingSkew = (loopIndex % 2 == 0) ? (1.0 + level2.swing * 0.66)
                                                      : (1.0 - level2.swing * 0.66);
        double target = L * swingSkew;
        if (level2.humanize > 0.0f)
            target *= 1.0 + (humanRng.unipolar() - 0.5f) * 0.08f * level2.humanize;

        // Clamp the FINAL length (after swing/humanize) so Double/Reverse — which
        // read up to 2×loopLen behind live — never exceed the recorded window
        // and freeze on the oldest sample.
        const double maxLen = juce::jmax (256.0, windowSamples * 0.5);
        currentLoopLen = juce::jlimit (256.0, maxLen, target);

        // Hard resync on transport (re)start OR a host locate / loop jump so
        // the pattern grid stays locked to the timeline instead of drifting.
        if (playing && (! wasPlaying || forceResync))
        {
            loopPos   = std::fmod (ppqSamples, currentLoopLen);
            if (loopPos < 0.0) loopPos += currentLoopLen; // negative ppq (host count-in / pre-roll)
            loopIndex = (long long) std::floor (ppqSamples / juce::jmax (1.0, currentLoopLen));
            // Anchor at the loop-START edge (loopPos samples ago), not the current
            // live edge — otherwise an off-grid locate shifts every mode's read by
            // up to one division until the next boundary re-anchors.
            anchorAbs = buffer.getTotalWritten() - 1 - (long long) std::llround (loopPos);
            if (anchorAbs < 0) anchorAbs = 0;
        }
        wasPlaying = playing;
    }

    void TemporalEngine::process (juce::AudioBuffer<float>& audio, bool engaged)
    {
        jassert (automation != nullptr);
        const int numSamples = audio.getNumSamples();
        const int nch = juce::jmin (channels, audio.getNumChannels());

        const auto& t = automation->getTransport();
        barSamples = automation->getPatternBeats() > 0.0
                       ? (automation->getPatternBeats() / (double) level2.bufferBars) * automation->getSamplesPerBeat()
                       : 4.0 * automation->getSamplesPerBeat();

        windowSamples = juce::jmin ((double) buffer.getCapacity() - 32.0,
                                    (double) level2.bufferBars * barSamples);

        // one visualiser bin spans this many input samples
        visSamplesPerBin = juce::jmax (1, (int) (windowSamples / (double) kVisBins));

        for (auto& d : declick) d.setTimeMs (level2.antiClickMs);

        // Detect a host locate / loop jump: while playing, ppq should advance
        // by exactly one block worth of samples. A larger delta means the
        // playhead moved — resync the loop clock so we stay on the grid.
        const double ppqSamples = t.ppqPosition * automation->getSamplesPerBeat();
        bool forceResync = false;
        if (t.isPlaying && havePrevPpq)
        {
            const double expected = lastPpqSamples + (double) lastBlockSamples;
            if (std::abs (ppqSamples - expected) > 64.0)
                forceResync = true;
        }
        updateLoopClock (t.isPlaying, ppqSamples, forceResync);

        // Live mode switch: reset the incoming mode's state (clears stale
        // accumulators) and crossfade so the change never clicks.
        if (requestedMode != lastProcessedMode)
        {
            if (auto* m = activeMode()) m->reset();
            for (int c = 0; c < 2; ++c) declick[(size_t) c].arm (lastWet[(size_t) c]);
            lastProcessedMode = requestedMode;
        }

        IMode* mode = activeMode();
        const bool isCustom = (requestedMode == params::Mode::Custom);

        // Only the Custom mode needs the curves — refresh its snapshots without
        // ever blocking the audio thread (keeps the previous snapshot on the
        // rare block where the editor holds the lock).
        if (isCustom)
        {
            auto* customMode = static_cast<CustomMode*> (modes[(size_t) params::Mode::Custom].get());
            automation->tryCopyTimeCurve   (customTimeSnapshot);
            automation->tryCopyVolumeCurve (customVolSnapshot);
            customMode->setCurves (&customTimeSnapshot, &customVolSnapshot);
        }

        // ducker coefficients depend only on block params — compute once here
        if (level2.scSource != 0 && level2.duck > 0.0001f)
            scEnv[0].setTimes (level2.duckAtkMs, level2.duckRelMs);

        // channel pointers
        float* ch[2] = { nch > 0 ? audio.getWritePointer (0) : nullptr,
                         nch > 1 ? audio.getWritePointer (1) : nullptr };

        float frameIn[2]  = { 0.0f, 0.0f };
        float frameWet[2] = { 0.0f, 0.0f };

        for (int n = 0; n < numSamples; ++n)
        {
            // --- smoothed macros ---
            const float mTime    = macroSmooth[0].process (macros.time);
            const float mDepth   = macroSmooth[1].process (macros.depth);
            const float mMix     = macroSmooth[2].process (macros.mix);
            const float mTexture = macroSmooth[3].process (macros.texture);
            const float mSpace   = macroSmooth[4].process (macros.space);
            const float mWidth   = macroSmooth[5].process (macros.width);

            // --- read + record input (sanitised: a single NaN/Inf from an
            //     upstream plugin would otherwise latch the Space feedback
            //     loop forever; ScopedNoDenormals does not cover NaN/Inf) ---
            for (int c = 0; c < nch; ++c)
            {
                const float v = ch[c] ? ch[c][n] : 0.0f;
                frameIn[c] = std::isfinite (v) ? v : 0.0f;
            }
            if (nch == 1) frameIn[1] = frameIn[0];
            buffer.write (frameIn);

            // publish the peak envelope one bin at a time (lock-free, GUI-read)
            const float visMono = 0.5f * (frameIn[0] + frameIn[1]);
            visBinPeak = juce::jmax (visBinPeak, std::abs (visMono));
            if (++visBinCount >= visSamplesPerBin)
            {
                const int wb = visWriteBin.load (std::memory_order_relaxed);
                visWave[(size_t) wb].store (visBinPeak, std::memory_order_relaxed);
                visWriteBin.store ((wb + 1) % kVisBins, std::memory_order_relaxed);
                visBinPeak = 0.0f;
                visBinCount = 0;
            }

            // --- loop clock advance / boundary ---
            bool loopReset = false;
            loopPos += 1.0;
            if (! isCustom && loopPos >= currentLoopLen)
            {
                loopPos -= currentLoopLen;
                ++loopIndex;
                anchorAbs = buffer.getTotalWritten() - 1;
                loopReset = true;
                if (level2.humanize > 0.0f)
                    humanizeGain = 1.0f - (humanRng.unipolar()) * 0.12f * level2.humanize;
                // recompute next loop length (swing alternation); never resync
                // mid-block on a boundary — that is handled once at block start.
                updateLoopClock (t.isPlaying, ppqSamples, false);
                for (int c = 0; c < nch; ++c) declick[(size_t) c].arm (lastWet[(size_t) c]);
            }

            // --- pattern phase for custom + visualiser ---
            const double phase = isCustom ? std::fmod (automation->blockStartPhase()
                                                       + (double) n * automation->phaseIncrementPerSample(), 1.0)
                                          : (loopPos / juce::jmax (1.0, currentLoopLen));

            // --- run mode ---
            ModeContext mc;
            mc.buffer = &buffer;
            mc.totalWritten = buffer.getTotalWritten();
            mc.anchorAbs = anchorAbs;
            mc.localSamples = loopPos;
            mc.loopLenSamples = currentLoopLen;
            mc.loopReset = loopReset;
            mc.phase = phase;
            mc.time = mTime; mc.depth = mDepth;
            mc.smartFade = level2.smartFade;
            mc.sampleRate = sampleRate;
            mc.samplesPerBeat = automation->getSamplesPerBeat();
            mc.windowSamples = windowSamples;
            mc.quality = level2.quality;

            mode->process (mc, frameWet);

            // --- anti-click on loop boundaries ---
            for (int c = 0; c < nch; ++c)
                frameWet[c] = declick[(size_t) c].process (frameWet[c]);

            // --- humanize gain ---
            if (level2.humanize > 0.0f)
                for (int c = 0; c < nch; ++c) frameWet[c] *= humanizeGain;

            // --- texture: saturation + tilt (+ vinyl crackle) ---
            if (mTexture > 0.0001f)
            {
                for (int c = 0; c < nch; ++c)
                {
                    float s = saturate (frameWet[c], mTexture * 0.8f);
                    s = tilt[(size_t) c].process (s, (mTexture - 0.5f) * 0.6f);
                    frameWet[c] = s;
                }
                if (requestedMode == params::Mode::Vinyl)
                {
                    const float crackle = (crackleRng.unipolar() < 0.002f * mTexture)
                                            ? (crackleRng.unipolar() - 0.5f) * mTexture * 0.5f : 0.0f;
                    for (int c = 0; c < nch; ++c) frameWet[c] += crackle;
                }
            }

            // --- synced gate (volume gating) ---
            if (level2.gate > 0.0001f)
            {
                // recomputed per-sample so a mid-block loop boundary (which can
                // change currentLoopLen via swing) keeps the gate grid aligned.
                const float gateDiv = juce::jmax (256.0f, (float) (currentLoopLen / 4.0));
                const float gp = std::fmod ((float) loopPos, gateDiv) / gateDiv;
                const float gTarget = gp < 0.5f ? 1.0f : (1.0f - level2.gate);
                const float g = gateSmooth.process (gTarget); // de-zippered
                for (int c = 0; c < nch; ++c) frameWet[c] *= g;
            }
            else
            {
                gateSmooth.reset (1.0f); // avoid a jump when the gate re-engages
            }

            // --- stereo width (mid/side) ---
            if (nch == 2)
            {
                const float mid  = 0.5f * (frameWet[0] + frameWet[1]);
                const float side = 0.5f * (frameWet[0] - frameWet[1]) * (mWidth * 2.0f);
                frameWet[0] = mid + side;
                frameWet[1] = mid - side;
            }

            // --- space / ambience ---
            if (mSpace > 0.0001f)
                space.process (frameWet, mSpace);

            lastWet[0] = frameWet[0]; lastWet[1] = frameWet[1];

            // --- internal sidechain duck ---
            float duckGain = 1.0f;
            if (level2.scSource != 0 && level2.duck > 0.0001f)
            {
                const float src = (level2.scSource == 1) ? frameWet[0] : frameIn[0];
                const float e = scEnv[0].process (src);
                duckGain = 1.0f - level2.duck * juce::jmin (1.0f, e * 4.0f);
            }

            // --- engage crossfade (MIDI trigger) ---
            const float eg = engageSmoother.process (engaged ? 1.0f : 0.0f);
            const float wetMix = mMix * eg;

            // --- final dry/wet ---
            for (int c = 0; c < nch; ++c)
            {
                const float dry = frameIn[c];
                const float wet = frameWet[c] * duckGain;
                const float outv = dry * (1.0f - wetMix) + wet * wetMix;
                ch[c][n] = std::isfinite (outv) ? outv : 0.0f; // never emit NaN/Inf
            }

            if (isCustom && ! t.isPlaying) automation->advanceFreePhase();
        }

        visPhase.store (isCustom ? automation->blockStartPhase()
                                 : (loopPos / juce::jmax (1.0, currentLoopLen)),
                        std::memory_order_relaxed);
        visDelay.store (windowSamples > 0.0 ? (buffer.getTotalWritten() - 1 - anchorAbs) / windowSamples : 0.0,
                        std::memory_order_relaxed);

        // remember where the playhead was so we can spot a locate next block
        lastPpqSamples = ppqSamples;
        lastBlockSamples = numSamples;
        havePrevPpq = t.isPlaying;
    }
}
