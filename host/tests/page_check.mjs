/* Node DOM-stub harness for the unified UI page (W03).
 *
 * Loads index.html's ids and the three page scripts in a vm context with a
 * minimal fake DOM, runs LAApp.init() and a scripted exercise of the
 * waveform/timeline renderers, then prints a JSON report. Unknown
 * getElementById lookups are failures: the app must null-guard or reference
 * an id that exists in the page (or one it creates itself).
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

const here = dirname(fileURLToPath(import.meta.url));
const staticDir = join(here, "..", "la_ui", "static");
const html = readFileSync(join(staticDir, "index.html"), "utf8");

const htmlIds = new Set();
for (const match of html.matchAll(/id="([^"]+)"/g)) {
  htmlIds.add(match[1]);
}

const unknownIds = [];
const ids = new Map();

class FakeClassList {
  constructor() { this.names = new Set(); }
  add(name) { this.names.add(name); }
  remove(name) { this.names.delete(name); }
  contains(name) { return this.names.has(name); }
  toggle(name, force) {
    if (force === undefined ? !this.names.has(name) : force) this.names.add(name);
    else this.names.delete(name);
  }
}

function fakeContext() {
  const noop = () => {};
  return new Proxy({}, {
    get(target, property) {
      if (property in target) return target[property];
      if (property === "canvas") return null;
      return noop;
    },
    set(target, property, value) { target[property] = value; return true; },
  });
}

class FakeElement {
  constructor(tag) {
    this.tagName = String(tag || "div").toUpperCase();
    this.children = [];
    this.listeners = {};
    this.style = {};
    this.dataset = {};
    this.classList = new FakeClassList();
    this._textContent = "";
    this._innerHTML = "";
    this._id = "";
    this.value = "";
    this.checked = false;
    this.disabled = false;
    this.title = "";
    this.type = "";
    this.files = null;
    this.clientWidth = 900;
    this.clientHeight = 96;
    this._context = null;
  }
  set id(value) {
    this._id = value || "";
    if (this._id) ids.set(this._id, this);
  }
  get id() { return this._id; }
  set textContent(value) { this._textContent = String(value); }
  get textContent() { return this._textContent; }
  set innerHTML(value) { this._innerHTML = String(value); this.children = []; }
  get innerHTML() { return this._innerHTML; }
  addEventListener(type, handler) {
    (this.listeners[type] = this.listeners[type] || []).push(handler);
  }
  removeEventListener(type, handler) {
    if (!this.listeners[type]) return;
    this.listeners[type] = this.listeners[type].filter((item) => item !== handler);
  }
  dispatch(type, event) {
    for (const handler of this.listeners[type] || []) handler(event || {});
  }
  appendChild(child) { this.children.push(child); return child; }
  removeChild(child) {
    this.children = this.children.filter((item) => item !== child);
    return child;
  }
  getContext() {
    if (!this._context) this._context = fakeContext();
    return this._context;
  }
  toBlob(callback) { callback(null); }
  getBoundingClientRect() {
    return { left: 0, top: 0, width: this.clientWidth, height: this.clientHeight };
  }
  click() {}
  focus() {}
  setAttribute() {}
  querySelectorAll() { return []; }
}

for (const id of htmlIds) {
  const element = new FakeElement(id.startsWith("btn-") ? "button"
    : id.startsWith("cap-") || id.startsWith("dec-") || id === "target-input"
      || id === "search-pattern" ? "input" : "div");
  element.id = id;
}

const documentStub = {
  readyState: "complete",
  getElementById(id) {
    if (ids.has(id)) return ids.get(id);
    unknownIds.push(id);
    return null;
  },
  createElement(tag) { return new FakeElement(tag); },
  addEventListener() {},
  removeEventListener() {},
};

const sandbox = {
  console,
  document: documentStub,
  devicePixelRatio: 1,
  localStorage: {
    getItem() { return null; },
    setItem() {},
    removeItem() {},
  },
  addEventListener() {},
  removeEventListener() {},
  fetch() { return new Promise(() => {}); },
  setTimeout() { return 1; },
  clearTimeout() {},
  setInterval() { return 1; },
  clearInterval() {},
  requestAnimationFrame(callback) { return 1; },
  cancelAnimationFrame() {},
  URL: { createObjectURL() { return "blob:stub"; }, revokeObjectURL() {} },
  EventSource: class {
    constructor() {}
    addEventListener() {}
    close() {}
  },
};
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);

for (const name of ["waveform.js", "timeline.js", "app.js"]) {
  const source = readFileSync(join(staticDir, name), "utf8");
  vm.runInContext(source, sandbox, { filename: name });
}

const checks = {};
try {
  const app = sandbox.LAApp;
  checks.init = !!(app && app.state && app.state.started);
  const waveform = app.state.waveform;
  checks.waveform = !!waveform;

  // Synthetic LAEN envelope: 1000 samples, 100 buckets, 8 channels.
  const total = 1000;
  const buckets = 100;
  const buffer = new ArrayBuffer(12 + 8 * buckets * 2);
  const view = new DataView(buffer);
  view.setUint8(0, 0x4c); view.setUint8(1, 0x41);
  view.setUint8(2, 0x45); view.setUint8(3, 0x4e);
  view.setUint8(4, 1);
  view.setUint8(5, 8);
  view.setUint16(6, buckets, true);
  view.setUint32(8, total, true);
  let offset = 12;
  for (let channel = 0; channel < 8; channel++) {
    for (let bucket = 0; bucket < buckets; bucket++) {
      view.setUint8(offset++, (bucket + channel) % 2);
    }
    for (let bucket = 0; bucket < buckets; bucket++) view.setUint8(offset++, 1);
  }
  waveform.setCapture({
    id: "test", total_samples: total, sample_rate_hz: 1000000,
    trigger_index: 400, pre_samples: 100, has_signal: true, encoding: 0,
  }, buffer);
  waveform.render();
  waveform.zoom(0.5, 500);
  waveform.setCursor("a");
  waveform.setCursor("b");
  waveform.zoomToCursors();
  waveform.panPixels(10);
  waveform.fit();
  await waveform.findEdge(1);
  waveform.setAnnotations({
    0: [{ kind: 1, name: "BYTE", t_sample: 100, data: "41", channel: 0, flags: 0 },
        { kind: 7, name: "ADDRESS", t_sample: 300, data: "50", channel: 1, flags: 0 }],
  });
  waveform.render();
  checks.annotationSlots = waveform.annotationSlots.length === 1;
  waveform.view = { start: 0, span: 8 };
  waveform.setDetail(0, new Uint8Array([1, 0, 1, 1, 0, 0, 1, 0]));
  waveform.pattern = "1011";
  checks.pattern = waveform.findPattern(1) === 0;
  checks.screenshot = waveform.screenshot("check") !== undefined;
  checks.measure = waveform.measure.length > 0;

  // Edge stepping: cursor A walks the selected channel's exact transitions
  // (bit 0 of 0,0,1,1,0,1,1,0 changes at samples 2, 4 and 5).
  waveform.selectedChannel = 0;
  waveform.cursors = { a: null, b: null };
  waveform.view = { start: 0, span: 8 };
  waveform.setDetail(0, new Uint8Array([0, 0, 1, 1, 0, 1, 1, 0]));
  const firstEdge = await waveform.findEdge(1);
  const secondEdge = await waveform.findEdge(1);
  const backEdge = await waveform.findEdge(-1);
  checks.edgeStep = firstEdge === 2 && secondEdge === 4 && backEdge === 2;
  checks.edgeCursor = waveform.cursors.a === 2;

  // Detail waveforms must be orthogonal steps, never diagonals: every
  // consecutive path segment is horizontal or vertical.
  const path = [];
  const recordContext = new Proxy({}, {
    get(target, property) {
      if (property === "moveTo") return (x, y) => path.push([x, y]);
      if (property === "lineTo") return (x, y) => path.push([x, y]);
      return () => {};
    },
    set(target, property, value) { target[property] = value; return true; },
  });
  const savedContext = waveform.ctx;
  waveform.ctx = recordContext;
  waveform.view = { start: 0, span: 4 };
  waveform.detail = { start: 0, samples: new Uint8Array([1, 0, 1, 0]) };
  waveform._drawDetailChannel(0, 10, 20);
  waveform.ctx = savedContext;
  let orthogonal = path.length >= 5;
  for (let index = 1; index < path.length; index++) {
    const [x0, y0] = path[index - 1];
    const [x1, y1] = path[index];
    if (x0 !== x1 && y0 !== y1) orthogonal = false;
  }
  checks.stepRendering = orthogonal;

  // Without a detail window the envelope locates the candidate bucket and a
  // fetched exact window pins the edge (1 forward edge at sample 3).
  waveform.detail = null;
  waveform.cursors = { a: null, b: null };
  waveform.view = { start: 0, span: 1000 };
  waveform.onSamplesNeeded = async (start, count) => {
    checks.edgeWindow = start === 0 && count === 11;
    return new Uint8Array([0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]);
  };
  checks.edgeEnvelope = (await waveform.findEdge(1)) === 3;

  // A transition exactly on a bucket boundary leaves both adjacent buckets
  // constant; the envelope candidate rule must still find it (4 buckets of
  // 4 samples each, CH0 goes high at sample 8).
  waveform.envelope = {
    version: 1, channels: 8, buckets: 4, total: 16, min: [], max: [],
  };
  for (let channel = 0; channel < 8; channel++) {
    waveform.envelope.min.push(new Uint8Array(4));
    waveform.envelope.max.push(new Uint8Array(4));
  }
  waveform.envelope.min[0] = new Uint8Array([0, 0, 1, 1]);
  waveform.envelope.max[0] = new Uint8Array([0, 0, 1, 1]);
  waveform.meta = { total_samples: 16, sample_rate_hz: 1000000,
                    has_signal: true };
  waveform.selectedChannel = 0;
  waveform.cursors = { a: null, b: null };
  waveform.view = { start: 0, span: 16 };
  waveform.detail = null;
  waveform.onSamplesNeeded = async (start, count) => {
    checks.boundaryWindow = start === 7 && count === 6;
    return new Uint8Array([0, 1, 1, 1, 1, 1]);
  };
  checks.edgeBoundary = (await waveform.findEdge(1)) === 8;

  const live = app.state.live[0];
  live.add([[100, 1, "41"], [200, 1, "42"], [300, 4, ""]]);
  live.render();
  checks.liveEvents = live.events.length === 3;
  const flood = [];
  for (let index = 0; index < 120000; index++) flood.push([index, 1, "00"]);
  live.add(flood);
  checks.liveBound = live.events.length === live.maxEvents && live.dropped > 0;

  const trace = app.state.traceTimeline;
  trace.setModel({
    lanes: [{ id: 1, name: "acq", kind: "task" }],
    spans: { "1": [[0, 10]] },
    markers: [{ t_ms: 5, token: "QS", lane: 0, group: "queue" }],
    summary: { records: 1, dropped: 0 },
    duration_ms: 10,
  });
  trace.render();
  checks.trace = true;
  checks.traceLanes = trace.model.lanes.length === 1;

  app.updateStats({
    state: "streaming", events: 1234, dropped: 7, ring_dropped: 3,
    client_dropped: 4, slots: ["streaming", "idle", "unconfigured"],
    slot_labels: ["uart:rx=0,baud=115200", "", ""],
  });
  checks.stats = documentStub.getElementById("sb-events").textContent ===
    "events 1234";
  checks.statsPills = documentStub.getElementById("live-pill-0").textContent ===
    "streaming";
  checks.slotStartDisabled =
    documentStub.getElementById("live-start-0").disabled === true;

  checks.livePanes = app.state.live.length === 3;
  checks.channelRows = documentStub.getElementById("chan-list").children.length === 8;
} catch (error) {
  checks.error = String(error && error.stack || error);
}

const report = {
  ok: !checks.error && unknownIds.length === 0 &&
      Object.values(checks).every((value) => value !== false),
  unknownIds: Array.from(new Set(unknownIds)),
  checks,
};
console.log(JSON.stringify(report, null, 2));
if (report.ok) console.log("PAGE CHECK OK");
process.exit(report.ok ? 0 : 1);
