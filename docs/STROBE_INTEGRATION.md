# CHRONA inside STROBE — integration architecture (design, not yet built)

> Purpose: define exactly how the CHRONA temporal effect plugs into the STROBE
> web DAW (`strobe404.html`) **before** writing any code, so we agree on the
> shape first. Nothing here is implemented yet.

## The core constraint

STROBE is a **Web Audio** app (AudioContext + AudioWorklet, single self-contained
HTML). The native CHRONA VST3/AU is a compiled binary and **cannot** be loaded
in a browser. So "CHRONA in STROBE" means a **Web Audio port** of CHRONA's DSP —
an `AudioWorkletProcessor` — wired into STROBE's existing graph. The native VST
stays as-is for native DAWs; the two share the *design*, not the binary.

Two ways to get the DSP into the worklet (decision deferred, see “Open choice”):
- **A — JS port:** reimplement the DSP (circular buffer, modes, curves, gate/
  duck, anti-click) in JavaScript inside the worklet. Fits STROBE's single-file,
  no-toolchain ethos. Fastest to ship. Sound matches the VST *design*.
- **B — WASM:** decouple the C++ DSP from JUCE, compile with Emscripten, run the
  exact same code in the worklet. Bit-identical to the VST, but adds a build
  toolchain and an embedded WASM blob (heavier for a one-file app).

Recommendation: **A (JS port)** for STROBE; keep B as a later option if we want
guaranteed parity.

---

## 1. Where it sits in STROBE's audio graph

STROBE builds one engine per context in `buildEngine(actx)` (shared by the live
context and the offline export `OfflineAudioContext`, so anything added there
works in export too). The per-track insert chain today is:

```
insertIn → eqLo → eqMid → eqHi → insertDrive → insertFilter → [panner]
        → channelGain → duckGain → outGain → (bus | masterGain)
                              duckGain → reverbSendGain → reverbBus
insertIn → analyser
```

CHRONA becomes an **insert node** in this chain. Proposed point: right after
`insertFilter`, before the panner, so it processes the post-EQ/drive/filter
signal in stereo:

```
… insertFilter → [chronaNode] → [panner] → channelGain → …
```

`chronaNode` is an `AudioWorkletNode` stored on the per-track `chain` object
(`chain.chrona`). When a track has CHRONA disabled, the node is bypassed by
reconnecting `insertFilter → panner` directly (or the worklet passes dry through
at `mix=0`). Because `buildEngine` is shared, export bounces get the effect for
free — we only must ensure the worklet **module** is registered on both contexts.

---

## 2. The worklet module

Mirror STROBE's existing pattern exactly (the envelope-follower worklet):

- `const CHRONA_WORKLET_CODE = \`…class ChronaProcessor extends AudioWorkletProcessor…registerProcessor('chrona', …)\`;`
- `getChronaWorkletUrl()` → `URL.createObjectURL(new Blob([CHRONA_WORKLET_CODE], …))`
- `ensureChronaModule(engine)` → `await engine.ctx.audioWorklet.addModule(url)` guarded by `engine.workletReady`-style flag (`engine.chronaReady`), same as `ensureWorkletModule`.

The DSP port maps 1:1 from the C++ modules:

| C++ (native)                        | Worklet (JS)                              |
|-------------------------------------|-------------------------------------------|
| `dsp/CircularBuffer.h`              | a `Float32Array` ring per channel (2 bars)|
| `dsp/Interpolation.h`               | linear/Hermite read helpers               |
| `dsp/Modes.h` (8 + Custom)          | one `processFrame` switch on `mode`       |
| `dsp/AntiClick.h`                   | declick ramp + smart-fade windows         |
| `automation/Curve.h`                | breakpoint eval for Custom mode           |
| gate / duck / texture / width / space | post chain in `process()`               |

Each track's node owns its own ring buffer + mode state (one processor instance
per track).

---

## 3. Parameters, transport and messaging

- **Continuous params** (time, depth, mix, texture, space, width, swing,
  humanize, gate, duck): declare as `parameterDescriptors` (AudioParams) so
  STROBE's knobs can automate them sample-accurately and they persist like other
  params.
- **Discrete/structured** (mode, bufferBars, sync division, trigger mode,
  quality, the Time/Volume **curves**): send via `node.port.postMessage({...})`
  from the UI; the processor keeps them in fields.
