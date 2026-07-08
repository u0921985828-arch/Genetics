# CHRONA — temporal manipulation (VST3 / AU / Standalone)

A modern time-warp effect: the punch of buffer-based beat repeat, the speed of
one-knob half/double-time, and a clean, minimal interface. Conceptually inspired
by classic time effects, but an **original design and implementation** — no
borrowed UI, no borrowed code.

> **UX goal:** a musical result in under 5 seconds. Pick a mode, turn a knob.
> The advanced curve editing is there when you want it, never in the way.

---

## Build it (turnkey)

Everything is configured so you **just compile**. JUCE is downloaded
automatically by CMake on the first configure — you don't install it yourself.

### Prerequisites (once)

- **CMake ≥ 3.22** and a **C++17 compiler**
  - macOS: Xcode Command Line Tools (`xcode-select --install`)
  - Windows: Visual Studio 2022 + "Desktop development with C++"
  - Linux: `gcc`/`clang` plus the dev libs listed in `build.sh`
- Internet access on the **first** build (to fetch JUCE).

### One command

```bash
# macOS / Linux
./build.sh

# Windows
build.bat
```

or manually, on any platform:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

### Where the plugin lands

| Format      | Path                                                            |
|-------------|-----------------------------------------------------------------|
| VST3        | `build/CHRONA_artefacts/Release/VST3/CHRONA.vst3`               |
| AU (macOS)  | `build/CHRONA_artefacts/Release/AU/CHRONA.component`            |
| Standalone  | `build/CHRONA_artefacts/Release/Standalone/CHRONA(.app/.exe)`   |

On macOS the bundles are also copied into `~/Library/Audio/Plug-Ins`
automatically (`COPY_PLUGIN_AFTER_BUILD`). Rescan in your DAW and load **CHRONA**
(manufacturer *Genetics Audio*).

Useful options: `-DCHRONA_BUILD_STANDALONE=OFF`, `-DCHRONA_JUCE_TAG=8.0.4`.

---

## Using it

### Level 1 — Simple (always visible)
Six macro knobs and a mode grid:

- **Time** – musical rate / warp amount (mode-dependent)
- **Depth** – effect intensity
- **Mix** – dry/wet
- **Texture** – saturation / vinyl grit / tone tilt
- **Space** – ambience tail
- **Width** – stereo width

Modes: **Half · Double · Reverse · Tape Stop · Stutter · Beat Repeat · Vinyl ·
Glitch** (plus **Custom (PAT)** for the curve editor).

### Level 2 — Advanced (drops down via **ADVANCED**)
- Editable **Time** and **Volume** curves (click to add, drag to move, double
  click/right click to delete), with **grid snap**.
- **Grid** division, **Buffer** length (1 or 2 bars), interpolation **Quality**.
- **MIDI Trigger**: Hold / Latch / Momentary, on a chosen note.
- **Sidechain / Duck**, synced **Gate**, **Swing**, **Humanize**, **Smart Fade**,
  **Anti-Click**.
- Real-time **buffer + playhead + waveform** visualiser.
- **Preset** browser (factory + user) and Save.

The window is HiDPI-crisp and **zooms 100 %–300 %** — drag any corner to resize.

---

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the module layout and how
the pieces fit together, and [`docs/ROADMAP.md`](docs/ROADMAP.md) for the planned
granular / freeze / spectral / AI expansions the codebase is structured for.
