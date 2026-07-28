// CHRONA WebView front-end
// Binds the six macros, mode selector and bypass to the plug-in's APVTS
// parameters via the JUCE 8 relay API, and renders the buffer visualiser and
// I/O meters from the "vis" events the processor pushes each frame.
import * as Juce from "./juce/index.js";

const MACROS = [
  ["time", "Time"], ["depth", "Depth"], ["mix", "Mix"],
  ["texture", "Texture"], ["space", "Space"], ["width", "Width"],
];

// Geometry of the machined knob (viewBox 0 0 100 100).
const A0 = -135, A1 = 135;                 // sweep, degrees
const rad = (d) => (d - 90) * Math.PI / 180;
const polar = (cx, cy, r, deg) => [cx + r * Math.cos(rad(deg)), cy + r * Math.sin(rad(deg))];
const arcPath = (cx, cy, r, a, b) => {
  const [x0, y0] = polar(cx, cy, r, a), [x1, y1] = polar(cx, cy, r, b);
  return `M ${x0} ${y0} A ${r} ${r} 0 ${b - a > 180 ? 1 : 0} 1 ${x1} ${y1}`;
};

// SVG defs (gradients) shared by every knob — injected once.
const DEFS = `
  <defs>
    <linearGradient id="arcGrad" x1="0" y1="1" x2="1" y2="0">
      <stop offset="0" stop-color="#2f5fd6"/><stop offset="0.6" stop-color="#5e95ff"/>
      <stop offset="1" stop-color="#57d6ff"/>
    </linearGradient>
    <radialGradient id="capGrad" cx="42%" cy="34%" r="72%">
      <stop offset="0" stop-color="#33383f"/><stop offset="62%" stop-color="#22262c"/>
      <stop offset="100%" stop-color="#151820"/>
    </radialGradient>
    <linearGradient id="capHi" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="rgba(255,255,255,.18)"/>
      <stop offset="0.4" stop-color="rgba(255,255,255,0)"/>
    </linearGradient>
  </defs>`;

const TICKS = 21;

function knobSVG() {
  let ticks = "";
  for (let i = 0; i < TICKS; i++) {
    const a = A0 + (A1 - A0) * i / (TICKS - 1);
    const [x0, y0] = polar(50, 50, 45, a), [x1, y1] = polar(50, 50, 40, a);
    ticks += `<line class="tick" data-i="${i}" x1="${x0.toFixed(2)}" y1="${y0.toFixed(2)}" x2="${x1.toFixed(2)}" y2="${y1.toFixed(2)}"/>`;
  }
  return `<svg viewBox="0 0 100 100">${DEFS}
    <g>${ticks}</g>
    <path class="arc-bg" d="${arcPath(50, 50, 33, A0, A1)}"/>
    <path class="arc" d="${arcPath(50, 50, 33, A0, A0)}"/>
    <circle class="cap-face" cx="50" cy="50" r="25"/>
    <circle class="cap-ring" cx="50" cy="50" r="21"/>
    <circle class="cap-hi" cx="50" cy="50" r="25"/>
    <line class="ptr" x1="50" y1="50" x2="50" y2="30"/>
  </svg>`;
}

