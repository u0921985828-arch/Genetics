// CHRONA WebView front-end
// Binds the six macros, mode selector and bypass to the plug-in's APVTS
// parameters via the JUCE 8 relay API, drives the preset browser and A/B
// compare through native functions, and renders the buffer visualiser and
// I/O meters from the "vis" events the processor pushes each frame.
import * as Juce from "./juce/index.js";

const native = (name) => Juce.getNativeFunction(name);

const MACROS = [
  ["time", "Time"], ["depth", "Depth"], ["mix", "Mix"],
  ["texture", "Texture"], ["space", "Space"], ["width", "Width"],
];
const MACRO_TIPS = {
  time: "Musical rate / warp amount (mode-dependent)",
  depth: "Effect intensity", mix: "Dry / wet balance",
  texture: "Saturation · vinyl grit · tone tilt",
  space: "Ambience tail", width: "Stereo width",
};
const MODE_TIPS = {
  half: "Half-time", double: "Double-time", reverse: "Reverse playback",
  "tape stop": "Tape-style spin-down", stutter: "Sliced repeat",
  "beat repeat": "Beat-locked repeat with variation", vinyl: "Wow/flutter + surface noise",
  glitch: "Stochastic slice/pitch/reverse", "time warp": "Curve-driven time warp",
  freeze: "Frozen spectral bloom", granular: "Grain cloud",
};

