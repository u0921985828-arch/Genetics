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
  - Linux: `gcc`/`clang` — the JUCE dev libraries are installed **for you** by
    `build.sh` (apt / dnf / pacman / zypper).
- Internet access on the **first** build (to fetch JUCE).

### One command — start to finish

```bash
# macOS / Linux — checks the toolchain, installs Linux deps, fetches JUCE,
# then configures and builds. Nothing else to set up.
./build.sh                 # Release  (./build.sh Debug for a debug build)

# Windows
build.bat
```

Escape hatches: `CHRONA_SKIP_DEPS=1 ./build.sh` skips the dependency install,
and `CHRONA_CMAKE_ARGS="-DCHRONA_LTO=OFF" ./build.sh` forwards extra CMake flags.

Or drive CMake yourself on any platform (deps must already be present):

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
(manufacturer *Anonymous*).

Useful options:
`-DCHRONA_BUILD_STANDALONE=OFF`, `-DCHRONA_JUCE_TAG=8.0.4`,
`-DCHRONA_COPY_AFTER_BUILD=OFF` (don't install into system plug-in folders — use
on CI), `-DCHRONA_BUILD_TESTS=ON -DCHRONA_SANITIZE=ON` (build the ASan+UBSan
stress test), `-DCHRONA_TSAN=ON` (ThreadSanitizer build),
`-DCHRONA_WEBVIEW=ON` (use the HTML/JS **WebView** editor from `WebUI/` instead
of the native UI — needs a system WebView: WebView2 on Windows, WebKitGTK+GTK3
on Linux `sudo apt install libwebkit2gtk-4.1-dev libgtk-3-dev`; the native UI is
the default and dependency-free). The WebView editor is full-featured: six
macro knobs, the mode grid, a preset-bank dropdown, A/B compare, live I/O
meters, the buffer visualiser with playhead, a Randomize dice, and — in Time
Warp mode — an editable **Time/Volume curve editor** with node/bend handles
and a Warp **Rate** selector.

CMake is the single source of truth for this project — it drives the plug-in
build, the CI/validator flow, and the sanitizer/test targets. Any IDE that opens
a CMake project (CLion, VS, VS Code, Xcode via `-G Xcode`) works directly.

### Validation & tests

```bash
# pluginval — the industry-standard validator (state, threads, params, fuzzing).
# Uses an installed pluginval, or downloads the right prebuilt for your OS.
cmake --build build --target validate
#   -DPLUGINVAL_LEVEL=5           # strictness 1..10 (default 10)
#   -DPLUGINVAL_EXE=/path/to/pluginval   # use an existing binary
#   -DPLUGINVAL_ARGS=--skip-gui-tests    # headless CI (or run under xvfb)

# Sanitizer stress test (all modes × sample rates × block sizes, randomised
# transport/params/MIDI + a concurrent curve-editing thread):
cmake -B build -DCHRONA_BUILD_TESTS=ON -DCHRONA_SANITIZE=ON   # ASan + UBSan
cmake --build build --target ChronaTests && ./build/ChronaTests_artefacts/*/ChronaTests
#   -DCHRONA_TSAN=ON                                          # ThreadSanitizer
```

The engine has been verified clean under ASan+UBSan and ThreadSanitizer (0 data
races) across all modes; `validate` adds the DAW-host-level checks.

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
Glitch** (plus **Time Warp** — the curve-driven mode for the Level-2 editor).

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
