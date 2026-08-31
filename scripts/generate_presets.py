#!/usr/bin/env python3
# ---------------------------------------------------------------------------
#  CHRONA factory preset-bank generator.
#
#  Produces 20 libraries x 40 presets = 800 .chrona files under presets/<Library>/.
#
#  Design intent (what makes these read as *curated* rather than random):
#    * Each library has a strong identity: its own mode set, macro territory,
#      per-mode voicing (interpolation quality, anti-click, smart-fade, buffer
#      length) and, for Time-Warp banks, hand-shaped motion curves.
#    * Within a library the 40 presets are laid out along a deliberate
#      *intensity arc* (subtle -> extreme) on the macros that define the bank,
#      with a golden-ratio jitter on the remaining macros so neighbours differ
#      on several axes at once — no two feel like duplicates.
#    * Names are descriptive: the adjective tracks the preset's intensity and
#      the whole vocabulary is themed to the bank, so a name evokes its sound.
#    * Deterministic: re-running yields byte-identical output.
#
#  A .chrona file is the XML of the plugin's state ValueTree:
#    <CHRONA version="1" presetName="..">
#      <STATE><PARAM id=".." value=".."/> ...</STATE>
#      <TimeCurve><p x y c/>..</TimeCurve>
#      <VolCurve><p x y c/>..</VolCurve>
#    </CHRONA>
#  Param values are DENORMALISED (choice=index, ms=milliseconds, pct=0..1);
#  the plugin clamps everything on load, so out-of-range is impossible to ship.
# ---------------------------------------------------------------------------
import html, pathlib

OUT = pathlib.Path(__file__).resolve().parent.parent / "presets"

# mode indices (mirror params::Mode) — index 8 is Time Warp (Custom)
HALF, DOUBLE, REVERSE, TAPESTOP, STUTTER, BEATREPEAT, VINYL, GLITCH, WARP, FREEZE, GRANULAR = range(11)

# full default parameter set (denormalised)
DEFAULTS = dict(
    time=0.5, depth=1.0, mix=1.0, texture=0.0, space=0.0, width=0.5,
    mode=HALF, sync=2, bufferBars=1, warpRate=3, snap=1, quality=1,
    antiClick=3.0, swing=0.0, humanize=0.0, smartFade=0.5,
    triggerMode=0, triggerNote=60, gateAmount=0.0, duckAmount=0.0,
    duckAttack=5.0, duckRelease=120.0, scSource=0, bypass=0,
)
INT_PARAMS = {"mode", "sync", "bufferBars", "warpRate", "snap", "quality",
              "triggerMode", "triggerNote", "scSource", "bypass"}

# authoritative bounds (mirror Parameters.cpp) — the generator clamps to these
# so a profile typo can never ship an out-of-range value.
FLOAT_RANGE = dict(
    time=(0.0, 1.0), depth=(0.0, 1.0), mix=(0.0, 1.0), texture=(0.0, 1.0),
    space=(0.0, 1.0), width=(0.0, 1.0), humanize=(0.0, 1.0), smartFade=(0.0, 1.0),
    gateAmount=(0.0, 1.0), duckAmount=(0.0, 1.0), swing=(-0.5, 0.5),
    antiClick=(0.5, 25.0), duckAttack=(0.5, 200.0), duckRelease=(5.0, 800.0),
)
CHOICE_MAX = dict(mode=10, sync=8, bufferBars=1, warpRate=4, snap=1,
                  quality=2, triggerMode=2, scSource=3, bypass=1)

def clampf(key, v):
    lo, hi = FLOAT_RANGE[key]
    return max(lo, min(hi, v))

def clampi(key, v):
    v = int(round(v))
    return max(0, min(CHOICE_MAX[key], v))

GOLDEN = 0.61803398875
def spread(i, lo, hi, phase):
    """Deterministic low-discrepancy value in [lo,hi] for preset i."""
    t = (0.5 + i * GOLDEN + phase) % 1.0
    return lo + (hi - lo) * t

def smooth(t):
    """Smoothstep — softens the ends of the intensity arc."""
    return t * t * (3.0 - 2.0 * t)