- **Transport / sync (the important part):** the worklet has no host playhead.
  It derives musical phase from `currentFrame / sampleRate` (global in
  `AudioWorkletGlobalScope`) plus:
  - `bpm` (posted whenever STROBE's `bpm` changes),
  - a `transportStartFrame` reference posted on play/stop so bar 1 lines up with
    STROBE's step clock (STROBE already schedules steps off `ctx.currentTime`;
    we post the same origin).
  This reproduces the VST's `AutomationEngine` phase clock. `swingAmount` is
  already global in STROBE and maps to CHRONA's swing.
- **MIDI trigger** modes (Hold/Latch/Momentary): STROBE is step/pad-driven, not
  MIDI-note driven, so map "engage" to a per-track **on/off + performance pad**
  (STROBE's pads) rather than a MIDI note. Same three behaviours.

---

## 4. UI

STROBE renders HTML/CSS panels and binds knobs with its own `bindKnob` helper
(window-level drag, persist on release). CHRONA's panel reuses that:

- A per-channel **CHRONA insert panel** (opened from the mixer's per-channel FX
  affordance — STROBE already has a `mini-btn fx` toggle per channel).
- **Level 1**: six knobs + the mode grid, styled with STROBE's existing tokens
  (the palette differs from CHRONA's native `#171717/#222` skin — we adopt
  STROBE's theme so it feels native to the DAW, not bolted on).
- **Level 2**: the curve editors (canvas), grid/snap, buffer size, gate/duck,
  and the live buffer/playhead view — same components, drawn on `<canvas>`
  instead of JUCE `Graphics`.

No new theming system: CHRONA-in-STROBE inherits STROBE's look (the brief's
`#171717` palette stays with the native VST).

---

## 5. Per-track state & persistence

- Extend the `TRACKS[i]` model with a `chrona` object: `{ on, mode, time, depth,
  mix, texture, space, width, bufferBars, sync, gate, duck, swing, humanize,
  triggerMode, timeCurve:[…], volCurve:[…] }`.
- Fold it into `saveProject`/`applyProjectData` (localStorage) exactly like the
  existing per-track fields; curves serialise as point arrays (same shape as the
  VST's ValueTree breakpoints).
- Undo/redo and the project snapshot history pick it up automatically once it's
  in the track model.

---

## 6. Export / offline

`renderOfflineWav()` builds an `OfflineAudioContext` and calls the same
`buildEngine`. We must `await ensureChronaModule(offlineEngine)` **before**
`startRendering()` (mirroring how sidechain ensures its worklet), and post the
transport origin/bpm to each node up front (offline has a deterministic
`currentFrame`, so phase is exact). Then bounces include CHRONA identically to
live playback.

---

## 7. Risks / things to decide

1. **A vs B (JS port vs WASM)** — pick before implementation. (Recommend A.)
2. **Insert point** — after `insertFilter` (proposed) vs a dedicated pre-fader
   slot. Affects how it interacts with EQ/drive.
3. **Latency** — CHRONA is near-zero latency (circular buffer, no lookahead), so
   no PDC needed; confirm the chosen modes don't add delay that would smear the
   step grid.
4. **CPU** — one ring + interpolation per track at 128-sample quanta; fine for a
   handful of tracks, watch it on many. Sinc quality should be opt-in.
5. **Bar-accurate sync** across play/stop/loop — needs the `transportStartFrame`
   handshake to be rock-solid; this is the main correctness surface.

---

## 8. Phased plan (once approved)

1. **P1 — skeleton:** register the `chrona` worklet, insert a pass-through node
   per track, wire enable/bypass + one macro (Mix). Prove signal path + export.
2. **P2 — core modes:** port CircularBuffer + Half/Double/Reverse/Stutter/Tape
   + the six macros + anti-click; hook the transport handshake.
3. **P3 — rest:** Beat Repeat/Vinyl/Glitch/Custom, curves + Level-2 UI, gate/
   duck (reuse STROBE's envelope worklet pattern), presets.
4. **P4 — polish:** persistence, undo/redo, offline parity, CPU pass.

This mirrors the native module boundaries, so the JS port stays a faithful
translation of the reviewed & sanitizer-tested C++ design.
```
