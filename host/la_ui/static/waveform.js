/* Unified UI waveform renderer (W03/W04).
 *
 * Canvas 2D, one row per channel plus one row per decoded slot. The view
 * draws the server-side min/max envelope at wide zoom and exact sample steps
 * when the view span is small enough for a detail window (the app fetches it
 * through onDetailNeeded). Zoom, pan, cursors A/B, trigger and pre/post
 * markers, channel layout, edge/pattern search and PNG screenshots live here;
 * the app owns the API calls and persistence.
 */
(function () {
  "use strict";

  const RULER = 26;
  const LABEL = 86;
  const ANN_ROW = 22;
  const DETAIL_SPAN = 8192;
  const ANN_DRAW_CAP = 1500;

  const ANN_COLORS = {
    1: "#4fc3f7", 2: "#f85149", 3: "#d29922", 4: "#81c784", 5: "#81c784",
    6: "#ba68c8", 7: "#ffb74d", 8: "#3fb950", 9: "#f85149", 10: "#8b949e",
    11: "#8b949e",
  };

  const DEFAULT_COLORS = ["#4fc3f7", "#ffb74d", "#81c784", "#e57373",
                          "#ba68c8", "#4dd0e1", "#f06292", "#aed581"];

  function defaultChannels() {
    const channels = [];
    for (let index = 0; index < 8; index++) {
      channels.push({
        index,
        name: "CH" + index,
        color: DEFAULT_COLORS[index],
        visible: true,
        height: 34,
      });
    }
    return channels;
  }

  function parseEnvelope(buffer) {
    const view = new DataView(buffer);
    const magic = String.fromCharCode(view.getUint8(0), view.getUint8(1),
                                      view.getUint8(2), view.getUint8(3));
    if (magic !== "LAEN") throw new Error("bad envelope magic " + magic);
    const version = view.getUint8(4);
    const channels = view.getUint8(5);
    const buckets = view.getUint16(6, true);
    const total = view.getUint32(8, true);
    const envelope = { version, channels, buckets, total, min: [], max: [] };
    let offset = 12;
    for (let channel = 0; channel < channels; channel++) {
      envelope.min.push(new Uint8Array(buffer, offset, buckets));
      offset += buckets;
      envelope.max.push(new Uint8Array(buffer, offset, buckets));
      offset += buckets;
    }
    return envelope;
  }

  function niceStep(raw) {
    if (!(raw > 0)) return 1;
    const power = Math.pow(10, Math.floor(Math.log10(raw)));
    const scaled = raw / power;
    if (scaled <= 1) return power;
    if (scaled <= 2) return 2 * power;
    if (scaled <= 5) return 5 * power;
    return 10 * power;
  }

  function formatTime(seconds) {
    const abs = Math.abs(seconds);
    if (abs >= 1) return seconds.toFixed(6) + " s";
    if (abs >= 1e-3) return (seconds * 1e3).toFixed(3) + " ms";
    if (abs >= 1e-6) return (seconds * 1e6).toFixed(3) + " µs";
    return (seconds * 1e9).toFixed(1) + " ns";
  }

  class WaveformView {
    constructor(canvas) {
      this.canvas = canvas;
      this.ctx = canvas.getContext("2d");
      this.meta = null;
      this.envelope = null;
      this.annotations = {};
      this.annotationSlots = [];
      this.channels = defaultChannels();
      this.selectedChannel = 0;
      this.view = { start: 0, span: 1 };
      this.cursors = { a: null, b: null };
      this.detail = null;
      this.drag = null;
      this.measure = "";
      this.onDetailNeeded = null;
      this.onStatus = null;
      this.onMeasure = null;
      this.width = 800;
      this.height = 200;
      this.plotX = LABEL;
      this.plotW = 600;
      this._bind();
      this.resize();
    }

    // -- data ---------------------------------------------------------------

    setCapture(meta, envelopeBuffer) {
      this.meta = meta || null;
      this.envelope = envelopeBuffer ? parseEnvelope(envelopeBuffer) : null;
      this.annotations = {};
      this.annotationSlots = [];
      this.cursors = { a: null, b: null };
      this.detail = null;
      this.fit();
    }

    setChannels(channels) {
      if (Array.isArray(channels) && channels.length === 8) {
        this.channels = channels;
        this.resize();
      }
    }

    setAnnotations(annotations) {
      this.annotations = {};
      this.annotationSlots = [];
      for (const [slot, events] of Object.entries(annotations || {})) {
        const sorted = events.slice().sort((left, right) =>
          left.t_sample - right.t_sample);
        this.annotations[slot] = sorted;
        this.annotationSlots.push(Number(slot));
      }
      this.annotationSlots.sort((left, right) => left - right);
      this.resize();
    }

    setDetail(start, samples) {
      this.detail = { start: Number(start), samples };
      this.render();
    }

    // -- view math ----------------------------------------------------------

    timeToX(time) {
      return this.plotX + (time - this.view.start) / this.view.span * this.plotW;
    }

    xToTime(x) {
      return this.view.start + (x - this.plotX) / this.plotW * this.view.span;
    }

    fit() {
      const total = this.meta ? Math.max(1, this.meta.total_samples) : 1;
      this.view = { start: 0, span: total };
      this.detail = null;
      this.render();
    }

    zoom(factor, anchorTime) {
      if (!this.meta) return;
      const span = Math.max(2, Math.min(this.meta.total_samples,
                                        this.view.span * factor));
      const anchor = anchorTime === undefined
        ? this.view.start + this.view.span / 2 : anchorTime;
      const ratio = (anchor - this.view.start) / this.view.span;
      let start = anchor - ratio * span;
      start = Math.max(0, Math.min(this.meta.total_samples - span, start));
      this.view = { start, span };
      this._maybeRequestDetail();
      this.render();
    }

    zoomToCursors() {
      const { a, b } = this.cursors;
      if (a === null || b === null || a === b) return;
      const start = Math.max(0, Math.min(a, b));
      const span = Math.max(2, Math.abs(a - b));
      this.view = { start, span };
      this._maybeRequestDetail();
      this.render();
    }

    panPixels(dx) {
      if (!this.meta) return;
      const span = this.view.span;
      let start = this.view.start - dx / this.plotW * span;
      start = Math.max(0, Math.min(this.meta.total_samples - span, start));
      this.view = { start, span };
      this._maybeRequestDetail();
      this.render();
    }

    setCursor(which) {
      if (!this.meta) return;
      if (this.cursors[which] !== null) {
        this.cursors[which] = null;
      } else {
        this.cursors[which] = this.view.start + this.view.span / 2;
      }
      this._updateMeasure();
      this.render();
    }

    _maybeRequestDetail() {
      if (!this.meta || !this.onDetailNeeded) return;
      if (this.view.span > DETAIL_SPAN) {
        this.detail = null;
        return;
      }
      const start = Math.max(0, Math.floor(this.view.start));
      const count = Math.min(DETAIL_SPAN, Math.ceil(this.view.span) + 2);
      this.onDetailNeeded(start, count);
    }

    _detailCoversView() {
      if (!this.detail || !this.meta) return false;
      return this.detail.start <= this.view.start &&
        this.detail.start + this.detail.samples.length >=
          this.view.start + this.view.span;
    }

    // -- rendering ----------------------------------------------------------

    layout() {
      const rows = [];
      let y = RULER;
      for (const channel of this.channels) {
        if (!channel.visible) continue;
        rows.push({ kind: "channel", channel, y, h: channel.height });
        y += channel.height;
      }
      for (const slot of this.annotationSlots) {
        rows.push({ kind: "annotations", slot, y, h: ANN_ROW });
        y += ANN_ROW;
      }
      return { rows, height: y + 6 };
    }

    resize() {
      const ratio = window.devicePixelRatio || 1;
      const width = this.canvas.clientWidth || 800;
      const layout = this.layout();
      const height = Math.max(140, layout.height);
      this.canvas.width = Math.round(width * ratio);
      this.canvas.height = Math.round(height * ratio);
      this.canvas.style.height = height + "px";
      this.ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      this.width = width;
      this.height = height;
      this.render();
    }

    render() {
      const ctx = this.ctx;
      ctx.clearRect(0, 0, this.width, this.height);
      ctx.fillStyle = "#0f141b";
      ctx.fillRect(0, 0, this.width, this.height);
      this.plotX = LABEL;
      this.plotW = Math.max(10, this.width - LABEL - 10);
      if (!this.meta) return;
      this._drawRuler();
      const layout = this.layout();
      for (const row of layout.rows) {
        if (row.kind === "channel") this._drawChannel(row);
        else this._drawAnnotationRow(row);
      }
      this._drawTrigger();
      this._drawCursors();
    }

    _drawRuler() {
      const ctx = this.ctx;
      const rate = this.meta.sample_rate_hz || 1;
      ctx.fillStyle = "#161b22";
      ctx.fillRect(0, 0, this.width, RULER);
      ctx.strokeStyle = "#2a3441";
      ctx.beginPath();
      ctx.moveTo(0, RULER - 0.5);
      ctx.lineTo(this.width, RULER - 0.5);
      ctx.stroke();
      const step = niceStep(this.view.span / 8);
      const first = Math.ceil(this.view.start / step) * step;
      ctx.fillStyle = "#7d8b9c";
      ctx.font = "10px Consolas, monospace";
      ctx.textAlign = "left";
      for (let time = first; time <= this.view.start + this.view.span;
           time += step) {
        const x = this.timeToX(time);
        if (x < this.plotX - 1 || x > this.plotX + this.plotW + 1) continue;
        ctx.strokeStyle = "#232c38";
        ctx.beginPath();
        ctx.moveTo(x, RULER - 6);
        ctx.lineTo(x, RULER);
        ctx.stroke();
        ctx.fillText(formatTime(time / rate), x + 2, 11);
      }
    }

    _drawChannel(row) {
      const ctx = this.ctx;
      const { channel, y, h } = row;
      const top = y + 2;
      const bottom = y + h - 2;
      const levelHigh = top + 4;
      const levelLow = bottom - 4;
      ctx.fillStyle = row.channel.index % 2 ? "#121821" : "#141b25";
      ctx.fillRect(0, y, this.width, h);
      ctx.strokeStyle = "#1f2833";
      ctx.beginPath();
      ctx.moveTo(0, y + h - 0.5);
      ctx.lineTo(this.width, y + h - 0.5);
      ctx.stroke();
      // label
      ctx.fillStyle = channel.color;
      ctx.font = "11px Consolas, monospace";
      ctx.textAlign = "right";
      ctx.fillText(channel.name, LABEL - 8, y + h / 2 + 4);
      ctx.textAlign = "left";
      // signal
      ctx.save();
      ctx.beginPath();
      ctx.rect(this.plotX, y, this.plotW, h);
      ctx.clip();
      ctx.strokeStyle = channel.color;
      ctx.lineWidth = 1.3;
      if (this._detailCoversView()) {
        this._drawDetailChannel(channel.index, levelHigh, levelLow);
      } else if (this.envelope) {
        this._drawEnvelopeChannel(channel.index, levelHigh, levelLow);
      }
      ctx.restore();
      if (channel.index === this.selectedChannel) {
        ctx.strokeStyle = "rgba(79,195,247,0.5)";
        ctx.strokeRect(0.5, y + 0.5, this.width - 1, h - 1);
      }
    }

    _drawEnvelopeChannel(index, levelHigh, levelLow) {
      const ctx = this.ctx;
      const envelope = this.envelope;
      if (!envelope || !envelope.buckets || !envelope.total) return;
      const min = envelope.min[index];
      const max = envelope.max[index];
      ctx.beginPath();
      let lastBucket = -1;
      const steps = Math.max(1, Math.floor(this.plotW));
      for (let pixel = 0; pixel <= steps; pixel++) {
        const time = this.view.start + (pixel / steps) * this.view.span;
        if (time < 0 || time >= envelope.total) continue;
        const bucket = Math.min(envelope.buckets - 1,
          Math.floor((time / envelope.total) * envelope.buckets));
        if (bucket === lastBucket) continue;
        lastBucket = bucket;
        const x = this.plotX + (pixel / steps) * this.plotW;
        const high = max[bucket] ? levelHigh : levelLow;
        const low = min[bucket] ? levelHigh : levelLow;
        ctx.moveTo(x, Math.min(high, low));
        ctx.lineTo(x, Math.max(high, low) + 0.01);
      }
      ctx.stroke();
    }

    _drawDetailChannel(index, levelHigh, levelLow) {
      const ctx = this.ctx;
      const samples = this.detail.samples;
      const mask = 1 << index;
      ctx.beginPath();
      let started = false;
      for (let offset = 0; offset < samples.length; offset++) {
        const time = this.detail.start + offset;
        if (time < this.view.start - 1 ||
            time > this.view.start + this.view.span + 1) continue;
        const x = this.timeToX(time);
        const level = (samples[offset] & mask) ? levelHigh : levelLow;
        if (!started) {
          ctx.moveTo(x, level);
          started = true;
        } else {
          ctx.lineTo(x, level);
        }
      }
      ctx.stroke();
    }

    _drawAnnotationRow(row) {
      const ctx = this.ctx;
      const events = this.annotations[row.slot] || [];
      const y = row.y;
      ctx.fillStyle = "#10161e";
      ctx.fillRect(0, y, this.width, row.h);
      ctx.fillStyle = "#7d8b9c";
      ctx.font = "10px Consolas, monospace";
      ctx.textAlign = "right";
      ctx.fillText("slot " + row.slot, LABEL - 8, y + row.h / 2 + 3);
      ctx.textAlign = "left";
      ctx.save();
      ctx.beginPath();
      ctx.rect(this.plotX, y, this.plotW, row.h);
      ctx.clip();
      const end = this.view.start + this.view.span;
      let drawn = 0;
      for (const event of events) {
        if (event.t_sample < this.view.start) continue;
        if (event.t_sample > end) break;
        if (drawn >= ANN_DRAW_CAP) break;
        const x = this.timeToX(event.t_sample);
        const color = ANN_COLORS[event.kind] || "#8b949e";
        ctx.strokeStyle = color;
        ctx.beginPath();
        ctx.moveTo(x, y + 1);
        ctx.lineTo(x, y + row.h - 1);
        ctx.stroke();
        const label = event.name + (event.data ? " " + event.data : "");
        ctx.fillStyle = color;
        ctx.font = "9px Consolas, monospace";
        ctx.fillText(label, x + 2, y + 10);
        drawn++;
      }
      ctx.restore();
    }

    _drawTrigger() {
      if (!this.meta) return;
      const ctx = this.ctx;
      const trigger = this.meta.trigger_index;
      if (trigger !== null && trigger !== undefined && trigger > 0) {
        const x = this.timeToX(trigger);
        if (x >= this.plotX && x <= this.plotX + this.plotW) {
          ctx.strokeStyle = "#f85149";
          ctx.setLineDash([4, 3]);
          ctx.beginPath();
          ctx.moveTo(x, RULER);
          ctx.lineTo(x, this.height);
          ctx.stroke();
          ctx.setLineDash([]);
          ctx.fillStyle = "#f85149";
          ctx.font = "10px Consolas, monospace";
          ctx.fillText("T", x + 2, RULER + 10);
        }
      }
      const pre = this.meta.pre_samples || 0;
      if (pre > 0 && trigger !== null && trigger !== undefined) {
        const x = this.timeToX(Math.max(0, trigger - pre));
        if (x >= this.plotX && x <= this.plotX + this.plotW) {
          ctx.strokeStyle = "#d29922";
          ctx.setLineDash([2, 3]);
          ctx.beginPath();
          ctx.moveTo(x, RULER);
          ctx.lineTo(x, this.height);
          ctx.stroke();
          ctx.setLineDash([]);
          ctx.fillStyle = "#d29922";
          ctx.fillText("pre", x + 2, RULER + 10);
        }
      }
    }

    _drawCursors() {
      const ctx = this.ctx;
      const rate = this.meta && this.meta.sample_rate_hz || 1;
      for (const [which, color] of [["a", "#4fc3f7"], ["b", "#ffb74d"]]) {
        const time = this.cursors[which];
        if (time === null || time === undefined) continue;
        const x = this.timeToX(time);
        if (x < this.plotX - 1 || x > this.plotX + this.plotW + 1) continue;
        ctx.strokeStyle = color;
        ctx.beginPath();
        ctx.moveTo(x, RULER);
        ctx.lineTo(x, this.height);
        ctx.stroke();
        ctx.fillStyle = color;
        ctx.font = "10px Consolas, monospace";
        ctx.fillText(which.toUpperCase() + " " +
                     formatTime(time / rate), x + 2, this.height - 4);
      }
    }

    // -- interactions -------------------------------------------------------

    _bind() {
      const canvas = this.canvas;
      canvas.addEventListener("wheel", (event) => {
        if (!this.meta) return;
        event.preventDefault();
        const rect = canvas.getBoundingClientRect();
        const anchor = this.xToTime(event.clientX - rect.left);
        this.zoom(event.deltaY > 0 ? 1.25 : 0.8, anchor);
      }, { passive: false });
      canvas.addEventListener("mousedown", (event) => {
        if (!this.meta) return;
        const rect = canvas.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const y = event.clientY - rect.top;
        if (y < RULER) {
          if (event.shiftKey) this.cursors.b = this.xToTime(x);
          else this.cursors.a = this.xToTime(x);
          this._updateMeasure();
          this.render();
          return;
        }
        const cursor = this._cursorAt(x);
        if (cursor) {
          this.drag = { kind: "cursor", which: cursor };
        } else {
          this.drag = { kind: "pan", x, start: this.view.start };
        }
      });
      canvas.addEventListener("mousemove", (event) => {
        const rect = canvas.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const y = event.clientY - rect.top;
        if (this.drag && this.drag.kind === "cursor") {
          this.cursors[this.drag.which] = Math.max(0, this.xToTime(x));
          this._updateMeasure();
          this.render();
          return;
        }
        if (this.drag && this.drag.kind === "pan") {
          const dx = x - this.drag.x;
          this.panPixels(dx);
          this.drag.x = x;
          return;
        }
        this._hover(x, y);
      });
      canvas.addEventListener("mouseup", () => { this.drag = null; });
      canvas.addEventListener("mouseleave", () => {
        this.drag = null;
        if (this.onStatus) this.onStatus("");
      });
      canvas.addEventListener("dblclick", () => this.fit());
    }

    _cursorAt(x) {
      for (const which of ["a", "b"]) {
        const time = this.cursors[which];
        if (time === null || time === undefined) continue;
        if (Math.abs(this.timeToX(time) - x) <= 4) return which;
      }
      return null;
    }

    _hover(x, y) {
      if (!this.onStatus) return;
      const layout = this.layout();
      for (const row of layout.rows) {
        if (row.kind !== "annotations") continue;
        if (y < row.y || y > row.y + row.h) continue;
        const events = this.annotations[row.slot] || [];
        let best = null;
        let bestDistance = 8;
        for (const event of events) {
          const distance = Math.abs(this.timeToX(event.t_sample) - x);
          if (distance < bestDistance) {
            bestDistance = distance;
            best = event;
          }
        }
        if (best) {
          this.onStatus(`slot ${row.slot} ${best.name} t=${best.t_sample} ` +
                        `0x${best.data || ""}`);
        } else {
          this.onStatus("");
        }
        return;
      }
      this.onStatus("");
    }

    _updateMeasure() {
      const rate = this.meta && this.meta.sample_rate_hz || 1;
      const { a, b } = this.cursors;
      if (a === null || b === null) {
        this.measure = "";
      } else {
        const delta = Math.abs(b - a);
        this.measure = `Δt ${delta} samples (${formatTime(delta / rate)}), ` +
          `1/Δt ${delta ? (rate / delta).toFixed(3) : "∞"} Hz`;
      }
      if (this.onMeasure) this.onMeasure(this.measure);
    }

    // -- search -------------------------------------------------------------

    _scanDetailForEdge(index, direction) {
      if (!this.detail) return null;
      const samples = this.detail.samples;
      const mask = 1 << index;
      const start = this.view.start;
      const end = this.view.start + this.view.span;
      if (direction > 0) {
        for (let offset = Math.max(1, Math.floor(start - this.detail.start) + 1);
             offset < samples.length; offset++) {
          const time = this.detail.start + offset;
          if (time <= start) continue;
          if (time > end + 1) return null;
          if ((samples[offset] & mask) !== (samples[offset - 1] & mask)) {
            return time;
          }
        }
      } else {
        for (let offset = Math.min(samples.length - 2,
                                   Math.ceil(start - this.detail.start) - 1);
             offset >= 1; offset--) {
          const time = this.detail.start + offset;
          if (time >= start) continue;
          if ((samples[offset] & mask) !== (samples[offset - 1] & mask)) {
            return time;
          }
        }
      }
      return null;
    }

    _scanEnvelopeForEdge(index, direction) {
      const envelope = this.envelope;
      if (!envelope || !envelope.buckets) return null;
      const min = envelope.min[index];
      const max = envelope.max[index];
      const current = direction > 0 ? this.view.start + this.view.span
                                    : this.view.start;
      const bucketOf = (time) => Math.max(0, Math.min(
        envelope.buckets - 1,
        Math.floor((time / envelope.total) * envelope.buckets)));
      let bucket = bucketOf(current);
      const limit = direction > 0 ? envelope.buckets - 1 : 0;
      while (direction > 0 ? bucket <= limit : bucket >= limit) {
        if (min[bucket] !== max[bucket]) {
          return (bucket / envelope.buckets) * envelope.total;
        }
        bucket += direction;
      }
      return null;
    }

    findEdge(direction) {
      if (!this.meta) return null;
      const index = this.selectedChannel;
      let time = null;
      if (this._detailCoversView()) {
        time = this._scanDetailForEdge(index, direction);
      } else {
        time = this._scanEnvelopeForEdge(index, direction);
        if (time !== null) {
          const span = Math.min(this.view.span, DETAIL_SPAN);
          let start = time - span / 2;
          start = Math.max(0, Math.min(this.meta.total_samples - span, start));
          this.view = { start, span };
          this._maybeRequestDetail();
          this.render();
          return time;
        }
      }
      if (time === null) return null;
      this.cursors.a = time;
      this._updateMeasure();
      this._maybeRequestDetail();
      this.render();
      return time;
    }

    findPattern(direction) {
      if (!this.meta || !this.detail || !this._detailCoversView()) return null;
      const text = (this.pattern || "").trim().toLowerCase();
      if (!text) return null;
      const bits = [];
      for (const character of text) {
        if (character === "0") bits.push(0);
        else if (character === "1") bits.push(1);
        else if (character === "x") bits.push(-1);
        else return null;
      }
      const samples = this.detail.samples;
      const mask = 1 << this.selectedChannel;
      const startOffset = Math.max(0,
        Math.floor(this.view.start - this.detail.start));
      const endOffset = Math.min(samples.length - bits.length,
        Math.ceil(this.view.start + this.view.span - this.detail.start));
      const range = [];
      if (direction > 0) {
        for (let offset = startOffset; offset <= endOffset; offset++) {
          range.push(offset);
        }
      } else {
        for (let offset = endOffset; offset >= startOffset; offset--) {
          range.push(offset);
        }
      }
      for (const offset of range) {
        let match = true;
        for (let index = 0; index < bits.length; index++) {
          if (bits[index] < 0) continue;
          const level = (samples[offset + index] & mask) ? 1 : 0;
          if (level !== bits[index]) { match = false; break; }
        }
        if (match) {
          const time = this.detail.start + offset;
          this.cursors.a = time;
          this._updateMeasure();
          this.render();
          return time;
        }
      }
      return null;
    }

    screenshot(name) {
      if (typeof this.canvas.toBlob !== "function") return false;
      this.canvas.toBlob((blob) => {
        if (!blob) return;
        const link = document.createElement("a");
        link.href = URL.createObjectURL(blob);
        link.download = (name || "waveform") + ".png";
        link.click();
        URL.revokeObjectURL(link.href);
      });
      return true;
    }
  }

  window.LAWaveform = {
    WaveformView,
    parseEnvelope,
    defaultChannels,
    formatTime,
    DEFAULT_COLORS,
  };
})();