# ---------------------------------------------------------------------------
#  Per-mode voicing. These are the choices a sound designer makes once per
#  sound type and rarely revisits: how the mode should be interpolated, how
#  long its crossfades are, how much the smart-fade smooths it. Banks can
#  override any of these in their `extra`.
# ---------------------------------------------------------------------------
MODE_AUX = {
    HALF:       dict(antiClick=6.0,  smartFade=0.62, quality=1),
    DOUBLE:     dict(antiClick=5.0,  smartFade=0.55, quality=2),
    REVERSE:    dict(antiClick=8.0,  smartFade=0.60, quality=1),
    TAPESTOP:   dict(antiClick=6.0,  smartFade=0.50, quality=1),
    STUTTER:    dict(antiClick=1.5,  smartFade=0.12, quality=1),
    BEATREPEAT: dict(antiClick=2.0,  smartFade=0.18, quality=1),
    VINYL:      dict(antiClick=5.0,  smartFade=0.50, quality=1),
    GLITCH:     dict(antiClick=1.0,  smartFade=0.10, quality=2),
    WARP:       dict(antiClick=4.0,  smartFade=0.50, quality=2),
    FREEZE:     dict(antiClick=14.0, smartFade=0.80, quality=1),
    GRANULAR:   dict(antiClick=12.0, smartFade=0.75, quality=2),
}

# ---- Time-Warp motion curves (y in [0,1], c in [-1,1] segment bend) --------
def flat_time(): return [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)]
def flat_vol():  return [(0.0, 1.0, 0.0), (1.0, 1.0, 0.0)]

def warp_curve(kind, amt):
    a = 0.2 + 0.75 * amt          # peak warp depth
    b = 0.35 * a
    shapes = [
        [(0.0, 0.0, 0.40), (1.0, a, 0.0)],                                     # eased rise
        [(0.0, a, -0.40), (1.0, 0.0, 0.0)],                                    # eased fall
        [(0.0, a, 0.0), (0.5, 0.0, 0.30), (1.0, a, -0.30)],                    # smooth V
        [(0.0, 0.0, 0.30), (0.5, a, -0.30), (1.0, 0.0, 0.0)],                  # smooth peak
        [(0.0, 0.0, 0.0), (0.25, a, 0.0), (0.5, b, 0.0), (0.75, a, 0.0), (1.0, 0.0, 0.0)],  # wave
        [(0.0, 0.0, 0.0), (0.33, a * 0.5, 0.0), (0.33, a * 0.5, 0.0),
         (0.66, a, 0.0), (0.66, a, 0.0), (1.0, a, 0.0)],                       # stair (held steps)
        [(0.0, 0.0, -0.50), (1.0, a, 0.0)],                                    # slow-in ramp
        [(0.0, 0.0, 0.0), (0.85, a, 0.45), (1.0, 0.0, 0.0)],                   # saw + release
    ]
    return shapes[kind % len(shapes)]

def vol_curve(kind, amt):
    d = 1.0 - 0.7 * amt
    shapes = [
        flat_vol(),
        [(0.0, 1.0, 0.0), (0.5, d, 0.3), (1.0, 1.0, -0.3)],                    # smooth dip
        [(0.0, 1.0, 0.0), (0.5, 1.0, 0.0), (0.5, d, 0.0), (1.0, d, 0.0)],      # step down
        [(0.0, d, 0.4), (1.0, 1.0, 0.0)],                                      # fade in
    ]
    return shapes[kind % len(shapes)]

# ---------------------------------------------------------------------------
#  Library profiles.
#    modes  : mode set (cycled across the 40)
#    ranges : per-macro (lo,hi) territory
#    arc    : macros whose value follows the intensity arc; +1 rises toward hi
#             with intensity, -1 falls toward lo. Macros not listed get a
#             golden-ratio spread (so they vary independently).
#    adj/nn : themed name pools (adjectives sorted mild -> intense)
#    extra  : fixed/ranged overrides (sync list, quality, gate arc, curves, ...)
# ---------------------------------------------------------------------------
def L(name, modes, ranges, arc, adj, nn, **extra):
    return dict(name=name, modes=modes, ranges=ranges, arc=arc, adj=adj, nn=nn, extra=extra)

R = lambda **kw: kw

