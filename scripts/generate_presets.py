#!/usr/bin/env python3
# ---------------------------------------------------------------------------
#  CHRONA factory preset-bank generator.
#
#  Produces 20 libraries × 40 presets = 800 .chrona files under presets/<Library>/.
#  Each library has a strong, distinct identity (mode set + macro centre +
#  texture/space/width signature + curve style); presets inside a library share
#  that identity but each has its own character. Deterministic — re-running
#  yields byte-identical output.
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
import os, html, pathlib

OUT = pathlib.Path(__file__).resolve().parent.parent / "presets"

# mode indices (mirror params::Mode)
HALF, DOUBLE, REVERSE, TAPESTOP, STUTTER, BEATREPEAT, VINYL, GLITCH, WARP, FREEZE, GRANULAR = range(11)

# full default parameter set (denormalised)
DEFAULTS = dict(
    time=0.5, depth=1.0, mix=1.0, texture=0.0, space=0.0, width=0.5,
    mode=HALF, sync=2, bufferBars=1, warpRate=3, snap=1, quality=1,
    antiClick=3.0, swing=0.0, humanize=0.0, smartFade=0.5,
    triggerMode=0, triggerNote=60, gateAmount=0.0, duckAmount=0.0,
    duckAttack=5.0, duckRelease=120.0, scSource=0, bypass=0,
)
INT_PARAMS = {"mode","sync","bufferBars","warpRate","snap","quality",
              "triggerMode","triggerNote","scSource","bypass"}

GOLDEN = 0.61803398875
def spread(i, lo, hi, phase):
    """Deterministic low-discrepancy value in [lo,hi] for preset i."""
    t = (0.5 + i * GOLDEN + phase) % 1.0
    return lo + (hi - lo) * t

# ---- curve generators (for Time-Warp libraries) --------------------------
def flat_time():  return [(0.0,0.0,0.0),(1.0,0.0,0.0)]
def flat_vol():   return [(0.0,1.0,0.0),(1.0,1.0,0.0)]

def warp_curve(kind, amt):
    a = 0.2 + 0.8*amt
    if kind == 0:  return [(0.0,0.0,0.0),(1.0,a,0.0)]                     # ramp up
    if kind == 1:  return [(0.0,a,0.0),(1.0,0.0,0.0)]                     # ramp down
    if kind == 2:  return [(0.0,a,0.0),(0.5,0.0,0.0),(1.0,a,0.0)]        # V
    if kind == 3:  return [(0.0,0.0,0.0),(0.5,a,0.0),(1.0,0.0,0.0)]      # peak
    if kind == 4:  return [(0.0,0.0,0.0),(0.33,a*0.5,0.0),(0.33,a*0.5,0.0),
                           (0.66,a,0.0),(0.66,a,0.0),(1.0,a,0.0)]         # stair
    if kind == 5:  return [(0.0,0.0,0.6),(1.0,a,0.0)]                     # eased swell
    if kind == 6:  return [(0.0,0.0,0.0),(0.25,a,0.0),(0.5,0.1,0.0),
                           (0.75,a,0.0),(1.0,0.0,0.0)]                    # wave
    return [(0.0,0.0,0.0),(0.9,a,0.0),(1.0,0.0,0.0)]                      # saw

def vol_curve(kind, amt):
    if kind == 0:  return flat_vol()
    d = 1.0 - 0.7*amt
    if kind == 1:  return [(0.0,1.0,0.0),(0.5,d,0.0),(1.0,1.0,0.0)]      # dip
    if kind == 2:  return [(0.0,1.0,0.0),(0.5,1.0,0.0),(0.5,d,0.0),(1.0,d,0.0)]  # step down
    return [(0.0,d,0.0),(1.0,1.0,0.0)]                                    # fade in

# ---- library profiles ----------------------------------------------------
# Each: name, modes, per-macro (lo,hi) ranges, and extra fixed/ranged params.
def L(name, modes, r, **extra):
    return dict(name=name, modes=modes, ranges=r, extra=extra)

R = lambda **kw: kw   # macro-range helper

