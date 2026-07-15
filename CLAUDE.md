# Genetics repo — CLAUDE.md

## ⚠️ Naming note

This repository is named `Genetics` on GitHub, but its actual content is a
**browser-based step-sequencer / DAW** ("STROBE // step engine"), unrelated
to biological or in-game genetics. There is nothing genetics-themed here —
this note exists purely so future work isn't misdirected by the repo name.
(Not to be confused with the `Strainmon` repo, whose CLAUDE.md documents an
actual genetics-simulation game engine — different repo, different content.)

## What this is

`strobe404.html` is a **single self-contained HTML file** (~5,900 lines: CSS
in a `<style>` block, JS in a bottom `<script>` block, no external assets, no
build step) implementing an FL-Studio-inspired mobile-first music production
app:

- Title: **"STROBE // step engine"**.
- Step sequencer (channel rack + grid), piano roll, playlist/arrangement view
  (FL-style free clips + automation lanes), mixer (channel strips, 3-band EQ,
  drive, sub-mix bus routing A/B, LED peak meters, VU/spectrum metering),
  per-channel FX rack, MPC-style pads view, live mic recording, WAV export.
- Runs entirely client-side via the **Web Audio API** (`AudioContext`,
  `OfflineAudioContext` for bounce/export, an `AudioWorklet` for the
  side-chain ducking envelope follower).
- Built as a **mobile WebView/PWA target** — no native filesystem save step,
  so persistence is autosave-only (see below), and there's an in-app modal
  system replacing native `confirm`/`prompt`/`alert` (WebViews render those
  inconsistently).

## How to run / test

- No server, no build, no package manager, no dependencies: open
  `strobe404.html` directly in a browser.
- No test suite, linter, or typecheck configured. Verify changes by opening
  the file and exercising the UI manually (transport, rack, playlist, mixer,
  piano roll, pads, export) — there is no automated check to run instead.

## Architecture (single file, organized by comment-delimited sections)

There are no modules/imports — everything lives in one top-level IIFE at the
bottom of the file, closed over shared state. Navigate by searching for the
`/* =========================================================` section
banners; approximate line numbers (may drift as the file grows):

**CSS (`<style>`, top of file):**
| Section | ~Line |
|---|---|
| Palette (light "C40" + dark flat theme, opt-in via `data-theme`) | 10, 33 |
| Top toolbar | 79 |
| Menu sheet | 144 |
| Workspace | 182 |
| Channel rack | 219 |
| Sound/channel | 269 |
| Playlist | 294 |
| Mixer | 341 |
| Pads | 436 |
| Overlays | 446 |

**JS (`<script>`, bottom of file):**
| Section | ~Line | Notes |
|---|---|---|
| Subdivision/BPM-sync math | 904 | |
| State | 939 | `TRACKS`, pattern/step model, grid resolution (incl. triplets) |
| Project templates | 1061 | Default demo beat vs. empty/free tracks |
| Project persistence | 1224 | `localStorage` autosave/restore (see below) |
| Undo/redo | 1563 | Snapshot history of the whole project JSON |
| Audio engine | 1704 | Web Audio graph setup |
| Side-chain ducking | 1932 | `AudioWorklet` envelope follower |
| Scheduler | 2194 | Lookahead pattern scheduler, locked to BPM subdivisions |
| UI: transport | 2459 | |
| In-app dialogs | 2769 | Custom modal system replacing native confirm/prompt/alert |
| UI: track headers + step grid | 2883 | |
| Dynamic rack | 2889 | Add/remove/reorder channels |
| UI: per-step graph editor | 3159 | Velocity / pan / pitch / repeat lanes |
| UI: sound edit view | 3262 | Includes destructive waveform edits (~3363) |
| Sound library | 3610 | Factory one-shots + user favorites |
| Arrangement/playlist view | 4024 | FL-style free clips, resizable/trim/loop, multi-select marquee |
| Live mic recording | 4518 | Onto the playlist, with latency compensation |
| Piano roll overlay | 4690 | Note move/resize (4817), velocity-lane drag (4869) |
| Mixer view | 4894 | Vertical channel strips, LED peak meters, FL-style pan knob & routing curves |
| Automation editor overlay | 4961 | Breakpoint curves, can automate EQ/drive/mixer params |
| Pads view | 5577 | MPC-style, multi-touch |
| WAV export | 5753 | `OfflineAudioContext` bounce |
| Resize handling | 5860 | |
| Init | 5868 | Renders all views, wires autosave, restores samples/clips from IndexedDB, unlocks audio on first touch |

## Persistence model (important — two different stores)

- **Project JSON** (patterns, steps, clip positions/trim/loop points, track
  names, mixer/automation state) → `localStorage`, key prefix
  `strobe404_*` (project save key, theme, playlist snap, pads/meter mode,
  recording latency, etc.). Small, string-based, autosaved every 4s plus on
  `visibilitychange`/`pagehide`.
- **Sample & audio-clip binary blobs** (recorded audio, imported one-shots)
  → **IndexedDB**, *not* `localStorage`, because raw audio blows past
  `localStorage`'s ~5MB string quota. Restored asynchronously on load via
  `restoreSamples()` / `restoreAudioClips()`.
- When adding a feature that stores new binary/blob data, use IndexedDB, not
  `localStorage`, for the same quota reason.

## Conventions

- English throughout (UI copy, identifiers, comments) — unlike `Strainmon`,
  which is Spanish-first.
- Everything stays in the one file — do not split it into modules or add a
  build step; the whole point is a single portable HTML file with zero
  external dependencies.
- FL Studio's UI vocabulary is the deliberate reference point (Channel Rack,
  Playlist, Piano Roll, Mixer, Pads) — keep new features consistent with
  that mental model rather than inventing new paradigms.
- Native `confirm`/`prompt`/`alert` are banned in favor of the in-app modal
  system (~line 2769) — WebView targets render native dialogs inconsistently.
- Touch/mobile-first: interactions must work with touch (multi-touch pads,
  pinch-to-zoom playlist, long-press menus) as well as mouse.