// Geometry of the knob (viewBox 0 0 100 100).
const A0 = -135, A1 = 135;                 // sweep, degrees
const rad = (d) => (d - 90) * Math.PI / 180;
const polar = (cx, cy, r, deg) => [cx + r * Math.cos(rad(deg)), cy + r * Math.sin(rad(deg))];
const arcPath = (cx, cy, r, a, b) => {
  const [x0, y0] = polar(cx, cy, r, a), [x1, y1] = polar(cx, cy, r, b);
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${b - a > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
};

// Shared SVG defs — one vivid gradient (cyan → blue → violet), injected once.
const DEFS = `
  <defs>
    <linearGradient id="arcGrad" x1="0" y1="1" x2="1" y2="0">
      <stop offset="0" stop-color="#4fd6ff"/>
      <stop offset="0.5" stop-color="#5b8cff"/>
      <stop offset="1" stop-color="#9b6bff"/>
    </linearGradient>
    <radialGradient id="capGrad" cx="50%" cy="38%" r="70%">
      <stop offset="0" stop-color="#20242c"/>
      <stop offset="100%" stop-color="#14171d"/>
    </radialGradient>
    <linearGradient id="capHi" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="rgba(255,255,255,.10)"/>
      <stop offset="0.45" stop-color="rgba(255,255,255,0)"/>
    </linearGradient>
  </defs>`;

// A clean, flat knob: a thin track, a gradient value arc, a flat cap and a
// single glowing bead + slim pointer. No ticks, no bevels.
function knobSVG() {
  return `<svg viewBox="0 0 100 100">${DEFS}
    <path class="arc-bg" d="${arcPath(50, 50, 39, A0, A1)}"/>
    <path class="arc" d="${arcPath(50, 50, 39, A0, A0)}"/>
    <circle class="cap-face" cx="50" cy="50" r="28"/>
    <circle class="cap-hi" cx="50" cy="50" r="28"/>
    <line class="ptr" x1="50" y1="30" x2="50" y2="16"/>
    <circle class="bead" cx="50" cy="11" r="3.4"/>
  </svg>`;
}

// ---- macro knobs ----------------------------------------------------------
const knobRoot = document.getElementById("knobs");
const macroStates = {};
for (const [id, label] of MACROS) {
  const state = Juce.getSliderState(id);
  macroStates[id] = state;

  const el = document.createElement("div");
  el.className = "knob";
  el.title = MACRO_TIPS[id] || label;
  el.innerHTML =
    `<div class="cap">${label}</div>` +
    `<div class="dial">${knobSVG()}</div>` +
    `<div class="val">0%</div>`;
  const dial = el.querySelector(".dial");
  const arc  = el.querySelector(".arc");
  const ptr  = el.querySelector(".ptr");
  const bead = el.querySelector(".bead");
  const val  = el.querySelector(".val");
  knobRoot.appendChild(el);

  const render = () => {
    const v = Math.min(1, Math.max(0, state.getNormalisedValue()));
    const a = A0 + (A1 - A0) * v;
    arc.setAttribute("d", arcPath(50, 50, 39, A0, Math.max(A0 + 0.01, a)));
    const [ox, oy] = polar(50, 50, 30, a), [ix, iy] = polar(50, 50, 16, a);
    ptr.setAttribute("x1", ox.toFixed(2)); ptr.setAttribute("y1", oy.toFixed(2));
    ptr.setAttribute("x2", ix.toFixed(2)); ptr.setAttribute("y2", iy.toFixed(2));
    const [bx, by] = polar(50, 50, 39, a);
    bead.setAttribute("cx", bx.toFixed(2)); bead.setAttribute("cy", by.toFixed(2));
    val.textContent = Math.round(v * 100) + "%";
  };

  state.valueChangedEvent.addListener(render);
  render();

  // vertical drag → normalised value (fine with Shift)
  let dragging = false, lastY = 0;
  const down = (e) => { dragging = true; lastY = (e.touches ? e.touches[0] : e).clientY; e.preventDefault(); };
  const move = (e) => {
    if (!dragging) return;
    const y = (e.touches ? e.touches[0] : e).clientY;
    const gain = e.shiftKey ? 520 : 170;
    const v = Math.min(1, Math.max(0, state.getNormalisedValue() - (y - lastY) / gain));
    lastY = y;
    state.setNormalisedValue(v);
    render();
  };
  const up = () => { dragging = false; };
  dial.addEventListener("mousedown", down);
  dial.addEventListener("touchstart", down, { passive: false });
  window.addEventListener("mousemove", move);
  window.addEventListener("touchmove", move, { passive: false });
  window.addEventListener("mouseup", up);
  window.addEventListener("touchend", up);
  dial.addEventListener("dblclick", () => { state.setNormalisedValue(0.5); render(); });
  dial.addEventListener("wheel", (e) => {
    e.preventDefault();
    const v = Math.min(1, Math.max(0, state.getNormalisedValue() - Math.sign(e.deltaY) * (e.shiftKey ? 0.01 : 0.04)));
    state.setNormalisedValue(v); render();
  }, { passive: false });
}

// ---- mode selector --------------------------------------------------------
// Compact line-icons keyed by the mode name (falls back to a generic glyph).
const ICONS = {
  half:      `<path d="M4 12h6M14 8v8M14 8l4 2-4 2"/>`,
  double:    `<path d="M3 10l3 2-3 2M9 8v8M13 10l3 2-3 2M18 8v8"/>`,
  reverse:   `<path d="M20 12H6M11 7l-5 5 5 5"/>`,
  "tape stop":`<path d="M12 4a8 8 0 108 8M12 4v4M12 12l5-2"/>`,
  stutter:   `<path d="M4 8v8M8 6v12M12 8v8M16 6v12M20 8v8"/>`,
  "beat repeat":`<path d="M5 10a6 6 0 016-6h4M15 4l2 2-2 2M19 14a6 6 0 01-6 6H9M9 20l-2-2 2-2"/>`,
  vinyl:     `<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.2"/>`,
  glitch:    `<path d="M3 12h4l2-5 3 10 2-6 2 3h3"/>`,
  "time warp":`<path d="M4 16c3-8 13-8 16 0M4 16h4M20 16h-4"/>`,
  freeze:    `<path d="M12 3v18M4.5 7.5l15 9M19.5 7.5l-15 9M12 3l-2 2M12 3l2 2M12 21l-2-2M12 21l2-2"/>`,
  granular:  `<circle cx="7" cy="9" r="1.4"/><circle cx="12" cy="14" r="1.4"/><circle cx="17" cy="8" r="1.4"/><circle cx="10" cy="6" r="1.1"/><circle cx="15" cy="17" r="1.1"/><circle cx="6" cy="16" r="1.1"/>`,
};
const iconFor = (name) => `<svg viewBox="0 0 24 24">${ICONS[name.toLowerCase()] || `<circle cx="12" cy="12" r="7"/>`}</svg>`;

const modeState = Juce.getComboBoxState("mode");
const modeRoot = document.getElementById("modes");
const scopeMode = document.getElementById("scopeMode");
function updateScopeMode() {
  const c = modeState.properties.choices || [];
  const n = c[modeState.getChoiceIndex()];
  if (n) scopeMode.textContent = n.toUpperCase();
}
function buildModes() {
  modeRoot.innerHTML = "";
  const choices = modeState.properties.choices || [];
  choices.forEach((name, i) => {
    const b = document.createElement("div");
    b.className = "mode" + (i === modeState.getChoiceIndex() ? " on" : "");
    b.title = MODE_TIPS[name.toLowerCase()] || name;
    b.innerHTML = iconFor(name) + `<span>${name}</span>`;
    b.onclick = () => { modeState.setChoiceIndex(i); };
    modeRoot.appendChild(b);
  });
  updateScopeMode();
}
modeState.valueChangedEvent.addListener(() => {
  modeRoot.querySelectorAll(".mode").forEach((el, i) =>
    el.classList.toggle("on", i === modeState.getChoiceIndex()));
  updateScopeMode();
});
modeState.propertiesChangedEvent.addListener(buildModes);
buildModes();

// ---- randomize (dice) -----------------------------------------------------
// A musical randomiser: spread the character macros widely but keep Mix
// listenable, and pick a fresh mode. Frontend-only (drives the relays).
const rndBtn = document.getElementById("rnd");
if (rndBtn) rndBtn.onclick = () => {
  const rand = (lo, hi) => lo + Math.random() * (hi - lo);
  const set = (id, v) => macroStates[id] && macroStates[id].setNormalisedValue(Math.min(1, Math.max(0, v)));
  set("time", rand(0, 1)); set("depth", rand(0.4, 1)); set("mix", rand(0.5, 1));
  set("texture", rand(0, 0.7)); set("space", rand(0, 0.6)); set("width", rand(0.3, 1));
  const n = (modeState.properties.choices || []).length || 1;
  modeState.setChoiceIndex(Math.floor(Math.random() * n));
};

// ---- bypass ---------------------------------------------------------------
const bypassState = Juce.getToggleState("bypass");
const bypassEl = document.getElementById("bypass");
const appEl = document.querySelector(".app");
const statusL = document.getElementById("statusL");
const syncBypass = () => {
  const on = bypassState.getValue();
  bypassEl.classList.toggle("on", on);
  bypassEl.setAttribute("aria-pressed", on ? "true" : "false");
  appEl.style.opacity = on ? ".55" : "1";
  statusL.textContent = on ? "Bypassed" : "Active";
};
bypassEl.onclick = () => bypassState.setValue(!bypassState.getValue());
bypassState.valueChangedEvent.addListener(syncBypass);
syncBypass();

// ---- preset browser -------------------------------------------------------
const presetName = document.getElementById("presetName");
const statusR = document.getElementById("statusR");
const presetPrev = native("presetPrev"), presetNext = native("presetNext");
document.getElementById("prev").onclick = () => presetPrev();
document.getElementById("next").onclick = () => presetNext();
window.__JUCE__.backend.addEventListener("preset", (e) => {
  if (e && e.name) presetName.textContent = e.name;
  if (statusR && typeof e.index === "number" && e.count)
    statusR.textContent = `CHRONA · ${e.index + 1}/${e.count}`;
});

// preset dropdown: click the name to browse the whole bank
const presetBox = document.querySelector(".preset");
const presetListNative = native("getPresetList"), presetLoadNative = native("presetLoad");
const dropdown = document.createElement("div");
dropdown.className = "preset-list";
presetBox.appendChild(dropdown);
let dropOpen = false;
presetName.style.cursor = "pointer";
presetName.onclick = async () => {
  dropOpen = !dropOpen;
  dropdown.classList.toggle("open", dropOpen);
  if (!dropOpen) return;
  let names = [];
  try { names = (await presetListNative()) || []; } catch (_) { names = []; }
  dropdown.innerHTML = "";
  names.forEach((nm, i) => {
    const it = document.createElement("div");
    it.className = "pl-item" + (nm === presetName.textContent ? " on" : "");
    it.textContent = nm;
    it.onclick = () => { presetLoadNative(i); dropOpen = false; dropdown.classList.remove("open"); };
    dropdown.appendChild(it);
  });
};
document.addEventListener("click", (e) => {
  if (!presetBox.contains(e.target)) { dropOpen = false; dropdown.classList.remove("open"); }
});

// ---- A/B compare ----------------------------------------------------------
const abSelect = native("abSelect");
const abSegs = document.querySelectorAll(".ab .seg");
abSegs.forEach((seg, i) => { seg.onclick = () => abSelect(i); });
window.__JUCE__.backend.addEventListener("ab", (e) => {
  const s = e && typeof e.slot === "number" ? e.slot : 0;
  abSegs.forEach((seg, i) => seg.classList.toggle("on", i === s));
});

// tell the backend the page is ready so it pushes initial preset + A/B state
if (Juce.getNativeFunction) native("uiReady")();

// ---- Time-curve editor (Time Warp mode) -----------------------------------
// An editable breakpoint curve laid over the buffer: click to add, drag a node
// to move, drag a segment handle to bend it, double-click to delete. Bound to
// the plug-in's Time automation curve; the RATE selector sets its cycle length.
const scopeWrap = document.querySelector(".scope-wrap");
const curveEl = document.createElement("div");
curveEl.className = "curve";
curveEl.innerHTML =
  `<div class="curve-head">
     <span class="ch-lane"><span class="lane on" data-l="time">TIME</span><span class="lane" data-l="vol">VOL</span></span>
     <div class="ch-tools"><span class="ch-rate" id="warpRate"></span>
       <span class="ch-hint">click add · drag · bend handle · dbl-click delete</span></div>
   </div>
   <svg class="curve-svg" viewBox="0 0 1000 1000" preserveAspectRatio="none">
     <defs><linearGradient id="cvGrad" x1="0" y1="0" x2="1" y2="0">
       <stop offset="0" stop-color="#4fd6ff"/><stop offset="0.5" stop-color="#5b8cff"/>
       <stop offset="1" stop-color="#9b6bff"/></linearGradient></defs>
     <g class="cv-grid"></g>
     <path class="cv-fill"/><path class="cv-line"/><g class="cv-handles"></g><g class="cv-nodes"></g>
   </svg>`;
scopeWrap.appendChild(curveEl);
const cvSvg   = curveEl.querySelector(".curve-svg");
const cvFill  = curveEl.querySelector(".cv-fill");
const cvLine  = curveEl.querySelector(".cv-line");
const cvNodes = curveEl.querySelector(".cv-nodes");
const cvHandles = curveEl.querySelector(".cv-handles");
const cvGrid  = curveEl.querySelector(".cv-grid");
{
  let g = "";
  for (let i = 1; i < 16; i++) { const x = (i / 16 * 1000).toFixed(1); g += `<line x1="${x}" y1="0" x2="${x}" y2="1000"/>`; }
  for (let j = 1; j < 4; j++) { const y = (j / 4 * 1000).toFixed(1); g += `<line x1="0" y1="${y}" x2="1000" y2="${y}"/>`; }
  cvGrid.innerHTML = g;
}

// two editable lanes: Time (delay) and Volume (gain)
const curves = {
  time: [{ x: 0, y: 0, c: 0 }, { x: 1, y: 0, c: 0 }],
  vol:  [{ x: 0, y: 1, c: 0 }, { x: 1, y: 1, c: 0 }],
};
let lane = "time";
let curve = curves[lane];
const setCurveNative = native("setCurve");
const NS = "http://www.w3.org/2000/svg";
const SX = (x) => x * 1000, SY = (y) => (1 - y) * 1000;
// mirror of Curve::applyCurve — exponential ease shaped by the segment's c.
const applyCurve = (t, c) => (Math.abs(c) < 1e-4) ? t
  : (Math.exp(c * 4 * t) - 1) / (Math.exp(c * 4) - 1);

let curvePushT = 0;
function pushCurve() {
  clearTimeout(curvePushT);
  // snapshot BOTH the lane and the points now — `curve` is reassigned on lane
  // switch, so reading it lazily at timer-fire could publish the wrong lane.
  const which = lane, pts = curve.map(p => ({ x: p.x, y: p.y, c: p.c || 0 }));
  curvePushT = setTimeout(() => setCurveNative(which, pts), 20);
}
function redrawCurve() {
  curve.sort((a, b) => a.x - b.x);
  // sampled path so segment curvature shows
  let d = `M ${SX(curve[0].x).toFixed(1)} ${SY(curve[0].y).toFixed(1)}`;
  for (let i = 0; i < curve.length - 1; i++) {
    const a = curve[i], b = curve[i + 1];
    for (let s = 1; s <= 12; s++) {
      const u = s / 12, t = applyCurve(u, a.c || 0);
      d += ` L ${SX(a.x + (b.x - a.x) * u).toFixed(1)} ${SY(a.y + (b.y - a.y) * t).toFixed(1)}`;
    }
  }
  cvLine.setAttribute("d", d);
  cvFill.setAttribute("d", d + ` L 1000 1000 L 0 1000 Z`);
  // segment bend handles (midpoint of each segment)
  cvHandles.innerHTML = "";
  for (let i = 0; i < curve.length - 1; i++) {
    const a = curve[i], b = curve[i + 1];
    const hx = a.x + (b.x - a.x) * 0.5, hy = a.y + (b.y - a.y) * applyCurve(0.5, a.c || 0);
    const h = document.createElementNS(NS, "circle");
    h.setAttribute("cx", SX(hx)); h.setAttribute("cy", SY(hy)); h.setAttribute("r", 7);
    h.dataset.seg = i; h.classList.add("cv-h"); cvHandles.appendChild(h);
  }
  cvNodes.innerHTML = "";
  curve.forEach((p, i) => {
    const c = document.createElementNS(NS, "circle");
    c.setAttribute("cx", SX(p.x)); c.setAttribute("cy", SY(p.y)); c.setAttribute("r", 13);
    c.dataset.i = i; cvNodes.appendChild(c);
  });
}
const toNorm = (e) => {
  const r = cvSvg.getBoundingClientRect();
  return {
    x: Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)),
    y: Math.min(1, Math.max(0, 1 - (e.clientY - r.top) / r.height)),
  };
};
const snapX = (x, e) => e && e.altKey ? x : Math.round(x * 16) / 16;

