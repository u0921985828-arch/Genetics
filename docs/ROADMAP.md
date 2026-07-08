# CHRONA — roadmap

The architecture was chosen so the following expansions slot in without
disturbing existing subsystems. Each is a new `IMode` and/or a new visualiser,
reusing the shared `CircularBuffer`, `AutomationEngine`, preset and UI plumbing.

## Near term
- **Freeze** — latch the ring contents and loop indefinitely (a `SpeedMode`
  variant with speed 0 and a captured anchor). Trivial addition.
- **Sidechain input bus** — expose the internal sidechain as a real side bus
  (add a bus to `makeBuses()`, feed `EnvelopeFollower` from it).
- **Sample-accurate MIDI trigger** — split blocks at note events instead of the
  current per-block engage evaluation.

## Mid term
- **Granular** — a `GranularMode : IMode` that scatters grains read from the
  ring; grain params ride the same Time/Depth macros and the curve engine.
- **Spectral time-stretch** — an FFT phase-vocoder mode; the buffer already
  provides the history, add an analysis/synthesis stage behind a new mode.

## Longer term
- **AI pattern generation** — a message-thread module that writes `Curve`
  breakpoints (Time + Volume) from a prompt/groove model and publishes them via
  the existing lock-free hand-off. No DSP changes required.
- **Modulation matrix** — LFOs/env-followers routed to any APVTS parameter.

## Extension checklist (adding a mode)
1. Implement `dsp::IMode` in `Modes.h` (or a new file under `dsp/modes/`).
2. Add an entry to `params::Mode` and `params::modeNames()`.
3. Construct it in `TemporalEngine`'s modes array.
4. (Optional) add a factory preset in `PresetManager::buildFactoryBank()`.

Everything else — automation, presets, UI binding, visualiser — picks it up for
free.