LIBRARIES = [
    L("Halftime Heavy", [HALF, DOUBLE],
      R(time=(0.05,0.35), depth=(0.6,1.0), mix=(0.85,1.0), texture=(0.1,0.4), space=(0.1,0.35), width=(0.4,0.6)),
      sync=[1,2]),
    L("Glitch Circuits", [GLITCH, STUTTER],
      R(time=(0.4,0.9), depth=(0.6,1.0), mix=(0.8,1.0), texture=(0.2,0.5), space=(0.0,0.2), width=(0.5,0.95)),
      sync=[2,3,4], quality=2, smartFade=(0.0,0.25)),
    L("Tape Nostalgia", [VINYL, TAPESTOP],
      R(time=(0.2,0.6), depth=(0.4,0.85), mix=(0.7,1.0), texture=(0.45,0.85), space=(0.2,0.45), width=(0.3,0.5))),
    L("Frozen Cathedrals", [FREEZE],
      R(time=(0.3,0.7), depth=(0.3,0.8), mix=(0.7,1.0), texture=(0.0,0.2), space=(0.6,1.0), width=(0.6,1.0))),
    L("Granular Clouds", [GRANULAR],
      R(time=(0.2,0.85), depth=(0.4,0.95), mix=(0.6,1.0), texture=(0.0,0.3), space=(0.3,0.7), width=(0.6,1.0))),
    L("Beat Repeat Mangler", [BEATREPEAT, STUTTER],
      R(time=(0.3,0.7), depth=(0.5,1.0), mix=(0.85,1.0), texture=(0.1,0.4), space=(0.05,0.25), width=(0.45,0.75)),
      sync=[2,3,4], swing=(0.0,0.3)),
    L("Reverse Worlds", [REVERSE],
      R(time=(0.3,0.7), depth=(0.4,0.9), mix=(0.6,1.0), texture=(0.0,0.3), space=(0.3,0.7), width=(0.5,0.9))),
    L("Time Warp Motion", [WARP],
      R(time=(0.3,0.7), depth=(0.5,1.0), mix=(0.85,1.0), texture=(0.0,0.3), space=(0.2,0.5), width=(0.4,0.7)),
      warpRate=[1,2,3], curves=True),
    L("Stutter Gate", [STUTTER],
      R(time=(0.4,0.8), depth=(0.4,0.9), mix=(0.85,1.0), texture=(0.1,0.3), space=(0.05,0.2), width=(0.4,0.7)),
      sync=[3,4], gateAmount=(0.3,0.9)),
    L("Vinyl Dust", [VINYL],
      R(time=(0.2,0.5), depth=(0.5,0.9), mix=(0.7,1.0), texture=(0.5,0.9), space=(0.1,0.3), width=(0.3,0.5))),
    L("Ambient Drift", [FREEZE, GRANULAR],
      R(time=(0.2,0.6), depth=(0.3,0.7), mix=(0.6,0.9), texture=(0.0,0.2), space=(0.5,1.0), width=(0.7,1.0))),
    L("Percussive Chops", [STUTTER, BEATREPEAT],
      R(time=(0.6,0.95), depth=(0.6,1.0), mix=(0.9,1.0), texture=(0.1,0.3), space=(0.0,0.15), width=(0.4,0.7)),
      sync=[4], smartFade=(0.0,0.2)),
    L("Riser FX", [REVERSE, TAPESTOP],
      R(time=(0.4,0.8), depth=(0.5,1.0), mix=(0.7,1.0), texture=(0.0,0.3), space=(0.3,0.6), width=(0.5,0.9))),
    L("Downlifter FX", [TAPESTOP, HALF],
      R(time=(0.05,0.4), depth=(0.6,1.0), mix=(0.8,1.0), texture=(0.1,0.4), space=(0.3,0.6), width=(0.4,0.6))),
    L("Lo-Fi Bedroom", [VINYL, HALF],
      R(time=(0.2,0.5), depth=(0.3,0.6), mix=(0.6,0.9), texture=(0.4,0.7), space=(0.2,0.4), width=(0.3,0.5))),
    L("Cinematic Space", [FREEZE, REVERSE],
      R(time=(0.3,0.7), depth=(0.4,0.9), mix=(0.6,1.0), texture=(0.0,0.2), space=(0.7,1.0), width=(0.7,1.0))),
    L("IDM Textures", [GLITCH, GRANULAR],
      R(time=(0.4,0.9), depth=(0.5,1.0), mix=(0.7,1.0), texture=(0.2,0.5), space=(0.2,0.5), width=(0.6,1.0)),
      quality=2),
    L("Rhythmic Sync", [STUTTER, BEATREPEAT],
      R(time=(0.35,0.7), depth=(0.4,0.9), mix=(0.85,1.0), texture=(0.1,0.3), space=(0.05,0.25), width=(0.45,0.75)),
      sync=[2,3], swing=(0.1,0.4), humanize=(0.1,0.4)),
    L("Warp Leads", [WARP],
      R(time=(0.3,0.7), depth=(0.4,0.9), mix=(0.85,1.0), texture=(0.1,0.4), space=(0.1,0.3), width=(0.4,0.7)),
      warpRate=[0,1,2], curves=True),
    L("Extreme Mangle", [GLITCH, GRANULAR, BEATREPEAT],
      R(time=(0.4,0.95), depth=(0.8,1.0), mix=(0.85,1.0), texture=(0.4,0.8), space=(0.3,0.7), width=(0.5,1.0)),
      quality=2, sync=[3,4]),
]

