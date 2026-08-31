# Changelog

All notable changes to CHRONA are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/); versioning is [SemVer](https://semver.org/).

## [1.0.0] — 2026-07-28

First public release.

### Effect
- 11 modes: Half, Double, Reverse, Tape Stop, Stutter, Beat Repeat, Vinyl,
  Glitch, Time Warp (curve-driven), Freeze, Granular.
- Six macro knobs (Time, Depth, Mix, Texture, Space, Width) for a musical
  result in seconds; full advanced panel underneath.
- Editable **Time and Volume curves** with node + segment-bend handles, grid
  snap, and a Warp **Rate** selector (1/4…4 bars) decoupled from the buffer.
- Tempo-synced, sample-accurate MIDI trigger (Hold / Latch / Momentary),
  swing, humanize, synced gate, sidechain/duck, smart fades and anti-click.
- Studio-grade DSP: rate-adaptive band-limited resampling (anti-aliased
  pitch-shifts), an 8-line modulated stereo FDN reverb with pre-delay,
  antiderivative (ADAA) saturation, 24-voice overlap-normalised granular, and
  a layered "bloom" freeze.

### Interface
- Two editors: a dependency-free native UI and an optional boutique **WebView**
  (HTML/JS) UI with machined-flat knobs, a hero buffer visualiser with
  playhead, I/O meters, a preset-bank dropdown, A/B compare and Randomize.

### Quality
- Verified on ubuntu / macOS / windows, clean under ASan+UBSan and
  ThreadSanitizer, and passing **pluginval strictness 10**.
- Audio-quality tests: bypass dry-path transparency, byte-identical state
  round-trip, and silence-in/silence-out stability across all modes.

[1.0.0]: https://github.com/u0921985828-arch/Genetics/releases/tag/v1.0.0