let dragIdx = -1, dragSeg = -1;
cvSvg.addEventListener("pointerdown", (e) => {
  e.preventDefault();
  const t = e.target;
  if (t.classList.contains("cv-h")) { dragSeg = +t.dataset.seg; }
  else if (t.tagName === "circle") { dragIdx = +t.dataset.i; }
  else {
    const p = toNorm(e);
    const node = { x: snapX(p.x, e), y: p.y, c: 0 };  // select by identity so a
    curve.push(node);                                  // coord collision with an
    curve.sort((a, b) => a.x - b.x);                   // endpoint can't grab it
    dragIdx = curve.indexOf(node);
    redrawCurve(); pushCurve();
  }
  cvSvg.setPointerCapture(e.pointerId);
});
cvSvg.addEventListener("pointermove", (e) => {
  if (dragSeg >= 0) {                                   // bend a segment
    const a = curve[dragSeg], b = curve[dragSeg + 1], p = toNorm(e);
    const midY = (a.y + b.y) * 0.5, dir = (b.y >= a.y) ? 1 : -1;
    a.c = Math.max(-1, Math.min(1, (p.y - midY) * 4 * dir));
    redrawCurve(); pushCurve(); return;
  }
  if (dragIdx < 0) return;
  const p = toNorm(e), endpoint = (dragIdx === 0 || dragIdx === curve.length - 1);
  curve[dragIdx].y = p.y;
  if (!endpoint) curve[dragIdx].x = snapX(p.x, e);
  redrawCurve(); pushCurve();
});
const endDrag = () => { dragIdx = -1; dragSeg = -1; };
cvSvg.addEventListener("pointerup", endDrag);
cvSvg.addEventListener("pointercancel", endDrag);
cvSvg.addEventListener("dblclick", (e) => {
  if (e.target.tagName !== "circle" || e.target.classList.contains("cv-h")) return;
  const i = +e.target.dataset.i;
  if (i === 0 || i === curve.length - 1) return;   // keep endpoints
  curve.splice(i, 1); redrawCurve(); pushCurve();
});

