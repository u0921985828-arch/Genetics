# CHRONA — architecture

The codebase enforces a strict separation of concerns. Each subsystem is
independent and communicates through narrow interfaces, so any one of them can
be swapped or extended without touching the others.

```
Source/
├── PluginProcessor.*      Host boundary. Thin: marshals transport / params /
│                          MIDI / audio into the subsystems. No DSP here.
├── PluginEditor.h         Zoomable (100–300%) HiDPI window; hosts the panels.
│
├── params/                Single source of truth for automatable state.
│   └── Parameters.*       APVTS layout + enums + sync-division maths.
│
├── dsp/                   Real-time audio. Never allocates after prepare().
│   ├── CircularBuffer.h   Shared ring (up to 2 bars), fractional reads.
│   ├── Interpolation.h    Linear / Hermite / windowed-sinc readers.
│   ├── AntiClick.h        DeclickRamp (equal-power) + SmartFade windowing.
│   ├── DspUtils.h         Smoothers, envelope follower, saturation, tilt.
│   ├── Space.h            Compact Schroeder ambience (the "Space" macro).
│   ├── Modes.h            The 8 algorithmic modes + Custom (curve) mode.
│   └── TemporalEngine.*   Orchestrator: buffer + modes + loop clock (swing/
│                          humanize) + colour/dynamics chain + mix.
│
├── automation/            Time-based automation (NOT host automation).
│   ├── Curve.h            Breakpoint curve model with grid snap + serialise.
│   └── AutomationEngine.h Musical phase clock + lock-free curve hand-off.
│
├── midi/                  Performance triggering.
│   └── MidiTriggerSystem.h  Hold / Latch / Momentary engage logic.
│
├── presets/               State persistence.
│   └── PresetManager.*    Full-state capture/apply + factory + user bank.
│
├── ui/                    Presentation only. No DSP knowledge.
│   ├── Theme.h            Palette (#171717/#222222/#2E2E2E + electric blue).
│   ├── ChronaLookAndFeel.h  LED knobs, flat combos/buttons.
│   ├── BigKnob.h          Macro knob bound to an APVTS parameter.
│   ├── ModeSelector.h     Touch mode grid.
│   └── Level1Panel.h / Level2Panel.h  The two UX levels.
│
└── visualizer/            Rendering of engine state (message thread, 60 FPS).
    ├── WaveformDisplay.h  Buffer + read position + "time behind live".
    └── CurveEditor.h      The Level-2 breakpoint editors.
```

## Data flow per block

```
   Host                          PluginProcessor                     Subsystems
 ────────                     ────────────────────                 ──────────────
 transport  ───────────────▶  updateTransport() ───────────────▶  AutomationEngine
 parameters ───────────────▶  pullParameters()  ───────────────▶  TemporalEngine (macros, L2)
 MIDI       ───────────────▶  MidiTriggerSystem ── engage bool ─▶  TemporalEngine
 audio      ───────────────▶  engine.process(buffer, engaged) ──▶  out

 TemporalEngine reads: AutomationEngine (phase + live curves), CircularBuffer.
 Visualiser reads (message thread): engine.getPlayheadPhase / ReadDelayNorm /
                                    getBuffer(); AutomationEngine curve snapshots.
```

## The unifying DSP idea

One shared **circular buffer** continuously records the input. Every mode is
just a rule for choosing a **read position** ("where on the recorded timeline am
I playing from") each sample:

- **Half / Double / Reverse** — read advances at 0.5× / 2× / −1× within a synced
  loop, so they are rhythmic and never drift.
- **Tape Stop** — read speed decays to zero (motor spin-down curve).
- **Stutter / Beat Repeat** — a captured slice repeats, each slice windowed by
  `SmartFade` so edges never click.
- **Vinyl** — near-live playback with wow/flutter and optional crackle.
- **Glitch** — per-slice stochastic reverse / pitch / gate (deterministic LCG).
- **Time Warp** (the curve mode) — the read delay and gain follow the editable
  **Time** and **Volume** curves, sampled by pattern phase. Virtual scratch and
  free time-warp gestures are just curve shapes (see the factory presets).

Loop boundaries crossfade through `DeclickRamp`; the whole path is anti-click by
construction.

## Thread-safety

- **Audio thread**: only reads parameters (cached atomics), reads the currently
  published curve snapshot, and reads/writes the ring buffer. No locks, no
  allocation after `prepareToPlay`.
- **Message thread**: edits curves and publishes them into `AutomationEngine`
  under a `SpinLock`. The audio thread only ever *tries* that lock
  (`tryCopyTimeCurve`/`tryCopyVolumeCurve`); on the rare block where the editor
  holds it, the audio thread keeps its previous snapshot rather than blocking —
  no priority inversion, and the editor can never realloc a vector the audio
  thread is mid-copy of. Snapshot vectors are pre-reserved so the copy never
  allocates. The visualiser only reads, and skips frames while the buffer is
  being (re)allocated (`CircularBuffer::isReady`).