LIBRARIES = [
    L("Halftime Heavy", [HALF, DOUBLE],
      R(time=(0.05, 0.35), depth=(0.6, 1.0), mix=(0.85, 1.0), texture=(0.1, 0.45), space=(0.1, 0.35), width=(0.4, 0.6)),
      dict(time=-1, depth=+1, texture=+1),
      ["Slow", "Sunken", "Heavy", "Leaden", "Molasses", "Tar", "Granite", "Colossal"],
      ["Drag", "Weight", "Lurch", "Slump", "Grind", "Haul", "Slab", "Anvil", "Boulder", "Crawl"],
      sync=[1, 2], bufferBars=1),

    L("Glitch Circuits", [GLITCH, STUTTER],
      R(time=(0.4, 0.9), depth=(0.6, 1.0), mix=(0.8, 1.0), texture=(0.2, 0.5), space=(0.0, 0.2), width=(0.5, 0.95)),
      dict(time=+1, depth=+1, width=+1),
      ["Clean", "Wired", "Static", "Jittered", "Fractured", "Severed", "Corrupted", "Overclocked"],
      ["Circuit", "Glitch", "Stutter", "Byte", "Fault", "Relay", "Spike", "Signal", "Chip", "Fracture"],
      sync=[2, 3, 4], quality=2, smartFade=(0.0, 0.22), bufferBars=0),

    L("Tape Nostalgia", [VINYL, TAPESTOP],
      R(time=(0.2, 0.6), depth=(0.4, 0.85), mix=(0.7, 1.0), texture=(0.45, 0.85), space=(0.2, 0.45), width=(0.3, 0.5)),
      dict(texture=+1, depth=+1),
      ["Faded", "Warm", "Worn", "Dusty", "Sepia", "Melted", "Aged", "Bygone"],
      ["Reel", "Spool", "Wobble", "Memory", "Cassette", "Warp", "Fade", "Loop", "Grain", "Echo"],
      bufferBars=1),

    L("Frozen Cathedrals", [FREEZE],
      R(time=(0.3, 0.7), depth=(0.3, 0.8), mix=(0.7, 1.0), texture=(0.0, 0.2), space=(0.6, 1.0), width=(0.6, 1.0)),
      dict(space=+1, depth=+1, width=+1),
      ["Still", "Distant", "Vast", "Glacial", "Cavernous", "Boundless", "Eternal", "Immense"],
      ["Cathedral", "Nave", "Vault", "Halo", "Expanse", "Chamber", "Aurora", "Monolith", "Choir", "Void"],
      bufferBars=1),

    L("Granular Clouds", [GRANULAR],
      R(time=(0.2, 0.85), depth=(0.4, 0.95), mix=(0.6, 1.0), texture=(0.0, 0.3), space=(0.3, 0.7), width=(0.6, 1.0)),
      dict(depth=+1, space=+1),
      ["Soft", "Drifting", "Scattered", "Billowing", "Swirling", "Dense", "Turbulent", "Storming"],
      ["Cloud", "Mist", "Vapor", "Nebula", "Swarm", "Pollen", "Dust", "Cumulus", "Haze", "Drift"],
      bufferBars=1),

    L("Beat Repeat Mangler", [BEATREPEAT, STUTTER],
      R(time=(0.3, 0.7), depth=(0.5, 1.0), mix=(0.85, 1.0), texture=(0.1, 0.4), space=(0.05, 0.25), width=(0.45, 0.75)),
      dict(depth=+1, time=+1),
      ["Tight", "Rolling", "Punchy", "Chopped", "Stuttered", "Mangled", "Shredded", "Ripped"],
      ["Roll", "Chop", "Repeat", "Kick", "Snare", "Break", "Loop", "Mangle", "Groove", "Slam"],
      sync=[2, 3, 4], swing=(0.0, 0.3), bufferBars=0),

    L("Reverse Worlds", [REVERSE],
      R(time=(0.3, 0.7), depth=(0.4, 0.9), mix=(0.6, 1.0), texture=(0.0, 0.3), space=(0.3, 0.7), width=(0.5, 0.9)),
      dict(depth=+1, space=+1),
      ["Gentle", "Backward", "Inverted", "Undertow", "Rewound", "Suction", "Vortex", "Reversed"],
      ["Swell", "Suck", "Reverse", "Undertow", "Tide", "Rewind", "Vacuum", "Backwash", "Bloom", "Pull"],
      bufferBars=1),

    L("Time Warp Motion", [WARP],
      R(time=(0.3, 0.7), depth=(0.5, 1.0), mix=(0.85, 1.0), texture=(0.0, 0.3), space=(0.2, 0.5), width=(0.4, 0.7)),
      dict(depth=+1),
      ["Fluid", "Rubber", "Elastic", "Warping", "Bending", "Twisting", "Contorted", "Melting"],
      ["Motion", "Warp", "Bend", "Ripple", "Wave", "Flux", "Curve", "Sway", "Morph", "Drift"],
      warpRate=[1, 2, 3], curves=True, bufferBars=1),

    L("Stutter Gate", [STUTTER],
      R(time=(0.4, 0.8), depth=(0.4, 0.9), mix=(0.85, 1.0), texture=(0.1, 0.3), space=(0.05, 0.2), width=(0.4, 0.7)),
      dict(gateAmount=+1, depth=+1),
      ["Choppy", "Gated", "Pulsing", "Chattering", "Strobing", "Ratcheted", "Seizing", "Machine"],
      ["Gate", "Stutter", "Pulse", "Chatter", "Ratchet", "Strobe", "Trigger", "Grid", "Chop", "Latch"],
      sync=[3, 4], gateAmount=(0.3, 0.9), bufferBars=0),

    L("Vinyl Dust", [VINYL],
      R(time=(0.2, 0.5), depth=(0.5, 0.9), mix=(0.7, 1.0), texture=(0.5, 0.9), space=(0.1, 0.3), width=(0.3, 0.5)),
      dict(texture=+1, depth=+1),
      ["Dusty", "Crackling", "Scratched", "Grimy", "Worn", "Muddy", "Cracked", "Filthy"],
      ["Dust", "Crackle", "Groove", "Scratch", "Static", "Needle", "Grime", "Pop", "Hiss", "Wax"],
      bufferBars=1),

    L("Ambient Drift", [FREEZE, GRANULAR],
      R(time=(0.2, 0.6), depth=(0.3, 0.7), mix=(0.6, 0.9), texture=(0.0, 0.2), space=(0.5, 1.0), width=(0.7, 1.0)),
      dict(space=+1, width=+1),
      ["Calm", "Floating", "Weightless", "Dreaming", "Distant", "Ethereal", "Suspended", "Infinite"],
      ["Drift", "Pad", "Bloom", "Halo", "Dream", "Aura", "Horizon", "Cloud", "Glow", "Sigh"],
      bufferBars=1),

    L("Percussive Chops", [STUTTER, BEATREPEAT],
      R(time=(0.6, 0.95), depth=(0.6, 1.0), mix=(0.9, 1.0), texture=(0.1, 0.3), space=(0.0, 0.15), width=(0.4, 0.7)),
      dict(time=+1, depth=+1),
      ["Snappy", "Tight", "Sharp", "Punchy", "Rapid", "Jagged", "Frantic", "Relentless"],
      ["Chop", "Snap", "Hit", "Slice", "Dice", "Stab", "Cut", "Ratchet", "Roll", "Jab"],
      sync=[4], smartFade=(0.0, 0.18), bufferBars=0),

    L("Riser FX", [REVERSE, TAPESTOP],
      R(time=(0.4, 0.8), depth=(0.5, 1.0), mix=(0.7, 1.0), texture=(0.0, 0.3), space=(0.3, 0.6), width=(0.5, 0.9)),
      dict(depth=+1, space=+1),
      ["Rising", "Building", "Soaring", "Ascending", "Surging", "Climbing", "Escalating", "Skybound"],
      ["Riser", "Uplift", "Surge", "Climb", "Ascent", "Build", "Lift", "Rush", "Sweep", "Launch"],
      bufferBars=1),

    L("Downlifter FX", [TAPESTOP, HALF],
      R(time=(0.05, 0.4), depth=(0.6, 1.0), mix=(0.8, 1.0), texture=(0.1, 0.4), space=(0.3, 0.6), width=(0.4, 0.6)),
      dict(time=-1, depth=+1),
      ["Sinking", "Falling", "Dropping", "Plunging", "Collapsing", "Diving", "Crashing", "Freefall"],
      ["Drop", "Downlift", "Plunge", "Fall", "Dive", "Collapse", "Descent", "Sink", "Crash", "Abyss"],
      bufferBars=1),

    L("Lo-Fi Bedroom", [VINYL, HALF],
      R(time=(0.2, 0.5), depth=(0.3, 0.6), mix=(0.6, 0.9), texture=(0.4, 0.7), space=(0.2, 0.4), width=(0.3, 0.5)),
      dict(texture=+1),
      ["Cozy", "Mellow", "Hazy", "Sleepy", "Muffled", "Dreamy", "Faded", "Nostalgic"],
      ["Bedroom", "Lullaby", "Cassette", "Blanket", "Dusk", "Vinyl", "Study", "Rain", "Loop", "Nook"],
      bufferBars=1),

    L("Cinematic Space", [FREEZE, REVERSE],
      R(time=(0.3, 0.7), depth=(0.4, 0.9), mix=(0.6, 1.0), texture=(0.0, 0.2), space=(0.7, 1.0), width=(0.7, 1.0)),
      dict(space=+1, depth=+1, width=+1),
      ["Wide", "Sweeping", "Vast", "Ominous", "Towering", "Dramatic", "Colossal", "Cataclysmic"],
      ["Horizon", "Expanse", "Score", "Vista", "Trailer", "Impact", "Nebula", "Odyssey", "Eclipse", "Requiem"],
      bufferBars=1),

    L("IDM Textures", [GLITCH, GRANULAR],
      R(time=(0.4, 0.9), depth=(0.5, 1.0), mix=(0.7, 1.0), texture=(0.2, 0.5), space=(0.2, 0.5), width=(0.6, 1.0)),
      dict(depth=+1, texture=+1),
      ["Angular", "Glitched", "Fragmented", "Erratic", "Complex", "Granular", "Algorithmic", "Fractal"],
      ["Texture", "Fragment", "Cell", "Pattern", "Lattice", "Artifact", "Sequence", "Node", "Mesh", "Grain"],
      quality=2, bufferBars=0),

    L("Rhythmic Sync", [STUTTER, BEATREPEAT],
      R(time=(0.35, 0.7), depth=(0.4, 0.9), mix=(0.85, 1.0), texture=(0.1, 0.3), space=(0.05, 0.25), width=(0.45, 0.75)),
      dict(depth=+1),
      ["Loose", "Swung", "Bouncing", "Grooving", "Shuffling", "Syncopated", "Humanized", "Pocketed"],
      ["Groove", "Swing", "Shuffle", "Pocket", "Bounce", "Sync", "Pulse", "Flow", "Ride", "Skip"],
      sync=[2, 3], swing=(0.1, 0.4), humanize=(0.1, 0.4), bufferBars=0),

    L("Warp Leads", [WARP],
      R(time=(0.3, 0.7), depth=(0.4, 0.9), mix=(0.85, 1.0), texture=(0.1, 0.4), space=(0.1, 0.3), width=(0.4, 0.7)),
      dict(depth=+1),
      ["Smooth", "Singing", "Gliding", "Soaring", "Bending", "Vibrant", "Screaming", "Searing"],
      ["Lead", "Glide", "Song", "Voice", "Cry", "Line", "Melody", "Siren", "Wail", "Flight"],
      warpRate=[0, 1, 2], curves=True, bufferBars=1),

    L("Extreme Mangle", [GLITCH, GRANULAR, BEATREPEAT],
      R(time=(0.4, 0.95), depth=(0.8, 1.0), mix=(0.85, 1.0), texture=(0.4, 0.8), space=(0.3, 0.7), width=(0.5, 1.0)),
      dict(depth=+1, texture=+1, time=+1),
      ["Broken", "Violent", "Savage", "Destroyed", "Obliterated", "Chaotic", "Nuclear", "Apocalyptic"],
      ["Mangle", "Chaos", "Meltdown", "Carnage", "Havoc", "Ruin", "Fallout", "Rupture", "Wreck", "Blast"],
      quality=2, sync=[3, 4], bufferBars=0),
]