// RATE selector (bound to the warpRate parameter)
const warpState = Juce.getComboBoxState("warpRate");
const rateRoot = document.getElementById("warpRate");
function buildRate() {
  rateRoot.innerHTML = "";
  (warpState.properties.choices || []).forEach((name, i) => {
    const b = document.createElement("span");
    b.className = "rate" + (i === warpState.getChoiceIndex() ? " on" : "");
    b.textContent = name;
    b.onclick = () => warpState.setChoiceIndex(i);
    rateRoot.appendChild(b);
  });
}
warpState.valueChangedEvent.addListener(() =>
  rateRoot.querySelectorAll(".rate").forEach((el, i) => el.classList.toggle("on", i === warpState.getChoiceIndex())));
warpState.propertiesChangedEvent.addListener(buildRate);
buildRate();

// lane switch (TIME / VOL)
const laneEls = curveEl.querySelectorAll(".ch-lane .lane");
laneEls.forEach((el) => { el.onclick = () => {
  lane = el.dataset.l; curve = curves[lane];
  laneEls.forEach(x => x.classList.toggle("on", x.dataset.l === lane));
  curveEl.classList.toggle("vol", lane === "vol");
  redrawCurve();
}; });

function showCurve(on) { curveEl.classList.toggle("show", on); scopeWrap.classList.toggle("editing", on); }
function syncCurveVisibility() { showCurve(modeState.getChoiceIndex() === 8); }
modeState.valueChangedEvent.addListener(syncCurveVisibility);