// ---- macro knobs ----------------------------------------------------------
const knobRoot = document.getElementById("knobs");
for (const [id, label] of MACROS) {
  const state = Juce.getSliderState(id);

  const el = document.createElement("div");
  el.className = "knob";
  el.innerHTML =
    `<div class="cap">${label}</div>` +
    `<div class="dial">${knobSVG()}</div>` +
    `<div class="val">0%</div>`;
  const dial = el.querySelector(".dial");
  const arc  = el.querySelector(".arc");
  const ptr  = el.querySelector(".ptr");
  const tickEls = el.querySelectorAll(".tick");
  const val  = el.querySelector(".val");
  knobRoot.appendChild(el);

  const render = () => {
    const v = Math.min(1, Math.max(0, state.getNormalisedValue()));
    const a = A0 + (A1 - A0) * v;
    arc.setAttribute("d", arcPath(50, 50, 33, A0, Math.max(A0 + 0.01, a)));
    const [px, py] = polar(50, 50, 20, a);
    ptr.setAttribute("x2", px.toFixed(2));
    ptr.setAttribute("y2", py.toFixed(2));
    tickEls.forEach((t, i) => t.classList.toggle("lit", i / (TICKS - 1) <= v + 1e-6));
    let txt = "";
    if (state.properties && typeof state.properties.end === "number") {
      const raw = state.getScaledValue ? state.getScaledValue() : null;
      txt = raw != null ? fmt(raw) : Math.round(v * 100) + "%";
    } else txt = Math.round(v * 100) + "%";
    val.textContent = txt;
  };
  const fmt = (x) => (Math.abs(x) >= 100 ? x.toFixed(0) : Math.abs(x) >= 10 ? x.toFixed(1) : x.toFixed(2));

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

let model = { bins: [], phase: 0, delay: 0 };
window.__JUCE__.backend.addEventListener("vis", (e) => {
  model = e;
  // derive I/O levels from the buffer envelope (peak of the visible window)
  let peak = 0;
  for (const b of (model.bins || [])) { const a = Math.abs(b); if (a > peak) peak = a; }
  mOut = Math.max(peak, mOut * 0.86);
  mIn  = Math.max(peak * 0.92, mIn * 0.9);
  setMeter(meterIn, Math.min(1, mIn));
  setMeter(meterOut, Math.min(1, mOut));
  draw();
});

function draw() {
  const w = scope.clientWidth, h = scope.clientHeight, mid = h / 2;
  if (!w || !h) return;
  ctx.clearRect(0, 0, w, h);

  // baseline grid
  ctx.strokeStyle = "rgba(255,255,255,.045)"; ctx.lineWidth = 1;
  for (let i = 1; i < 8; i++) { const x = (w * i / 8) | 0; ctx.beginPath(); ctx.moveTo(x, 4); ctx.lineTo(x, h - 4); ctx.stroke(); }
  ctx.strokeStyle = "rgba(255,255,255,.07)";
  ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke();

  const bins = model.bins || [];
  const n = bins.length;
  if (n > 1) {
    const amp = h * 0.42;
    const X = (i) => w * i / (n - 1);
    const Yt = (i) => mid - (bins[i] || 0) * amp;
    const Yb = (i) => mid + (bins[i] || 0) * amp;

    // filled body (top mirror to bottom)
    const grad = ctx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0.0, "rgba(98,176,255,.34)");
    grad.addColorStop(0.5, "rgba(61,123,255,.10)");
    grad.addColorStop(1.0, "rgba(98,176,255,.34)");
    ctx.beginPath();
    ctx.moveTo(0, mid);
    for (let i = 0; i < n; i++) ctx.lineTo(X(i), Yt(i));
    for (let i = n - 1; i >= 0; i--) ctx.lineTo(X(i), Yb(i));
    ctx.closePath();
    ctx.fillStyle = grad; ctx.fill();

    // crisp edges
    ctx.lineWidth = 1.4; ctx.strokeStyle = "#8fc0ff";
    ctx.shadowColor = "rgba(61,123,255,.5)"; ctx.shadowBlur = 6;
    for (const Y of [Yt, Yb]) {
      ctx.beginPath();
      for (let i = 0; i < n; i++) i ? ctx.lineTo(X(i), Y(i)) : ctx.moveTo(X(i), Y(i));
      ctx.stroke();
    }
    ctx.shadowBlur = 0;
  }

  // read-head (delay position) — bright accent line + glow
  const px = w * (1 - Math.min(1, model.delay || 0) * 0.6);
  ctx.strokeStyle = "#62b0ff"; ctx.lineWidth = 1.6;
  ctx.shadowColor = "#62b0ff"; ctx.shadowBlur = 10;
  ctx.beginPath(); ctx.moveTo(px, 2); ctx.lineTo(px, h - 2); ctx.stroke();
  ctx.shadowBlur = 0;
  ctx.fillStyle = "#62b0ff";
  ctx.beginPath(); ctx.arc(px, mid, 2.4, 0, Math.PI * 2); ctx.fill();
}
