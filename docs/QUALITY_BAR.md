# CHRONA — Quality Threshold Prompt

A reusable audit prompt. Feed it (with the codebase) to a reviewer or an AI agent
before any release. **A gate PASSES only with executed evidence attached — visual
inspection or "looks fine" is not evidence.** Any 🔴 open → NOT shippable.

---

## Prompt

> You are the release gatekeeper for a commercial VST3/AU audio plug-in, held to
> the standard of a boutique DSP house (the bar UAD / Output / Spectrasonics /
> AIR engineers apply). Audit the plug-in against every gate below. For each,
> return **PASS / FAIL / N-A**, the **evidence** (command + result, not prose),
> and severity of any gap. Do not rationalize a gap away; if you cannot verify a
> gate, mark it **UNVERIFIED** and say why. Be adversarial. Zero complacency.

### 1 · Real-time safety 🔴
- No heap alloc / lock / syscall / unbounded work on the audio thread after `prepareToPlay`.
- No per-sample transcendental that could be per-block.
- Evidence: ThreadSanitizer run (0 races) + code trace of every audio-thread allocation site.

### 2 · Numerical robustness 🔴
- No NaN/Inf reaches the output; input from the host is sanitized.
- No divide-by-zero at any tempo (incl. BPM 0), sample rate (8 k–192 k), or block size.
- Feedback paths bounded (< 1.0 loop gain); denormals flushed (`ScopedNoDenormals`).
- Evidence: ASan+UBSan stress run across all modes × {44.1,48,96 k} × {16…2048} with random params/transport/MIDI; assert output finite & bounded.

### 3 · Host validation 🔴
- pluginval **strictness 10** passes (state, automation, threads, editor, fuzz), VST3 **and** AU.
- Evidence: pluginval log, exit 0.

### 4 · Anti-aliasing 🔴
- Nonlinearities (saturation/drive) anti-aliased (oversampling or ADAA).
- Pitch-up / re-sample paths use a band-limited reader (no imaging).
- Evidence: null/FFT test or code trace of every nonlinearity and repitch site.

### 5 · State & automation 🟠
- Full state round-trips (params + custom data); version-guarded against future formats.
- Every audible parameter is automatable and smoothed (no zipper); bypass is a real `getBypassParameter`.
- Evidence: save→load equality test; automation sweep under ASan.

### 6 · Multi-instance & lifecycle 🟠
- No mutable global/static state (N instances independent).
- Editor open/close during processing / automation / preset load never crashes (SafePointer on async).
- Evidence: `grep` for non-const static; pluginval multi-instance + editor tests.

### 7 · Channel / bus / latency 🟠
- Mono and stereo both correct; unsupported layouts rejected cleanly.
- `getLatencySamples()` is truthful (report it if you add look-ahead).

### 8 · Host-readable parameters 🟠
- DAW generic panel shows real units ("50 %", "3.0 ms", "+25 %"), not raw floats.
- Evidence: parameter text dump.

### 9 · Accessibility 🟠
- Every control has an accessible name (screen-reader) and keyboard focus; contrast ≥ WCAG-AA.

### 10 · Performance 🟡
- CPU per instance is sane at the smallest block size and highest quality; no pathological mode.
- Evidence: measured ns/sample per mode or DAW CPU meter.

### 11 · Distribution (non-code) 🟡
- macOS code-signed + notarized; Windows signed; installers (pkg / InnoSetup);
  preset library format; version/build stamped; EULA + branding.

---

## Verdict rule
`SHIPPABLE ⇔ all 🔴 PASS ∧ no 🟠 FAIL without a signed-off waiver ∧ every gate has evidence.`
Report: table of gate → verdict → evidence → severity, then the one-line verdict.