# themed name vocabularies per library (8 adjectives × 8 nouns → 64 combos)
ADJS = ["Deep","Broken","Molten","Velvet","Neon","Dusty","Frozen","Crushed",
        "Hollow","Liquid","Static","Golden","Phantom","Warped","Silent","Amber"]
NOUNS = ["Pulse","Drift","Fracture","Bloom","Halo","Machine","Echo","Current",
         "Mirage","Circuit","Cascade","Ember","Vapor","Anchor","Signal","Tide"]

def name_for(lib_index, i):
    # co-prime stride so 40 names are unique within a library, varied across libs
    base = (lib_index * 7 + i * 17) % (len(ADJS) * len(NOUNS))
    return f"{ADJS[base // len(NOUNS)]} {NOUNS[base % len(NOUNS)]}"

def fmt(v, key):
    if key in INT_PARAMS:
        return str(int(round(v)))
    return f"{v:.4f}".rstrip('0').rstrip('.') if isinstance(v, float) else str(v)

def preset_xml(name, params, tcurve, vcurve):
    def curve_xml(tag, pts):
        rows = "".join(f'    <p x="{x:.4f}" y="{y:.4f}" c="{c:.4f}"/>\n' for (x,y,c) in pts)
        return f'  <{tag}>\n{rows}  </{tag}>\n'
    prm = "".join(f'    <PARAM id="{k}" value="{fmt(params[k], k)}"/>\n' for k in DEFAULTS)
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<CHRONA version="1" presetName="{html.escape(name, quote=True)}">\n'
            f'  <STATE>\n{prm}  </STATE>\n'
            f'{curve_xml("TimeCurve", tcurve)}{curve_xml("VolCurve", vcurve)}'
            f'</CHRONA>\n')

def build():
    total = 0
    for li, lib in enumerate(LIBRARIES):
        folder = OUT / lib["name"]
        folder.mkdir(parents=True, exist_ok=True)
        for i in range(40):
            p = dict(DEFAULTS)
            p["mode"] = lib["modes"][i % len(lib["modes"])]
            for ph, (mk, (lo, hi)) in enumerate(lib["ranges"].items()):
                p[mk] = round(spread(i, lo, hi, ph * 0.137), 4)
            ex = lib["extra"]
            if "sync" in ex:        p["sync"] = ex["sync"][i % len(ex["sync"])]
            if "warpRate" in ex:    p["warpRate"] = ex["warpRate"][i % len(ex["warpRate"])]
            if "quality" in ex:     p["quality"] = ex["quality"]
            for rk in ("swing","humanize","gateAmount","smartFade"):
                if rk in ex and isinstance(ex[rk], tuple):
                    p[rk] = round(spread(i, ex[rk][0], ex[rk][1], 0.31), 4)
            # curves
            if ex.get("curves"):
                tc = warp_curve(i % 8, spread(i, 0.4, 1.0, 0.5))
                vc = vol_curve(i % 3, spread(i, 0.2, 0.8, 0.7))
            else:
                tc, vc = flat_time(), flat_vol()
            nm = name_for(li, i)
            (folder / f"{nm}.chrona").write_text(preset_xml(nm, p, tc, vc), encoding="utf-8")
            total += 1
    print(f"generated {total} presets across {len(LIBRARIES)} libraries in {OUT}")

if __name__ == "__main__":
    build()