# ---------------------------------------------------------------------------
#  Naming: adjective tracks intensity (mild presets get mild words), noun walks
#  the pool for variety. Guaranteed unique within a bank via a collision bump.
# ---------------------------------------------------------------------------
def build_names(lib):
    adj, nn = lib["adj"], lib["nn"]
    used = set()
    names = []
    for i in range(40):
        t = i / 39.0
        ai = min(len(adj) - 1, int(t * len(adj) + 0.5 * spread(i, 0.0, 1.0, 0.23)))
        ai = max(0, min(len(adj) - 1, ai))
        ni = (i * 3) % len(nn)
        for _ in range(len(nn)):
            nm = f"{adj[ai]} {nn[ni]}"
            if nm not in used:
                break
            ni = (ni + 1) % len(nn)
        else:
            # exhausted this adjective row — step the adjective too
            ai = (ai + 1) % len(adj)
            nm = f"{adj[ai]} {nn[ni]}"
        used.add(nm)
        names.append(nm)
    return names

# ---------------------------------------------------------------------------
def fmt(v, key):
    if key in INT_PARAMS:
        return str(int(round(v)))
    return f"{v:.4f}".rstrip('0').rstrip('.') if isinstance(v, float) else str(v)

def preset_xml(name, params, tcurve, vcurve):
    def curve_xml(tag, pts):
        rows = "".join(f'    <p x="{x:.4f}" y="{y:.4f}" c="{c:.4f}"/>\n' for (x, y, c) in pts)
        return f'  <{tag}>\n{rows}  </{tag}>\n'
    prm = "".join(f'    <PARAM id="{k}" value="{fmt(params[k], k)}"/>\n' for k in DEFAULTS)
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<CHRONA version="1" presetName="{html.escape(name, quote=True)}">\n'
            f'  <STATE>\n{prm}  </STATE>\n'
            f'{curve_xml("TimeCurve", tcurve)}{curve_xml("VolCurve", vcurve)}'
            f'</CHRONA>\n')

