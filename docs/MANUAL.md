# CHRONA — User Manual

CHRONA is a temporal-manipulation effect: buffer-based time-warping with a
one-knob-per-idea workflow. Pick a mode, turn a knob, get a musical result in
seconds. The advanced controls are there when you want them, never in the way.

---

## Install
- **macOS:** run the `.pkg` → installs the VST3 to `/Library/Audio/Plug-Ins/VST3`
  and the AU to `…/Components`. Rescan in your DAW.
- **Windows:** run the setup → installs the VST3 to
  `C:\Program Files\Common Files\VST3`.
- Load **CHRONA** (manufacturer **ARTiFACTS**) on any audio track.

---

## The macros (Level 1)
| Knob | What it does |
|---|---|
| **Time** | Musical rate / warp amount — meaning depends on the mode (division, slice size, spin-down length, grain size…). |
| **Depth** | Effect intensity (feedback of the character, repeat decay, grain density/spread, spin curve…). |
| **Mix** | Dry / wet balance. |
| **Texture** | Saturation + tone tilt, plus vinyl surface noise in Vinyl mode. |
| **Space** | Ambience — the built-in stereo FDN reverb tail. |
| **Width** | Stereo width of the wet signal. |

Double-click a knob to reset it; **Shift-drag** for fine control; mouse-wheel to
nudge. Hit the **dice** (WebView UI) to randomise the macros and mode.

## Modes
| Mode | Sound |
|---|---|
| **Half** | Half-time — plays the buffer back at ½ speed on the grid. |
| **Double** | Double-time — ½-window read at 2× (anti-aliased). |
| **Reverse** | Reverses the buffer per division. |
| **Tape Stop** | Tape-style motor spin-down; Time sets length, Depth the curve. |
| **Stutter** | Clean sliced repeat; Time sets the subdivision (1–8). |
| **Beat Repeat** | Beat-locked repeat that *evolves* — per-repeat pitch/reverse variation + decay by Depth. |
| **Vinyl** | Near-live playback with wow/flutter + filtered surface crackle. |
| **Glitch** | Stochastic slice / pitch / reverse / gate, reseeded on the grid. |
| **Time Warp** | Curve-driven time-warp — you draw the motion (see below). |
| **Freeze** | Freezes the incoming sound into an evolving, layered "bloom". |
| **Granular** | A cloud of up to 24 overlapping, pitched, panned grains. |

## Advanced (Level 2)
Open with **ADVANCED** (native UI). Available: **Grid** division, **Buffer**
length (1 or 2 bars), interpolation **Quality** (Linear/Hermite/Sinc),
**Anti-Click**, **Swing**, **Humanize**, **Smart Fade**, synced **Gate**,
**Sidechain / Duck** (Off / Wet / Dry / External), and the **MIDI Trigger**
(note + Hold/Latch/Momentary). A real-time buffer + playhead visualiser sits on
top. The window is HiDPI-crisp and zooms 100 %–300 % (drag a corner).

### The curve editor (Time Warp)
Select **Time Warp** and the buffer view becomes an editable curve:
- **Click** empty space to add a node · **drag** a node to move it · **drag a
  segment handle** to bend it · **double-click** a node to delete it.
- X snaps to 1/16 (hold **Alt** for free).
- Switch the **TIME / VOL** lanes to edit the time-warp motion and the volume
  envelope independently.
- **Rate** sets how long one cycle of the curve lasts (1/4 … 4 bars),
  independent of the recording buffer.

### MIDI trigger
Set **SC/Trigger → Momentary/Latch/Hold** and a note. With no MIDI, the effect
is always on (zero-setup). Triggers are sample-accurate. A quick tap in
Momentary still produces a ~100 ms audible gesture.

## Presets & A/B
Browse the factory + user bank from the preset name (dropdown in the WebView UI,
combo in the native UI). **A/B** keeps two independent snapshots so you can
compare two settings; tweak A, click B, compare. Save your own from the native
UI's **Save**.

### Preset banks
CHRONA ships **20 libraries × 40 presets** (see `presets/`). Each library has a
distinct identity (Halftime Heavy, Glitch Circuits, Tape Nostalgia, Frozen
Cathedrals, Granular Clouds, …) and appears in the browser grouped as
`Bank / Preset`. The Windows installer seeds them automatically; on macOS/Linux
run `scripts/install-presets.sh` (or copy the bank folders into the CHRONA
preset directory — see `presets/README.md`).

---

## Tips
- For clean pitch-shifts on bright material, set **Quality → Sinc**.
- **Freeze + Space** makes instant pads; **Granular + Width** makes clouds.
- Automate **Time** and **Mix** from your DAW for build-ups; everything is a
  host-automatable parameter.

## Support
See `README.md` for building from source and `docs/ARCHITECTURE.md` for how the
engine is put together.
