# CHRONA factory preset banks

**20 libraries × 40 presets = 800 presets.** Each folder here is a *bank* with
its own sonic identity; presets inside a bank share that character but each has
its own personality, and the difference *between* banks is deliberate and clear.

| Bank | Territory |
|---|---|
| Halftime Heavy | Slow half/double-time weight (trap / hip-hop) |
| Glitch Circuits | Digital stutter/glitch, tight and surgical |
| Tape Nostalgia | Warm vinyl + tape spin-down |
| Frozen Cathedrals | Freeze into huge stereo space |
| Granular Clouds | Evolving grain textures |
| Beat Repeat Mangler | Rhythmic beat-repeat chops |
| Reverse Worlds | Reverse swells and suck-backs |
| Time Warp Motion | Curve-drawn time motion |
| Stutter Gate | Gated rhythmic stutters |
| Vinyl Dust | Lo-fi vinyl crackle |
| Ambient Drift | Slow frozen/granular pads |
| Percussive Chops | Fast, tight slice chops |
| Riser FX | Reverse/tape build-ups |
| Downlifter FX | Tension drops |
| Lo-Fi Bedroom | Cozy vinyl warmth |
| Cinematic Space | Dramatic frozen space |
| IDM Textures | Experimental glitch/granular |
| Rhythmic Sync | Swung, humanised grooves |
| Warp Leads | Melodic curve warping |
| Extreme Mangle | Chaotic destruction |

## Install
Copy the bank folders into CHRONA's preset directory (they show up grouped as
`Bank / Preset` in the browser):

- **macOS:** `~/Library/Application Support/CHRONA/Presets`
- **Windows:** `%APPDATA%\CHRONA\Presets`
- **Linux:** `~/.config/CHRONA/Presets`

Convenience installers: `scripts/install-presets.sh` (macOS/Linux) or
`scripts/install-presets.bat` (Windows). The Windows setup installer also seeds
them automatically.

## Regenerate
These files are produced deterministically by `scripts/generate_presets.py`
(re-running yields byte-identical output). Edit the library profiles there to
retune a bank or add new ones.