const loadLane = (arr) => arr.map(p => ({ x: +p.x, y: +p.y, c: +p.c || 0 }));
const reloadCurves = () => {
  if (!Juce.getNativeFunction) { redrawCurve(); return; }
  native("getCurves")().then((c) => {
    if (c && Array.isArray(c.time) && c.time.length >= 2) curves.time = loadLane(c.time);
    if (c && Array.isArray(c.vol)  && c.vol.length  >= 2) curves.vol  = loadLane(c.vol);
    curve = curves[lane]; redrawCurve();
  }).catch(() => redrawCurve());
};
// A preset load or A/B swap republishes the backend curves — pull them back in
// so the editor never displays (and then re-publishes) stale geometry.
window.__JUCE__.backend.addEventListener("preset", reloadCurves);
window.__JUCE__.backend.addEventListener("ab", reloadCurves);
reloadCurves();
syncCurveVisibility();

// ---- I/O meters -----------------------------------------------------------
const meterIn  = document.querySelector("#meterIn .m-bar i");
const meterOut = document.querySelector("#meterOut .m-bar i");
let mIn = 0, mOut = 0;                                   // smoothed 0..1
const setMeter = (el, v) => { el.style.height = (v * 100).toFixed(1) + "%"; };

// ---- visualiser (pushed from C++ as "vis" events) -------------------------
const scope = document.getElementById("scope");
const ctx = scope.getContext("2d");
function fit() {
  const r = scope.getBoundingClientRect(), d = window.devicePixelRatio || 1;
  scope.width = Math.max(1, r.width * d);
  scope.height = Math.max(1, r.height * d);
  ctx.setTransform(d, 0, 0, d, 0, 0);
}
window.addEventListener("resize", fit);
requestAnimationFrame(fit);

