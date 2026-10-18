/* Unified analyzer UI (W03-W07): connection, capture, decode, live, trace,
 * pattern generator, exports, offline open and settings persistence.
 *
 * The page is a plain browser client of the /api routes in
 * host/la_ui/server.py. One analyzer connection, one live SSE feed; the
 * server enforces the capture/live exclusivity and the page reflects it.
 */
(function () {
  "use strict";

  const PAGE_VERSION = "w11";
  const SETTINGS_KEY = "la-ui.settings.v1";
  const CHANNELS_KEY = "la-ui.channels.v1";
  const PATGEN_MODES = ["off", "counter", "walking", "prbs", "uart", "spi",
                        "i2c", "uart_text", "uart_bin", "uart_ferr",
                        "spi_multi", "spi16", "i2c_read", "i2c_nack",
                        "i2c_alt"];

  const state = {
    started: false,
    connected: false,
    target: null,
    pong: null,
    capture: null,
    job: null,
    jobTimer: null,
    session: "idle",
    slots: ["unconfigured", "unconfigured", "unconfigured"],
    slotLabels: ["", "", ""],
    settings: {},
    channels: null,
    waveform: null,
    traceTimeline: null,
    live: [],
    events: null,
    stateTimer: null,
    detailTimer: null,
    detailPending: null,
    detailKey: null,
    lastCaptureId: null,
    edgePending: false,
    liveFeedWarned: false,
    generation: 0,
    resetting: false,
  };

  function $(id) {
    return document.getElementById(id);
  }

  function text(id, value) {
    const element = $(id);
    if (element) element.textContent = value === null ||
      value === undefined ? "" : String(value);
  }

  function setDisabled(id, disabled) {
    const element = $(id);
    if (element) element.disabled = !!disabled;
  }

  function setClass(id, name, on) {
    const element = $(id);
    if (element && element.classList) {
      if (on) element.classList.add(name);
      else element.classList.remove(name);
    }
  }

  function value(id) {
    const element = $(id);
    return element ? element.value : "";
  }

  function toast(message, kind) {
    const element = $("toast");
    if (!element) return;
    element.textContent = message;
    element.className = "toast" + (kind ? " " + kind : "");
    clearTimeout(toast._timer);
    toast._timer = setTimeout(() => {
      element.className = "toast hidden";
    }, 6000);
  }

  async function api(path, options) {
    const settings = options || {};
    const init = { method: settings.method || "GET" };
    if (settings.body !== undefined) {
      if (settings.raw) {
        init.body = settings.body;
      } else {
        init.headers = { "Content-Type": "application/json" };
        init.body = JSON.stringify(settings.body);
      }
    }
    const response = await fetch(path, init);
    const raw = await response.text();
    let payload = null;
    try {
      payload = raw ? JSON.parse(raw) : null;
    } catch (error) {
      payload = { error: raw };
    }
    if (!response.ok) {
      throw new Error((payload && payload.error) || ("HTTP " + response.status));
    }
    return payload;
  }

  // -- settings ---------------------------------------------------------------

  function loadSettings() {
    try {
      const stored = window.localStorage.getItem(SETTINGS_KEY);
      return stored ? JSON.parse(stored) : {};
    } catch (error) {
      return {};
    }
  }

  function saveSettings() {
    const settings = state.settings || {};
    settings.target = value("target-input");
    settings.capture = captureOptions();
    settings.decoders = decoderSpecs();
    settings.patgen = value("patgen-mode");
    try {
      window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
    } catch (error) { /* private mode */ }
  }

  function loadChannels() {
    const channels = window.LAWaveform.defaultChannels();
    try {
      const stored = window.localStorage.getItem(CHANNELS_KEY);
      if (!stored) return channels;
      const parsed = JSON.parse(stored);
      if (!Array.isArray(parsed) || parsed.length !== 8) return channels;
      return channels.map((channel, index) => Object.assign(channel,
        parsed[index] || {}, { index }));
    } catch (error) {
      return channels;
    }
  }

  function saveChannels() {
    try {
      window.localStorage.setItem(CHANNELS_KEY,
        JSON.stringify(state.channels));
    } catch (error) { /* private mode */ }
  }

  function applySettingsToForm() {
    const settings = state.settings || {};
    if (settings.target) {
      const input = $("target-input");
      if (input) input.value = settings.target;
    }
    if (settings.capture) {
      const capture = settings.capture;
      if ($("cap-rate")) $("cap-rate").value = capture.rate || "10M";
      if ($("cap-words")) $("cap-words").value = capture.words || "64k";
      if ($("cap-mode")) $("cap-mode").value = capture.mode || "raw";
      if ($("cap-pre")) $("cap-pre").value = capture.pre || 0;
      if ($("cap-trig-rise")) $("cap-trig-rise").value =
        capture.trig_rise === null || capture.trig_rise === undefined
          ? "" : capture.trig_rise;
      if ($("cap-trig-fall")) $("cap-trig-fall").value =
        capture.trig_fall === null || capture.trig_fall === undefined
          ? "" : capture.trig_fall;
      if ($("cap-patgen")) $("cap-patgen").value = capture.patgen || "off";
      if ($("cap-timeout")) $("cap-timeout").value = capture.timeout || 30;
    }
    const decoders = settings.decoders || ["uart:rx=0,baud=115200", "", ""];
    for (let slot = 0; slot < 3; slot++) {
      const input = $("dec-slot-" + slot);
      if (input && decoders[slot] !== undefined) input.value = decoders[slot];
    }
    if ($("patgen-mode")) $("patgen-mode").value = settings.patgen || "off";
  }

  function captureOptions() {
    const rise = value("cap-trig-rise");
    const fall = value("cap-trig-fall");
    return {
      rate: value("cap-rate") || "10M",
      words: value("cap-words") || "64k",
      mode: value("cap-mode") || "raw",
      pre: Number(value("cap-pre") || 0),
      trig_rise: rise === "" ? null : Number(rise),
      trig_fall: fall === "" ? null : Number(fall),
      patgen: value("cap-patgen") || "off",
      timeout: Number(value("cap-timeout") || 30),
    };
  }

  function decoderSpecs() {
    return [value("dec-slot-0"), value("dec-slot-1"), value("dec-slot-2")];
  }

  // -- static UI --------------------------------------------------------------

  function buildPatgenOptions() {
    for (const id of ["cap-patgen", "patgen-mode"]) {
      const select = $(id);
      if (!select) continue;
      select.innerHTML = "";
      for (const mode of PATGEN_MODES) {
        const option = document.createElement("option");
        option.value = mode;
        option.textContent = mode;
        select.appendChild(option);
      }
      select.value = "off";
    }
  }

  function buildChannelList() {
    const container = $("chan-list");
    if (!container) return;
    container.innerHTML = "";
    state.channels.forEach((channel, index) => {
      const row = document.createElement("div");
      row.className = "chan-row" +
        (index === state.waveform.selectedChannel ? " selected" : "");
      const toggle = document.createElement("input");
      toggle.type = "checkbox";
      toggle.checked = channel.visible;
      toggle.title = "show " + channel.name;
      toggle.addEventListener("change", () => {
        channel.visible = toggle.checked;
        saveChannels();
        state.waveform.setChannels(state.channels);
      });
      const color = document.createElement("input");
      color.type = "color";
      color.value = channel.color;
      color.addEventListener("change", () => {
        channel.color = color.value;
        saveChannels();
        state.waveform.setChannels(state.channels);
      });
      const label = document.createElement("span");
      label.className = "chan-index";
      label.textContent = "CH" + index;
      label.addEventListener("click", () => selectChannel(index));
      const name = document.createElement("input");
      name.type = "text";
      name.value = channel.name;
      name.addEventListener("change", () => {
        channel.name = name.value || "CH" + index;
        saveChannels();
        state.waveform.setChannels(state.channels);
      });
      const up = document.createElement("button");
      up.textContent = "▲";
      up.title = "move up";
      up.addEventListener("click", () => moveChannel(index, -1));
      const down = document.createElement("button");
      down.textContent = "▼";
      down.title = "move down";
      down.addEventListener("click", () => moveChannel(index, 1));
      row.appendChild(toggle);
      row.appendChild(color);
      row.appendChild(label);
      row.appendChild(name);
      row.appendChild(up);
      row.appendChild(down);
      container.appendChild(row);
    });
  }

  function moveChannel(index, delta) {
    const target = index + delta;
    if (target < 0 || target >= state.channels.length) return;
    const channels = state.channels;
    const moved = channels.splice(index, 1)[0];
    channels.splice(target, 0, moved);
    channels.forEach((channel, position) => { channel.index = position; });
    saveChannels();
    buildChannelList();
    state.waveform.setChannels(channels);
  }

  function selectChannel(index) {
    state.waveform.selectedChannel = index;
    buildChannelList();
    state.waveform.render();
  }

  function buildLivePanes() {
    const container = $("live-panes");
    if (!container) return;
    container.innerHTML = "";
    state.live = [];
    for (let slot = 0; slot < 3; slot++) {
      const pane = document.createElement("div");
      pane.className = "live-pane";
      pane.id = "live-pane-" + slot;
      const head = document.createElement("div");
      head.className = "live-head";
      const pill = document.createElement("span");
      pill.className = "pill slot-state";
      pill.id = "live-pill-" + slot;
      pill.textContent = "unconfigured";
      const label = document.createElement("span");
      label.className = "slot-label";
      label.id = "live-label-" + slot;
      label.textContent = "slot " + slot;
      const spacer = document.createElement("span");
      spacer.className = "spacer";
      const start = document.createElement("button");
      start.className = "ghost";
      start.id = "live-start-" + slot;
      start.textContent = "Start";
      start.addEventListener("click", () => liveStart(slot));
      const stop = document.createElement("button");
      stop.className = "ghost";
      stop.id = "live-stop-" + slot;
      stop.textContent = "Stop";
      stop.addEventListener("click", () => liveStop(slot));
      head.appendChild(pill);
      head.appendChild(label);
      head.appendChild(spacer);
      head.appendChild(start);
      head.appendChild(stop);
      const canvas = document.createElement("canvas");
      canvas.className = "live-canvas";
      canvas.id = "live-canvas-" + slot;
      const foot = document.createElement("div");
      foot.className = "live-foot dim";
      foot.id = "live-last-" + slot;
      foot.textContent = "no events";
      pane.appendChild(head);
      pane.appendChild(canvas);
      pane.appendChild(foot);
      container.appendChild(pane);
      const timeline = new window.LATimeline.LiveWaveform(canvas, slot);
      timeline.onInfo = message => text("live-last-" + slot, message);
      const controls = document.createElement("div");
      controls.className = "row wrap live-controls";
      for (const [label, action] of [["Pause view", () => timeline.pause()],
        ["Follow", () => timeline.resume()], ["Fit history", () => timeline.fit()],
        ["+", () => timeline.zoom(0.5)], ["−", () => timeline.zoom(2)]]) {
        const button = document.createElement("button");
        button.className = "ghost";
        button.textContent = label;
        button.addEventListener("click", action);
        controls.appendChild(button);
      }
      pane.appendChild(controls);
      state.live.push(timeline);
    }
  }

  function bindUi() {
    const on = (id, event, handler) => {
      const element = $(id);
      if (element) element.addEventListener(event, handler);
    };
    on("btn-refresh-ports", "click", refreshPorts);
    on("port-select", "change", () => {
      const select = $("port-select");
      if (select && select.value) {
        const input = $("target-input");
        if (input) input.value = "serial://" + select.value;
      }
    });
    on("btn-connect", "click", connect);
    on("btn-disconnect", "click", disconnect);
    on("btn-ping", "click", ping);
    on("btn-reset-ui", "click", resetUi);
    on("btn-capture", "click", startCapture);
    on("btn-abort", "click", abortJob);
    on("btn-decode", "click", decodeCapture);
    on("btn-live-config", "click", applyLiveConfig);
    on("btn-live-stop-all", "click", liveStopAll);
    on("btn-export-sr", "click", () => exportCapture("sr"));
    on("btn-export-vcd", "click", () => exportCapture("vcd"));
    on("btn-export-png", "click", () => {
      if (state.waveform.screenshot(state.capture ? state.capture.name
                                                  : "waveform")) {
        text("export-status", "screenshot saved");
      }
    });
    on("btn-export-csv-events", "click", () => exportCapture("csv-events"));
    on("btn-export-csv-samples", "click", () => exportCapture("csv-samples"));
    on("open-file", "change", (event) => {
      const file = event.target.files && event.target.files[0];
      if (file) openFile(file);
    });
    on("btn-trace-start", "click", traceStart);
    on("btn-trace-stop", "click", traceStop);
    on("btn-trace-dump", "click", traceDump);
    on("btn-patgen", "click", armPatgen);
    on("btn-fit", "click", () => state.waveform.fit());
    on("btn-focus", "click", () => state.waveform.focusActivity());
    on("btn-zoom-in", "click", () => state.waveform.zoom(0.8));
    on("btn-zoom-out", "click", () => state.waveform.zoom(1.25));
    on("btn-zoom-cursors", "click", () => state.waveform.zoomToCursors());
    on("btn-cursor-a", "click", () => state.waveform.setCursor("a"));
    on("btn-cursor-b", "click", () => state.waveform.setCursor("b"));
    on("btn-prev-edge", "click", () => stepEdge(-1));
    on("btn-next-edge", "click", () => stepEdge(1));
    on("btn-search-next", "click", () => searchPattern(1));
    on("btn-search-prev", "click", () => searchPattern(-1));
    window.addEventListener("resize", resizeAll);
  }

  function searchPattern(direction) {
    state.waveform.pattern = value("search-pattern");
    if (!state.waveform.pattern) {
      toast("enter a 0/1/x pattern first", "warn");
      return;
    }
    if (!state.waveform._detailCoversView()) {
      state.waveform._maybeRequestDetail();
      toast("fetching detail for the current view; press again", "warn");
      return;
    }
    const time = state.waveform.findPattern(direction);
    text("view-status", time === null
      ? "pattern not found in the current view"
      : "pattern at sample " + time);
  }

  async function stepEdge(direction) {
    if (state.edgePending) return;
    state.edgePending = true;
    try {
      const time = await state.waveform.findEdge(direction);
      text("view-status", time === null
        ? (direction > 0 ? "no later edge" : "no earlier edge")
        : "edge at sample " + time);
    } finally {
      state.edgePending = false;
    }
  }

  function resizeAll() {
    if (state.waveform) state.waveform.resize();
    for (const timeline of state.live) timeline.resize();
    if (state.traceTimeline) state.traceTimeline.resize();
  }

  // -- server state -----------------------------------------------------------

  function setServerError(error) {
    state.connected = false;
    setClass("link-pill", "on", false);
    setClass("link-pill", "bad", true);
    text("link-pill", "server error");
    text("status-text", error ? error.message : "");
  }

  function updateFromState(snapshot) {
    state.connected = !!snapshot.connected;
    state.target = snapshot.target;
    state.pong = snapshot.pong;
    state.session = snapshot.session || "idle";
    state.slots = snapshot.slots || state.slots;
    state.slotLabels = snapshot.slot_labels || state.slotLabels;
    state.waveformSupported = snapshot.waveform_supported;
    text("page-version", `page ${PAGE_VERSION} | server ${snapshot.version}`);

    setClass("link-pill", "on", state.connected);
    setClass("link-pill", "bad", false);
    text("link-pill", state.connected ? "linked" : "offline");
    text("fw-info", state.pong
      ? `fw 0x${Number(state.pong.fw_version).toString(16).padStart(8, "0")} ` +
        `pl 0x${Number(state.pong.pl_id).toString(16).padStart(8, "0")}`
      : "");

    setDisabled("btn-connect", state.connected);
    setDisabled("btn-disconnect", !state.connected);
    setDisabled("btn-ping", !state.connected);
    setDisabled("btn-capture", !state.connected || !!state.job);
    setDisabled("btn-live-config", !state.connected || state.session !== "idle");
    setDisabled("btn-trace-start", !state.connected || state.session !== "idle");
    setDisabled("btn-trace-stop", !state.connected || state.session !== "idle");
    setDisabled("btn-trace-dump", !state.connected || state.session !== "idle");
    setDisabled("btn-patgen", !state.connected || state.session !== "idle");
    applySlots();
    if (snapshot.stats) updateStats(snapshot.stats);
    text("sb-session", `session ${state.session}`);
    if (state.job) {
      text("cap-status", `${state.job.label}: ${Math.round(
        state.job.progress * 100)}%`);
      setDisabled("btn-abort", false);
    } else {
      setDisabled("btn-abort", true);
    }
    if (snapshot.captures && snapshot.captures.length) {
      text("capture-sub", `${snapshot.captures.length} capture(s) in the session`);
    }
    if (snapshot.trace_ready && !state.traceTimeline.model) {
      loadTraceModel();
    }
  }

  function applySlots() {
    for (let slot = 0; slot < 3; slot++) {
      const pill = $("live-pill-" + slot);
      const slotState = state.slots[slot] || "unconfigured";
      if (pill) {
        pill.textContent = slotState;
        pill.className = "pill slot-state" +
          (slotState === "streaming" ? " on" : "");
      }
      text("live-label-" + slot, state.slotLabels[slot] || "slot " + slot +
        " (unconfigured)");
      if (state.live[slot]) state.live[slot].configure(state.slotLabels[slot] || "");
      const pane = $("live-pane-" + slot);
      if (pane) pane.style.display = state.slotLabels[slot] ? "" : "none";
      setDisabled("live-start-" + slot,
        !state.connected || !state.slotLabels[slot] ||
        slotState === "streaming");
      setDisabled("live-stop-" + slot, slotState !== "streaming");
    }
    setDisabled("btn-live-stop-all", state.session === "idle");
    text("sb-session", `session ${state.session}`);
  }

  async function refreshState() {
    const generation = state.generation;
    try {
      const snapshot = await api("/api/state");
      if (generation !== state.generation || state.resetting) return;
      updateFromState(snapshot);
    } catch (error) {
      setServerError(error);
    }
  }

  async function refreshPorts() {
    try {
      const payload = await api("/api/ports");
      const select = $("port-select");
      if (!select) return;
      select.innerHTML = "";
      const placeholder = document.createElement("option");
      placeholder.value = "";
      placeholder.textContent = payload.ports.length
        ? "scan ports…" : "no serial ports";
      select.appendChild(placeholder);
      for (const port of payload.ports) {
        const option = document.createElement("option");
        option.value = port.device;
        option.textContent = `${port.device} — ${port.description}` +
          (port.vid_pid ? ` (${port.vid_pid})` : "") +
          (port.analyzer ? " *" : "");
        select.appendChild(option);
      }
    } catch (error) {
      toast(`ports: ${error.message}`, "error");
    }
  }

  async function connect() {
    const generation = state.generation;
    const target = value("target-input").trim();
    if (!target) {
      toast("select a port or type serial://COMx", "warn");
      return;
    }
    text("status-text", "connecting…");
    try {
      const payload = await api("/api/connect", { method: "POST",
                                                  body: { target } });
      if (generation !== state.generation) return;
      state.connected = true;
      state.pong = payload.pong;
      saveSettings();
      text("status-text", "");
      toast(`connected to ${target}`);
      await refreshState();
    } catch (error) {
      text("status-text", "");
      toast(`connect: ${error.message}`, "error");
    }
  }

  async function disconnect() {
    try {
      await api("/api/disconnect", { method: "POST" });
      toast("disconnected");
      await refreshState();
    } catch (error) {
      toast(`disconnect: ${error.message}`, "error");
    }
  }

  async function resetUi() {
    if (state.resetting) return;
    state.resetting = true;
    state.generation++;
    setDisabled("btn-reset-ui", true);
    clearInterval(state.jobTimer);
    clearTimeout(state.detailTimer);
    state.jobTimer = state.detailTimer = null;
    if (state.events) state.events.close();
    state.events = null;
    try {
      const snapshot = await api("/api/reset", { method: "POST", body: {} });
      state.capture = state.job = state.detailPending = state.detailKey = null;
      state.lastCaptureId = null;
      state.channels = window.LAWaveform.defaultChannels();
      state.settings = { target: "", capture: { rate: "10M", words: "64k", mode: "raw",
        pre: 0, trig_rise: null, trig_fall: null, patgen: "off", timeout: 30 },
        decoders: ["uart:rx=0,baud=115200", "", ""], patgen: "off" };
      try {
        window.localStorage.removeItem(SETTINGS_KEY);
        window.localStorage.removeItem(CHANNELS_KEY);
      } catch (error) { /* private mode */ }
      $("target-input").value = "";
      $("port-select").value = "";
      $("search-pattern").value = "";
      $("open-file").value = "";
      applySettingsToForm();
      state.waveform.selectedChannel = 0;
      state.waveform.pattern = "";
      state.waveform.setChannels(state.channels);
      state.waveform.setCapture(null, null);
      state.traceTimeline.setModel(null);
      buildChannelList();
      for (const timeline of state.live) timeline.clear();
      for (let slot = 0; slot < 3; slot++) text("live-last-" + slot, "no events");
      for (const [id, message] of Object.entries({ "capture-title": "no capture",
        "capture-sub": "", "cap-status": "idle", "decode-status": "no capture",
        "view-status": "", "measure-text": "no capture", "export-status": "",
        "trace-status": "no dump", "trace-summary": "no dump", "patgen-status": "",
        "sb-warning": "", "live-summary": "idle", "sb-events": "events 0",
        "sb-drops": "drops 0" })) text(id, message);
      $("wave-empty").className = "empty";
      $("trace-empty").className = "empty";
      setExportEnabled(false);
      setDisabled("btn-decode", true);
      updateFromState(snapshot);
      toast("UI reset to defaults — select the analyzer port to reconnect");
    } catch (error) {
      toast(`reset: ${error.message}`, "error");
    } finally {
      state.resetting = false;
      setDisabled("btn-reset-ui", false);
      connectEvents();
      refreshState();
    }
  }

  async function ping() {
    try {
      const payload = await api("/api/ping");
      const pong = payload.pong;
      toast(`pong: fw 0x${Number(pong.fw_version).toString(16)} ` +
            `pl 0x${Number(pong.pl_id).toString(16)} ` +
            `uptime ${pong.uptime_ms} ms`);
    } catch (error) {
      toast(`ping: ${error.message}`, "error");
    }
  }

  // -- capture ----------------------------------------------------------------

  function watchJob(jobId, label, onDone) {
    const generation = state.generation;
    state.job = { id: jobId, label, progress: 0 };
    if (state.jobTimer) clearInterval(state.jobTimer);
    setDisabled("btn-abort", false);
    state.jobTimer = setInterval(async () => {
      let job;
      try {
        job = await api(`/api/jobs/${jobId}`);
        if (generation !== state.generation) return;
      } catch (error) {
        clearInterval(state.jobTimer);
        state.jobTimer = null;
        state.job = null;
        toast(`${label}: ${error.message}`, "error");
        return;
      }
      state.job = { id: jobId, label, progress: job.progress };
      if (job.state === "running" || job.state === "queued") return;
      clearInterval(state.jobTimer);
      state.jobTimer = null;
      state.job = null;
      setDisabled("btn-abort", true);
      if (job.state === "done") {
        onDone(job);
      } else {
        toast(`${label} ${job.state}: ${job.error || ""}`, "error");
      }
      refreshState();
    }, 250);
  }

  async function startCapture() {
    const generation = state.generation;
    if (!state.connected) return;
    saveSettings();
    if (state.session !== "idle") {
      try {
        await api("/control/stop", { method: "POST" });
      } catch (error) { /* the server also auto-stops */ }
    }
    let payload;
    try {
      payload = await api("/api/capture", { method: "POST",
                                            body: captureOptions() });
      if (generation !== state.generation) return;
    } catch (error) {
      toast(`capture: ${error.message}`, "error");
      return;
    }
    text("cap-status", "capture: queued");
    watchJob(payload.job.id, "capture", async (job) => {
      const capture = job.result.capture;
      text("cap-status", `captured ${capture.total_samples} samples ` +
        `(${capture.encoding ? "txn" : "raw"})`);
      text("sb-warning", "");
      if (capture.warnings && capture.warnings.length) {
        toast(capture.warnings[0], "warn");
        text("sb-warning", capture.warnings[0]);
      }
      await loadCapture(capture);
    });
  }

  async function abortJob() {
    if (!state.job) return;
    try {
      await api(`/api/jobs/${state.job.id}/cancel`, { method: "POST" });
      toast("cancelling…");
    } catch (error) {
      toast(`abort: ${error.message}`, "error");
    }
  }

  async function loadCapture(meta) {
    const generation = state.generation;
    state.capture = meta;
    state.detailKey = null;
    state.lastCaptureId = meta.id;
    const canvas = $("wave-canvas");
    const width = canvas && canvas.clientWidth ? canvas.clientWidth : 900;
    let envelope = null;
    if (meta.has_signal) {
      try {
        const response = await fetch(
          `/api/captures/${meta.id}/envelope?buckets=${Math.max(256, Math.round(width))}`);
        if (response.ok) envelope = await response.arrayBuffer();
      } catch (error) {
        toast(`envelope: ${error.message}`, "error");
      }
    }
    if (generation !== state.generation || state.capture !== meta) return;
    state.waveform.setCapture(meta, envelope);
    await state.waveform.focusActivity();
    if (generation !== state.generation || state.capture !== meta) return;
    text("capture-title", `${meta.name} — ${meta.total_samples} samples ` +
      `@ ${meta.sample_rate_hz} Hz (${meta.encoding ? "txn" : "raw"})`);
    text("view-status", "");
    text("measure-text", "no cursors");
    const annotations = await fetchAnnotations(meta.id);
    if (generation !== state.generation || state.capture !== meta) return;
    state.waveform.setAnnotations(annotations);
    const eventCount = Object.values(annotations).reduce(
      (total, events) => total + events.length, 0);
    text("decode-status", eventCount
      ? `${eventCount} decoded events loaded`
      : "no decoded events; press Decode capture");
    setExportEnabled(true);
    setDisabled("btn-decode", false);
    const empty = $("wave-empty");
    if (empty) empty.className = meta.has_signal ? "empty hidden" : "empty";
  }

  async function fetchAnnotations(captureId) {
    try {
      const payload = await api(`/api/captures/${captureId}/annotations`);
      return payload.annotations || {};
    } catch (error) {
      return {};
    }
  }

  function setExportEnabled(enabled) {
    for (const id of ["btn-export-sr", "btn-export-vcd", "btn-export-png",
                      "btn-export-csv-events", "btn-export-csv-samples"]) {
      setDisabled(id, !enabled);
    }
  }

  async function decodeCapture() {
    const generation = state.generation;
    if (!state.capture) {
      toast("capture something first", "warn");
      return;
    }
    const specs = decoderSpecs();
    try {
      const payload = await api(
        `/api/captures/${state.capture.id}/decode`,
        { method: "POST", body: { specs } });
      if (generation !== state.generation) return;
      text("decode-status", "decoding…");
      watchJob(payload.job.id, "decode", async (job) => {
        const counts = job.result.counts || {};
        const summary = Object.entries(counts)
          .map(([slot, count]) => `slot ${slot}: ${count}`).join(", ");
        text("decode-status", summary ? `decoded ${summary}` : "no events");
        await loadCapture(state.capture);
      });
    } catch (error) {
      toast(`decode: ${error.message}`, "error");
    }
  }

  function exportCapture(format) {
    if (!state.capture) return;
    const link = document.createElement("a");
    link.href = `/api/captures/${state.capture.id}/export?format=${format}`;
    link.download = "";
    link.click();
    text("export-status", `downloading ${format}…`);
  }

  async function openFile(file) {
    const generation = state.generation;
    text("export-status", `opening ${file.name}…`);
    try {
      const buffer = await file.arrayBuffer();
      const payload = await api(
        `/api/open?name=${encodeURIComponent(file.name)}`,
        { method: "POST", body: buffer, raw: true });
      if (generation !== state.generation) return;
      toast(`opened ${file.name}`);
      text("export-status", `opened ${file.name}`);
      await loadCapture(payload.capture);
    } catch (error) {
      text("export-status", "");
      toast(`open: ${error.message}`, "error");
    }
  }

  // -- live -------------------------------------------------------------------

  function connectEvents() {
    if (state.events || typeof window.EventSource !== "function") return;
    const source = new window.EventSource("/events");
    state.events = source;
    source.onopen = () => {
      state.liveFeedWarned = false;
    };
    source.onmessage = (event) => {
      let message;
      try {
        message = JSON.parse(event.data);
      } catch (error) {
        return;
      }
      if (message.stats) {
        updateStats(message.stats);
        return;
      }
      if (message.waveform) {
        for (let slot = 0; slot < 3; slot++) {
          if (state.slotLabels[slot] && state.live[slot]) state.live[slot].addWaveform(message.waveform);
        }
        return;
      }
      if (typeof message.s === "number" && Array.isArray(message.e)) {
        const timeline = state.live[message.s];
        if (timeline) timeline.add(message.e);
        const last = message.e[message.e.length - 1];
        if (last) {
          const kind = window.LATimeline.KIND_NAMES[last[1]] || last[1];
          text("live-last-" + message.s,
               `${kind} ${last[2] || ""} t=${last[0]}`);
        }
      }
    };
    source.onerror = () => {
      if (!state.liveFeedWarned) {
        state.liveFeedWarned = true;
        toast("Live feed disconnected; close extra UI tabs or reload the page", "warn");
      }
    };
  }

  function updateStats(stats) {
    state.session = stats.state || state.session;
    state.slots = stats.slots || state.slots;
    state.slotLabels = stats.slot_labels || state.slotLabels;
    applySlots();
    text("sb-events", `events ${stats.events || 0}`);
    text("sb-drops", `drops ${stats.dropped || 0} ` +
      `(ring ${stats.ring_dropped || 0}, client ${stats.client_dropped || 0})`);
    text("live-summary", state.session === "streaming"
      ? `streaming — ${stats.events || 0} events` : "idle");
    text("live-wave-status", stats.error || (stats.waveform_frames
      ? `${stats.waveform_frames} real waveform packets`
      : state.connected && state.waveformSupported === false
        ? "Live waveform requires firmware 0x000A0000 or newer"
        : "waiting for real waveform data"));
  }

  async function applyLiveConfig() {
    const generation = state.generation;
    const specs = decoderSpecs();
    try {
      const payload = await api("/api/live/config",
                                 { method: "POST", body: { specs } });
      if (generation !== state.generation) return;
      state.slotLabels = payload.slot_labels;
      saveSettings();
      toast("live decoder config applied");
      await refreshState();
    } catch (error) {
      toast(`live config: ${error.message}`, "error");
    }
  }

  async function liveStart(slot) {
    try {
      if (state.session === "idle" && state.live[slot]) {
        for (const timeline of state.live) timeline.clear();
      }
      await api(`/control/start?slot=${slot}`, { method: "POST" });
      await refreshState();
    } catch (error) {
      toast(`live: ${error.message}`, "error");
    }
  }

  async function liveStop(slot) {
    try {
      await api(`/control/stop?slot=${slot}`, { method: "POST" });
      await refreshState();
    } catch (error) {
      toast(`live: ${error.message}`, "error");
    }
  }

  async function liveStopAll() {
    try {
      await api("/control/stop", { method: "POST" });
      await refreshState();
    } catch (error) {
      toast(`live: ${error.message}`, "error");
    }
  }

  // -- trace ------------------------------------------------------------------

  async function traceStart() {
    try {
      await api("/api/trace/start", { method: "POST" });
      text("trace-status", "recording");
    } catch (error) {
      toast(`trace: ${error.message}`, "error");
    }
  }

  async function traceStop() {
    try {
      await api("/api/trace/stop", { method: "POST" });
      text("trace-status", "stopped");
    } catch (error) {
      toast(`trace: ${error.message}`, "error");
    }
  }

  async function traceDump() {
    try {
      const payload = await api("/api/trace/dump", { method: "POST" });
      text("trace-status", "dumping…");
      watchJob(payload.job.id, "trace", async () => {
        await loadTraceModel();
      });
    } catch (error) {
      toast(`trace: ${error.message}`, "error");
    }
  }

  async function loadTraceModel() {
    try {
      const payload = await api("/api/trace");
      state.traceTimeline.setModel(payload.model);
      const summary = payload.model.summary || {};
      text("trace-status", `${summary.records || 0} records, ` +
        `${summary.dropped || 0} dropped, ` +
        `${(summary.duration_s || 0) * 1000} ms`);
      text("trace-summary", `${summary.tasks || 0} tasks, ` +
        `${summary.queues || 0} queues`);
      const empty = $("trace-empty");
      if (empty) empty.className = "empty hidden";
    } catch (error) {
      toast(`trace: ${error.message}`, "error");
    }
  }

  async function armPatgen() {
    try {
      const payload = await api("/api/patgen",
                                { method: "POST",
                                  body: { mode: value("patgen-mode") } });
      text("patgen-status", `armed: ${payload.patgen}`);
      state.settings.patgen = payload.patgen;
      saveSettings();
    } catch (error) {
      toast(`patgen: ${error.message}`, "error");
    }
  }

  // -- detail windows ---------------------------------------------------------

  async function fetchSamples(start, count) {
    if (!state.capture || !state.capture.has_signal) return null;
    const captureId = state.capture.id;
    try {
      const response = await fetch(
        `/api/captures/${captureId}/samples?start=${start}&count=${count}`);
      if (!response.ok) return null;
      const buffer = await response.arrayBuffer();
      if (!state.capture || state.capture.id !== captureId) return null;
      return new Uint8Array(buffer);
    } catch (error) {
      return null;
    }
  }

  function requestDetail(start, count) {
    if (!state.capture || !state.capture.has_signal) return;
    const key = `${state.capture.id}:${start}:${count}`;
    if (key === state.detailKey) return;
    state.detailPending = { key, start, count };
    if (state.detailTimer) return;
    state.detailTimer = setTimeout(async () => {
      state.detailTimer = null;
      const pending = state.detailPending;
      state.detailPending = null;
      if (!pending) return;
      state.detailKey = pending.key;
      try {
        const response = await fetch(
          `/api/captures/${state.capture.id}/samples` +
          `?start=${pending.start}&count=${pending.count}`);
        if (!response.ok) return;
        const buffer = await response.arrayBuffer();
        if (state.capture && pending.key.startsWith(state.capture.id)) {
          state.waveform.setDetail(pending.start, new Uint8Array(buffer));
        }
      } catch (error) { /* detail is best-effort */ }
    }, 60);
  }

  // -- init -------------------------------------------------------------------

  function init() {
    if (state.started) return;
    state.started = true;
    state.settings = loadSettings();
    state.channels = loadChannels();
    buildPatgenOptions();
    applySettingsToForm();
    state.waveform = new window.LAWaveform.WaveformView($("wave-canvas"));
    state.waveform.setChannels(state.channels);
    state.waveform.onDetailNeeded = requestDetail;
    state.waveform.onSamplesNeeded = fetchSamples;
    state.waveform.onStatus = (message) => text("view-status", message);
    state.waveform.onMeasure = (message) =>
      text("measure-text", message || "no cursors");
    state.traceTimeline = new window.LATimeline.TraceTimeline($("trace-canvas"));
    buildChannelList();
    buildLivePanes();
    bindUi();
    text("page-version", `page ${PAGE_VERSION}`);
    connectEvents();
    refreshPorts();
    refreshState();
    state.stateTimer = setInterval(refreshState, 1500);
  }

  window.LAApp = { init, state, PAGE_VERSION, updateStats };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
