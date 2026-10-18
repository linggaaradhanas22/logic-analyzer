/* Unified UI timeline renderers (W03 live, W06 trace).
 *
 * LiveTimeline retains the legacy event view. LiveWaveform draws actual FPGA
 * runs with aligned decoding and bounded/frozen history. TraceTimeline draws the P8.3 trace
 * model (task lanes, busy spans, queue/stream/mark/ISR markers) on a
 * millisecond axis. Both are plain canvases owned by the app.
 */
(function () {
  "use strict";

  const KIND_COLORS = {
    1: "#4fc3f7", 2: "#f85149", 3: "#d29922", 4: "#81c784", 5: "#81c784",
    6: "#ba68c8", 7: "#ffb74d", 8: "#3fb950", 9: "#f85149", 10: "#8b949e",
    11: "#8b949e",
  };
  const KIND_NAMES = {
    1: "BYTE", 2: "FRAME_ERR", 3: "PARITY_ERR", 4: "START", 5: "STOP",
    6: "DISC", 7: "ADDR", 8: "ACK", 9: "NACK", 10: "CS_A", 11: "CS_R",
  };
  const LABEL = 54;

  function niceStep(raw) {
    if (!(raw > 0)) return 1;
    const power = Math.pow(10, Math.floor(Math.log10(raw)));
    const scaled = raw / power;
    if (scaled <= 1) return power;
    if (scaled <= 2) return 2 * power;
    if (scaled <= 5) return 5 * power;
    return 10 * power;
  }

  function formatSeconds(value) {
    const abs = Math.abs(value);
    if (abs >= 1) return value.toFixed(4) + " s";
    if (abs >= 1e-3) return (value * 1e3).toFixed(3) + " ms";
    if (abs >= 1e-6) return (value * 1e6).toFixed(2) + " µs";
    return (value * 1e9).toFixed(0) + " ns";
  }

  class LiveTimeline {
    constructor(slot, options) {
      const settings = options || {};
      this.slot = Number(slot);
      this.events = [];
      this.maxEvents = settings.maxEvents || 100000;
      this.dropped = 0;
      this.sampleRate = settings.sampleRate || 100000000;
      this.view = { start: 0, span: 20000 };
      this.follow = true;
      this.drag = null;
      this.canvas = null;
      this.width = 600;
      this.height = 96;
      this._bound = false;
    }

    attach(canvas) {
      this.canvas = canvas;
      if (canvas && !this._bound) {
        this._bound = true;
        canvas.addEventListener("wheel", (event) => {
          event.preventDefault();
          this.follow = false;
          const rect = canvas.getBoundingClientRect();
          const anchor = this.xToTime(event.clientX - rect.left);
          const factor = event.deltaY > 0 ? 1.3 : 0.77;
          const span = Math.max(20, this.view.span * factor);
          const ratio = (anchor - this.view.start) / this.view.span;
          this.view.span = span;
          this.view.start = Math.max(0, anchor - ratio * span);
          this.render();
        }, { passive: false });
        canvas.addEventListener("mousedown", (event) => {
          this.follow = false;
          this.drag = { x: event.clientX, start: this.view.start };
        });
        canvas.addEventListener("mousemove", (event) => {
          if (!this.drag) return;
          const plotW = Math.max(10, this.width - LABEL - 8);
          const dx = event.clientX - this.drag.x;
          this.view.start = Math.max(0, this.drag.start - dx / plotW * this.view.span);
          this.drag.x = event.clientX;
          this.render();
        });
        canvas.addEventListener("mouseup", () => { this.drag = null; });
        canvas.addEventListener("mouseleave", () => { this.drag = null; });
        canvas.addEventListener("dblclick", () => {
          this.follow = true;
          this.render();
        });
      }
      this.resize();
    }

    clear() {
      this.events = [];
      this.dropped = 0;
      this.follow = true;
      this.view = { start: 0, span: 20000 };
      this.render();
    }

    add(records) {
      for (const record of records) {
        this.events.push({
          t: Number(record[0]),
          kind: Number(record[1]),
          data: record[2] || "",
        });
      }
      if (this.events.length > this.maxEvents) {
        const excess = this.events.length - this.maxEvents;
        this.events.splice(0, excess);
        this.dropped += excess;
      }
      if (this.follow) {
        const last = this.events[this.events.length - 1];
        if (last) {
          this.view.start = Math.max(0, last.t - this.view.span);
        }
      }
      this.render();
    }

    resize() {
      if (!this.canvas) return;
      const ratio = window.devicePixelRatio || 1;
      const width = this.canvas.clientWidth || 600;
      const height = this.canvas.clientHeight || 96;
      this.canvas.width = Math.round(width * ratio);
      this.canvas.height = Math.round(height * ratio);
      this.canvas.getContext("2d").setTransform(ratio, 0, 0, ratio, 0, 0);
      this.width = width;
      this.height = height;
      this.render();
    }

    xToTime(x) {
      const plotW = Math.max(10, this.width - LABEL - 8);
      return this.view.start + ((x - LABEL) / plotW) * this.view.span;
    }

    timeToX(time) {
      const plotW = Math.max(10, this.width - LABEL - 8);
      return LABEL + ((time - this.view.start) / this.view.span) * plotW;
    }

    render() {
      if (!this.canvas) return;
      const ctx = this.canvas.getContext("2d");
      ctx.clearRect(0, 0, this.width, this.height);
      ctx.fillStyle = "#0f141b";
      ctx.fillRect(0, 0, this.width, this.height);
      const plotW = Math.max(10, this.width - LABEL - 8);
      const axisY = this.height - 16;
      // axis
      const step = niceStep(this.view.span / 6);
      const first = Math.ceil(this.view.start / step) * step;
      ctx.strokeStyle = "#2a3441";
      ctx.beginPath();
      ctx.moveTo(LABEL, axisY + 0.5);
      ctx.lineTo(LABEL + plotW, axisY + 0.5);
      ctx.stroke();
      ctx.fillStyle = "#7d8b9c";
      ctx.font = "9px Consolas, monospace";
      for (let time = first; time <= this.view.start + this.view.span;
           time += step) {
        const x = this.timeToX(time);
        if (x < LABEL || x > LABEL + plotW) continue;
        ctx.fillText(formatSeconds(time / this.sampleRate), x + 2, axisY + 11);
      }
      // event ticks
      const end = this.view.start + this.view.span;
      let drawn = 0;
      for (const event of this.events) {
        if (event.t < this.view.start) continue;
        if (event.t > end) break;
        const x = this.timeToX(event.t);
        const color = KIND_COLORS[event.kind] || "#8b949e";
        ctx.strokeStyle = color;
        ctx.beginPath();
        ctx.moveTo(x, 4);
        ctx.lineTo(x, axisY - 2);
        ctx.stroke();
        drawn++;
      }
      ctx.fillStyle = "#7d8b9c";
      ctx.font = "10px Consolas, monospace";
      const last = this.events[this.events.length - 1];
      const label = last
        ? `${KIND_NAMES[last.kind] || last.kind} ${last.data || ""} t=${last.t}`
        : "waiting for events";
      ctx.fillText(label, LABEL, 12);
      ctx.fillStyle = "#5b6875";
      ctx.fillText(`${this.events.length} kept / ${this.dropped} dropped`,
                   LABEL, this.height - 4);
      if (this.follow) {
        ctx.fillStyle = "#4fc3f7";
        ctx.fillText("follow", LABEL + plotW - 34, 12);
      }
    }
  }

  class LiveWaveform extends window.LAWaveform.WaveformView {
    constructor(canvas, slot) {
      super(canvas);
      this.slot = slot;
      this.runs = [];
      this.events = [];
      this.maxEvents = 10000;
      this.dropped = 0;
      this.gaps = 0;
      this.follow = true;
      this.sampleRate = 100000000;
      this.latestActivity = null;
      this.spec = "";
      this.mask = 1;
      this.onInfo = null;
      this.frozen = null;
      canvas.addEventListener("mousedown", () => this.pause());
      canvas.addEventListener("dblclick", () => this.resume());
    }

    configure(spec) {
      if (this.spec === spec) return;
      this.spec = spec || "";
      const used = [...this.spec.matchAll(/(?:rx|sck|mosi|miso|cs|scl|sda)=([0-7])\b/g)]
        .map(match => Number(match[1]));
      this.mask = used.reduce((mask, channel) => mask | (1 << channel), 0) || 1;
      this.channels = window.LAWaveform.defaultChannels().map(channel =>
        Object.assign(channel, { visible: !!(this.mask & (1 << channel.index)) }));
      this.resize();
    }

    clear() {
      this.runs = [];
      this.events = [];
      this.annotations = {};
      this.annotationSlots = [];
      this.dropped = this.gaps = 0;
      this.latestActivity = null;
      this.follow = true;
      this.frozen = null;
      this.meta = null;
      this.cursors = { a: null, b: null };
      this.resize();
    }

    add(records) {
      for (const record of records) this.events.push({ t_sample: Number(record[0]),
        kind: Number(record[1]), data: record[2] || "",
        name: KIND_NAMES[record[1]] || String(record[1]) });
      if (this.events.length > this.maxEvents) {
        const excess = this.events.length - this.maxEvents;
        this.events.splice(0, excess);
        this.dropped += excess;
      }
      this.annotations[this.slot] = this.events;
      if (!this.annotationSlots.length) {
        this.annotationSlots = [this.slot];
        this.resize();
      }
      this.render();
    }

    addWaveform(packet) {
      let time = Number(packet.start);
      const previous = this.runs[this.runs.length - 1];
      if (previous && time < previous.end) return;
      if (previous && time > previous.end) this.gaps++;
      let last = previous;
      for (const word of packet.words) {
        const sample = word & 255;
        const end = time + Math.floor(word / 256);
        if (last && last.end === time && last.sample === sample) {
          last.end = end;
        } else {
          if (last && last.end === time && ((last.sample ^ sample) & this.mask)) {
            this.latestActivity = time;
          }
          last = { start: time, end, sample };
          this.runs.push(last);
        }
        time = end;
      }
      this.sampleRate = Number(packet.rate);
      if (!this.meta) {
        this.meta = { total_samples: time, sample_rate_hz: this.sampleRate, has_signal: true };
        const baud = /baud=(\d+)/.exec(this.spec);
        this.view.span = this.sampleRate * (baud ? 80 / Number(baud[1]) : 0.002);
      }
      this.meta.total_samples = time;
      this.meta.sample_rate_hz = this.sampleRate;
      // Retain at most 16k exact runs and two seconds, also while paused.
      const oldest = Math.max(0, time - this.sampleRate * 2);
      let trim = Math.max(0, this.runs.length - 16384);
      while (trim < this.runs.length - 1 && this.runs[trim].end < oldest) trim++;
      if (trim) this.runs.splice(0, trim);
      if (this.runs[0].start < oldest) this.runs[0] = Object.assign({}, this.runs[0], {start: oldest});
      const first = this.runs[0].start;
      let eventTrim = 0;
      while (eventTrim < this.events.length && this.events[eventTrim].t_sample < first) eventTrim++;
      if (eventTrim) { this.events.splice(0, eventTrim); this.dropped += eventTrim; }
      if (this.follow) {
        const anchor = this.latestActivity === null || this.latestActivity < first
          ? time : this.latestActivity;
        this.view.start = Math.max(first, anchor - this.view.span * 0.85);
      } else if (!this.frozen && this.view.start < first) {
        this.view.start = first;
      }
      this.omitted = packet.omitted;
      this.render();
    }

    _detailCoversView() { return !!(this.runs && this.runs.length); }
    _maybeRequestDetail() {}

    _drawDetailChannel(index, high, low) {
      const ctx = this.ctx;
      ctx.beginPath();
      let previous = null;
      for (const run of this.frozen ? this.frozen.runs : this.runs) {
        if (run.end < this.view.start) continue;
        if (run.start > this.view.start + this.view.span) break;
        const left = Math.max(run.start, this.view.start);
        const right = Math.min(run.end, this.view.start + this.view.span);
        const level = run.sample & (1 << index) ? high : low;
        const x = this.timeToX(left);
        if (previous && previous.end === run.start) ctx.lineTo(x, level);
        else ctx.moveTo(x, level); // Unknown ranges stay blank, never bridged.
        ctx.lineTo(this.timeToX(right), level);
        previous = run;
      }
      ctx.stroke();
    }

    _drawAnnotationRow(row) {
      const current = this.annotations[row.slot];
      if (this.frozen) this.annotations[row.slot] = this.frozen.events;
      super._drawAnnotationRow(row);
      this.annotations[row.slot] = current;
    }
    pause() {
      if (!this.frozen) this.frozen = {
        runs: this.runs.map(run => Object.assign({}, run)), events: this.events.slice() };
      this.follow = false;
      this.render();
    }
    zoom(factor, anchor) { this.pause(); super.zoom(factor, anchor); }
    fit() {
      if (!this.runs || !this.runs.length) return;
      this.pause();
      const runs = this.frozen.runs;
      if (!runs.length) return;
      this.view = { start: runs[0].start,
        span: Math.max(2, runs[runs.length - 1].end - runs[0].start) };
      this.render();
    }
    resume() { this.frozen = null; this.follow = true; this.render(); }
    render() {
      if (this._pendingDraw) return;
      this._pendingDraw = true;
      window.requestAnimationFrame(() => {
        this._pendingDraw = false;
        super.render();
        if (this.meta && this.onInfo) this.onInfo(
          `${this.follow ? "Following activity" : "Paused"} · real signal · ` +
          `${this.runs.length} runs · ${this.gaps} gaps · ${this.omitted || 0} omitted preview ranges`);
      });
    }
  }

  class TraceTimeline {
    constructor(canvas) {
      this.canvas = canvas;
      this.model = null;
      this.view = { start: 0, span: 1 };
      this.drag = null;
      this.width = 600;
      this.height = 180;
      this._bind();
    }

    _bind() {
      const canvas = this.canvas;
      if (!canvas) return;
      canvas.addEventListener("wheel", (event) => {
        if (!this.model) return;
        event.preventDefault();
        const rect = canvas.getBoundingClientRect();
        const anchor = this.xToTime(event.clientX - rect.left);
        const factor = event.deltaY > 0 ? 1.3 : 0.77;
        const span = Math.max(0.01, this.view.span * factor);
        const ratio = (anchor - this.view.start) / this.view.span;
        this.view.span = span;
        this.view.start = Math.max(0, anchor - ratio * span);
        this.render();
      }, { passive: false });
      canvas.addEventListener("mousedown", (event) => {
        this.drag = { x: event.clientX, start: this.view.start };
      });
      canvas.addEventListener("mousemove", (event) => {
        if (!this.drag) return;
        const plotW = Math.max(10, this.width - 110);
        const dx = event.clientX - this.drag.x;
        this.view.start = Math.max(0,
          this.drag.start - dx / plotW * this.view.span);
        this.drag.x = event.clientX;
        this.render();
      });
      canvas.addEventListener("mouseup", () => { this.drag = null; });
      canvas.addEventListener("mouseleave", () => { this.drag = null; });
      canvas.addEventListener("dblclick", () => this.fit());
    }

    setModel(model) {
      this.model = model;
      this.fit();
    }

    fit() {
      const duration = this.model && this.model.duration_ms
        ? this.model.duration_ms : 1;
      this.view = { start: 0, span: Math.max(0.01, duration) };
      this.render();
    }

    resize() {
      if (!this.canvas) return;
      const ratio = window.devicePixelRatio || 1;
      const width = this.canvas.clientWidth || 600;
      const height = this.canvas.clientHeight || 180;
      this.canvas.width = Math.round(width * ratio);
      this.canvas.height = Math.round(height * ratio);
      this.canvas.getContext("2d").setTransform(ratio, 0, 0, ratio, 0, 0);
      this.width = width;
      this.height = height;
      this.render();
    }

    timeToX(timeMs) {
      const plotW = Math.max(10, this.width - 110);
      return 100 + ((timeMs - this.view.start) / this.view.span) * plotW;
    }

    xToTime(x) {
      const plotW = Math.max(10, this.width - 110);
      return this.view.start + ((x - 100) / plotW) * this.view.span;
    }

    render() {
      if (!this.canvas) return;
      const ctx = this.canvas.getContext("2d");
      ctx.clearRect(0, 0, this.width, this.height);
      ctx.fillStyle = "#0f141b";
      ctx.fillRect(0, 0, this.width, this.height);
      if (!this.model) return;
      const lanes = this.model.lanes || [];
      const laneCount = Math.max(1, lanes.length);
      const laneHeight = Math.min(26, (this.height - 26) / laneCount);
      const plotW = Math.max(10, this.width - 110);
      // lanes
      lanes.forEach((lane, index) => {
        const y = 6 + index * laneHeight;
        ctx.fillStyle = index % 2 ? "#121821" : "#141b25";
        ctx.fillRect(0, y, this.width, laneHeight);
        ctx.fillStyle = "#d7e0ea";
        ctx.font = "10px Consolas, monospace";
        ctx.textAlign = "right";
        ctx.fillText(lane.name, 94, y + laneHeight / 2 + 3);
        ctx.textAlign = "left";
        const spans = (this.model.spans || {})[String(lane.id)] || [];
        for (const span of spans) {
          const x0 = this.timeToX(span[0]);
          const x1 = this.timeToX(span[1]);
          if (x1 < 100 || x0 > 100 + plotW) continue;
          ctx.fillStyle = "rgba(79,195,247,0.45)";
          ctx.fillRect(Math.max(100, x0), y + 3,
                       Math.max(1, Math.min(100 + plotW, x1) - Math.max(100, x0)),
                       laneHeight - 6);
        }
      });
      // markers
      for (const marker of this.model.markers || []) {
        const x = this.timeToX(marker.t_ms);
        if (x < 100 || x > 100 + plotW) continue;
        if (marker.lane === null || marker.lane === undefined) {
          ctx.strokeStyle = "#f85149";
          ctx.setLineDash([3, 3]);
          ctx.beginPath();
          ctx.moveTo(x, 4);
          ctx.lineTo(x, this.height - 20);
          ctx.stroke();
          ctx.setLineDash([]);
          continue;
        }
        const y = 6 + marker.lane * laneHeight + laneHeight / 2;
        ctx.fillStyle = marker.group === "stream" ? "#2f81f7"
          : marker.group === "queue" ? "#d29922" : "#3fb950";
        ctx.fillRect(x - 1, y - 2, 3, 4);
      }
      // axis
      const axisY = this.height - 16;
      const step = niceStep(this.view.span / 6);
      const first = Math.ceil(this.view.start / step) * step;
      ctx.strokeStyle = "#2a3441";
      ctx.beginPath();
      ctx.moveTo(100, axisY + 0.5);
      ctx.lineTo(100 + plotW, axisY + 0.5);
      ctx.stroke();
      ctx.fillStyle = "#7d8b9c";
      ctx.font = "9px Consolas, monospace";
      for (let time = first; time <= this.view.start + this.view.span;
           time += step) {
        const x = this.timeToX(time);
        if (x < 100 || x > 100 + plotW) continue;
        ctx.fillText(time.toFixed(2) + " ms", x + 2, axisY + 11);
      }
      ctx.fillStyle = "#7d8b9c";
      ctx.fillText(`${(this.model.summary || {}).records || 0} records, ` +
                   `${(this.model.summary || {}).dropped || 0} dropped`,
                   100, 12);
    }
  }

  window.LATimeline = { LiveTimeline, LiveWaveform, TraceTimeline, KIND_NAMES, KIND_COLORS };
})();