MACROS = ("time", "depth", "mix", "texture", "space", "width")

def build():
    total = 0
    for li, lib in enumerate(LIBRARIES):
        folder = OUT / lib["name"]
        folder.mkdir(parents=True, exist_ok=True)
        names = build_names(lib)
        ex = lib["extra"]
        arc = lib["arc"]
        for i in range(40):
            t = smooth(i / 39.0)
            p = dict(DEFAULTS)
            mode = lib["modes"][i % len(lib["modes"])]
            p["mode"] = mode

            # --- macros: arc-driven follow intensity, the rest spread ------
            for ph, mk in enumerate(MACROS):
                lo, hi = lib["ranges"][mk]
                if mk in arc:
                    base = lo + (hi - lo) * (t if arc[mk] > 0 else (1.0 - t))
                    jit = spread(i, -0.09, 0.09, 0.11 * (ph + 1)) * (hi - lo)
                    p[mk] = clampf(mk, base + jit)
                else:
                    p[mk] = clampf(mk, spread(i, lo, hi, 0.137 * (ph + 1)))

            # --- per-mode voicing, then bank overrides ---------------------
            aux = MODE_AUX[mode]
            p["antiClick"] = clampf("antiClick", aux["antiClick"])
            p["smartFade"] = clampf("smartFade", aux["smartFade"])
            p["quality"] = clampi("quality", ex.get("quality", aux["quality"]))
            p["bufferBars"] = clampi("bufferBars", ex.get("bufferBars", DEFAULTS["bufferBars"]))

            if "sync" in ex:     p["sync"] = clampi("sync", ex["sync"][i % len(ex["sync"])])
            if "warpRate" in ex: p["warpRate"] = clampi("warpRate", ex["warpRate"][i % len(ex["warpRate"])])

            # ranged aux (gate/swing/humanize/smartFade override) — arc-aware
            for rk in ("swing", "humanize", "gateAmount", "smartFade"):
                if rk in ex and isinstance(ex[rk], tuple):
                    rlo, rhi = ex[rk]
                    if rk in arc:
                        p[rk] = clampf(rk, rlo + (rhi - rlo) * (t if arc[rk] > 0 else (1.0 - t)))
                    else:
                        p[rk] = clampf(rk, spread(i, rlo, rhi, 0.31))

            # --- curves ----------------------------------------------------
            if ex.get("curves"):
                tc = warp_curve(i % 8, spread(i, 0.4, 1.0, 0.5))
                vc = vol_curve(i % 4, spread(i, 0.2, 0.8, 0.7))
            else:
                tc, vc = flat_time(), flat_vol()

            (folder / f"{names[i]}.chrona").write_text(
                preset_xml(names[i], p, tc, vc), encoding="utf-8")
            total += 1
    print(f"generated {total} presets across {len(LIBRARIES)} libraries in {OUT}")

if __name__ == "__main__":
    build()
