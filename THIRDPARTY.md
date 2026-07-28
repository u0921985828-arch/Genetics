# Third-party components

CHRONA bundles / links the following third-party software. Their licenses must
be honoured in any distribution (see `LICENSE` for how this constrains CHRONA's
own license).

| Component | Version | Use | License | Notes |
|---|---|---|---|---|
| [JUCE](https://juce.com) | 8.0.4 (pinned in `CMakeLists.txt`) | Framework: audio/plugin hosting, DSP, GUI, WebView | GPLv3 **or** JUCE commercial | Dual-licensed. Your CHRONA distribution license must match the JUCE tier you build under. `JUCE_DISPLAY_SPLASH_SCREEN=0` is set — this is only permitted under a paid JUCE license (or you must re-enable it for a GPL build). |
| Inter (font) | — | Optional UI typeface referenced by the WebView CSS | SIL OFL 1.1 | Only if you embed the font file; the CSS currently falls back to system fonts, so no font is shipped by default. |

No other third-party source is vendored. JUCE itself is fetched at configure
time by CMake (`FetchContent`) and is **not** committed to this repository.

## Attribution

CHRONA is an original design and implementation. It is *conceptually* inspired
by classic time-manipulation effects but contains no borrowed code, UI assets,
presets, or algorithms from any third-party plugin.