let model = { bins: [], phase: 0, delay: 0, inLvl: 0, outLvl: 0 };
window.__JUCE__.backend.addEventListener("vis", (e) => {
  model = e;
  // real, independent input/output levels from the processor (smoothed)
  mIn  = Math.max(model.inLvl  || 0, mIn  * 0.85);
  mOut = Math.max(model.outLvl || 0, mOut * 0.85);
  setMeter(meterIn, Math.min(1, mIn));
  setMeter(meterOut, Math.min(1, mOut));
  draw();
});

function draw() {
  const w = scope.clientWidth, h = scope.clientHeight, mid = h / 2;
  if (!w || !h) return;
  ctx.clearRect(0, 0, w, h);

  // faint centre line only — keep it clean
  ctx.strokeStyle = "rgba(255,255,255,.05)"; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke();

  const bins = model.bins || [];
  const n = bins.length;
  if (n > 1) {
    const amp = h * 0.44;
    const X = (i) => w * i / (n - 1);
    const Yt = (i) => mid - (bins[i] || 0) * amp;
    const Yb = (i) => mid + (bins[i] || 0) * amp;

    // vivid gradient body (cyan → blue → violet), left-to-right
    const g = ctx.createLinearGradient(0, 0, w, 0);
    g.addColorStop(0.0, "rgba(79,214,255,.26)");
    g.addColorStop(0.5, "rgba(91,140,255,.20)");
    g.addColorStop(1.0, "rgba(155,107,255,.26)");
    ctx.beginPath();
    ctx.moveTo(0, mid);
    for (let i = 0; i < n; i++) ctx.lineTo(X(i), Yt(i));
    for (let i = n - 1; i >= 0; i--) ctx.lineTo(X(i), Yb(i));
    ctx.closePath();
    ctx.fillStyle = g; ctx.fill();

    // bright gradient edge with glow
    const ge = ctx.createLinearGradient(0, 0, w, 0);
    ge.addColorStop(0.0, "#6fe0ff");
    ge.addColorStop(0.5, "#7ba6ff");
    ge.addColorStop(1.0, "#b58bff");
    ctx.lineWidth = 1.6; ctx.strokeStyle = ge;
    ctx.shadowColor = "rgba(120,150,255,.55)"; ctx.shadowBlur = 8;
    for (const Y of [Yt, Yb]) {
      ctx.beginPath();
      for (let i = 0; i < n; i++) i ? ctx.lineTo(X(i), Y(i)) : ctx.moveTo(X(i), Y(i));
      ctx.stroke();
    }
    ctx.shadowBlur = 0;
  }

  // pattern playhead (phase) — faint sweeping marker
  const phx = w * Math.min(1, Math.max(0, model.phase || 0));
  ctx.strokeStyle = "rgba(120,150,255,.28)"; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(phx, 4); ctx.lineTo(phx, h - 4); ctx.stroke();

  // read-head (delay position) — thin bright accent line + bead
  const px = w * (1 - Math.min(1, model.delay || 0) * 0.6);
  ctx.strokeStyle = "rgba(179,139,255,.9)"; ctx.lineWidth = 1.4;
  ctx.shadowColor = "rgba(155,107,255,.8)"; ctx.shadowBlur = 10;
  ctx.beginPath(); ctx.moveTo(px, 6); ctx.lineTo(px, h - 6); ctx.stroke();
  ctx.shadowBlur = 0;
  ctx.fillStyle = "#c9a8ff";
  ctx.beginPath(); ctx.arc(px, mid, 2.6, 0, Math.PI * 2); ctx.fill();
}
