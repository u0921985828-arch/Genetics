// CHRONA WebView front-end — binds to the plug-in's APVTS parameters through
// the JUCE 8 relay API and draws the buffer visualiser from "vis" events.
import * as Juce from "./juce/index.js";

const MACROS = [
  ["time", "Time"], ["depth", "Depth"], ["mix", "Mix"],
  ["texture", "Texture"], ["space", "Space"], ["width", "Width"],
];

// ---- macro knobs ----------------------------------------------------------
const knobRoot = document.getElementById("knobs");
for (const [id, label] of MACROS) {
  const state = Juce.getSliderState(id);

  const el = document.createElement("div");
  el.className = "knob";
  el.innerHTML =
    `<div class="cap">${label}</div>` +
    `<div class="dial"><div class="ptr"></div></div>` +
    `<div class="val">0%</div>`;
  const dial = el.querySelector(".dial");
  const val  = el.querySelector(".val");
  knobRoot.appendChild(el);

  const render = () => {
    const v = state.getNormalisedValue();
    dial.style.setProperty("--v", v);
    val.textContent = Math.round(v * 100) + "%";
  };
  state.valueChangedEvent.addListener(render);
  render();

  // vertical drag → normalised value
  let dragging = false, lastY = 0;
  const down = (e) => { dragging = true; lastY = (e.touches ? e.touches[0] : e).clientY; e.preventDefault(); };
  const move = (e) => {
    if (!dragging) return;
    const y = (e.touches ? e.touches[0] : e).clientY;
    const v = Math.min(1, Math.max(0, state.getNormalisedValue() - (y - lastY) / 180));
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
}

// ---- mode selector --------------------------------------------------------
const modeState = Juce.getComboBoxState("mode");
const modeRoot = document.getElementById("modes");
function buildModes() {
  modeRoot.innerHTML = "";
  const choices = modeState.properties.choices || [];
  choices.forEach((name, i) => {
    const b = document.createElement("div");
    b.className = "mode" + (i === modeState.getChoiceIndex() ? " on" : "");
    b.textContent = name;
    b.onclick = () => modeState.setChoiceIndex(i);
    modeRoot.appendChild(b);
  });
}
modeState.valueChangedEvent.addListener(() => {
  modeRoot.querySelectorAll(".mode").forEach((el, i) =>
    el.classList.toggle("on", i === modeState.getChoiceIndex()));
});
modeState.propertiesChangedEvent.addListener(buildModes);
buildModes();

// ---- bypass ---------------------------------------------------------------
const bypassState = Juce.getToggleState("bypass");
const bypassEl = document.getElementById("bypass");
const syncBypass = () => bypassEl.classList.toggle("on", bypassState.getValue());
bypassEl.onclick = () => bypassState.setValue(!bypassState.getValue());
bypassState.valueChangedEvent.addListener(syncBypass);
syncBypass();

// ---- visualiser (pushed from C++ as "vis" events) -------------------------
const scope = document.getElementById("scope");
const ctx = scope.getContext("2d");
function fit() {
  const r = scope.getBoundingClientRect(), d = window.devicePixelRatio || 1;
  scope.width = r.width * d; scope.height = r.height * d; ctx.setTransform(d, 0, 0, d, 0, 0);
}
window.addEventListener("resize", fit); fit();

let model = { bins: [], phase: 0, delay: 0 };
window.__JUCE__.backend.addEventListener("vis", (e) => { model = e; draw(); });

function draw() {
  const w = scope.clientWidth, h = scope.clientHeight, mid = h / 2;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = "#33373d"; ctx.globalAlpha = .5;
  for (let i = 1; i < 8; i++) { const x = (w * i / 8) | 0; ctx.beginPath(); ctx.moveTo(x, 6); ctx.lineTo(x, h - 6); ctx.stroke(); }
  ctx.globalAlpha = 1; ctx.strokeStyle = "#b6b9c0"; ctx.lineWidth = 1.25;
  const n = model.bins.length || 1;
  for (const s of [-1, 1]) {
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = w * i / (n - 1), y = mid + s * (model.bins[i] || 0) * h * 0.44;
      i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    }
    ctx.stroke();
  }
  const px = w * (1 - Math.min(1, model.delay || 0) * 0.35);
  ctx.strokeStyle = "#2f7bff"; ctx.beginPath(); ctx.moveTo(px, 6); ctx.lineTo(px, h - 6); ctx.stroke();
}
