// P2.7 la_capture_ctrl
//
// Capture state machine: IDLE -> ARMED -> CAPT(triggered) -> DRAIN -> IDLE
// for burst captures, and ARMED -> CAPT (until abort/overflow) for live
// streaming, with trigger, timeout, abort and overflow handling plus packet
// flush scheduling for the encoder/packer stage.
//
// Roles
//   * Owns ring run control (ring_run_o), the burst emission target and
//     sample countdown, the timeout watchdog (burst ARMED phase only), the
//     live flush cadence (FCLK timer and, in txn mode, a 64K-word flush),
//     overflow stop and the flush/done sequence at the end of a capture.
//     It never touches the sample data: the encoder/packer consume the ring
//     sample stream directly and their flush requests come from here.
//   * Configuration (mode, stream, pre_req, post, timeout, live flush
//     cycles, max run) is latched at the arm edge. Arm while not IDLE is a
//     no-op; abort returns to IDLE immediately and discards the partial
//     capture without a flush; timeout aborts like abort.
//
// Burst sequence (stream_en = 0)
//   ARMED: the ring runs and history rolls. Timeout counts valid samples
//   since arm; at the limit the capture aborts. On trig the trigger event
//   fires and the state moves to CAPT with a remaining emission count of
//   K + POST, where K = min(PRE_SAMPLES, samples seen) is computed here
//   (the ring clamps identically, so the countdown matches its window).
//   The controller counts ring sample emissions; the emission that empties
//   the countdown is the last needed sample: the ring is stopped (any
//   in-flight remainder, e.g. the trigger sample of a window-only capture,
//   is discarded by the ring at the run drop), a flush closes the pending
//   encoder run or raw tail, and the controller DRAINs until the closing
//   word (tlast) is seen or a small no-progress bound expires (an empty or
//   word-aligned ending never produces a closing beat). POST counts samples
//   from the trigger sample onward (the ring emits the window, then the
//   trigger sample first among the post samples), so POST=0 captures only
//   the pre window.
//
// Stream sequence (stream_en = 1)
//   The ring runs in pass-through (la_top forces pre_req = 0 in stream
//   mode). A trigger is optional and fires the trigger event once per arm
//   (a continuously true trigger does not re-fire). Flushes
//   close packets: a timer counts FCLK cycles since the previous flush
//   (LIVE_FLUSH_CYCLES, 0 = disabled) and in txn mode a 16-bit word count
//   since the previous flush requests a flush at 65536 words; in raw mode
//   the packer closes its own 64K-word packets, so only the timer flushes.
//
// Overflow
//   Sampling cannot be paused. When the ring or the FIFO reports (sticky)
//   overflow during CAPT, the controller stops the ring, reports the
//   overflow event and DRAINs the partial stream with a flush before
//   reporting done. A gap is never silent and a no-progress stall is never
//   allowed to spin.
//
// Outputs
//   Events are single-cycle pulses (ev_triggered_o, ev_done_o,
//   ev_aborted_o, ev_overflow_o). armed_o is high in ARMED; streaming_o is
//   high while a stream capture is running (ARMED or CAPT). Word counters
//   cover the capture from arm through the closing word. pkt_words_o carries
//   the raw burst packet word count (ceil((K+POST)/4), latched at the
//   trigger; 0 in stream or txn mode) so the raw packer can close the packet
//   on its final word.
//
// Interface: single FCLK, synchronous active-high srst; ports described in
// the port list.

module la_capture_ctrl (
  input  logic        clk,
  input  logic        srst,
  // commands (pulses)
  input  logic        arm_i,
  input  logic        abort_i,
  // configuration (latched on arm)
  input  logic        mode_enc_i,       // 0 = raw packed, 1 = transition
  input  logic        stream_en_i,
  input  logic [15:0] pre_req_i,
  input  logic [31:0] post_cnt_i,
  input  logic [31:0] timeout_i,        // sample clocks; 0 = no timeout
  input  logic [31:0] live_flush_cycles_i,
  input  logic [23:0] txn_max_run_i,
  // sample domain
  input  logic        sample_valid_i,
  input  logic        trig_i,           // trigger pulse (aligned with samples)
  // ring
  output logic        ring_run_o,
  input  logic        ring_overflow_i,  // sticky ring overflow
  input  logic        sample_emit_i,    // ring out_valid && out_ready
  // word side (encoder/packer taps)
  input  logic        word_valid_i,
  input  logic        word_ready_i,
  input  logic        word_last_i,
  output logic        flush_o,          // flush request pulse
  input  logic        fifo_ovf_i,       // sticky FIFO overflow
  // events / status
  output logic        ev_triggered_o,
  output logic        ev_done_o,
  output logic        ev_aborted_o,
  output logic        ev_overflow_o,
  output logic        armed_o,
  output logic        streaming_o,
  output logic [31:0] word_count_lo_o,
  output logic [31:0] word_count_hi_o,
  output logic        txn_mode_o,
  output logic [23:0] txn_max_run_o,
  // Exact word count of a raw burst packet (0 in stream/txn mode): the raw
  // packer tags the final word with tlast from this value, so a word-aligned
  // burst still closes its DMA packet instead of deferring tlast forever.
  output logic [31:0] pkt_words_o
);

  localparam logic [1:0] S_IDLE  = 2'd0;
  localparam logic [1:0] S_ARMED = 2'd1;
  localparam logic [1:0] S_CAPT  = 2'd2;
  localparam logic [1:0] S_DRAIN = 2'd3;
  localparam logic [3:0] DRAIN_BOUND = 4'd8;

  logic [1:0]  state_q;
  logic [23:0] max_run_q;
  logic        txn_q;
  logic        stream_q;
  logic [31:0] post_q;
  logic [31:0] timeout_q;
  logic [31:0] live_q;
  logic [15:0] pre_req_q;
  logic [31:0] s_elapsed_q;      // valid samples since arm (burst)
  logic [31:0] rem_q;            // burst emissions remaining (K + POST)
  logic [31:0] pkt_words_q;      // raw burst packet words (ceil((K+POST)/4))
  logic [31:0] words_lo_q;
  logic [31:0] words_hi_q;
  logic [31:0] cycles_q;         // FCLK cycles since the last live flush
  logic [15:0] words_since_q;    // words since the last live flush (txn)
  logic        done_q;           // report ev_done while in IDLE
  logic [3:0]  drain_cnt_q;
  logic        flush_pulse_q;
  logic        last_word_seen_q;
  logic        ev_trig_r;
  logic        ev_abort_r;
  logic        ev_ovf_r;
  logic        trig_seen_q;      // stream trigger is one-shot per arm

  assign ring_run_o    = (state_q == S_ARMED) || (state_q == S_CAPT);
  assign armed_o       = (state_q == S_ARMED);
  assign streaming_o   = stream_q && (state_q != S_IDLE);
  assign ev_done_o     = done_q && (state_q == S_IDLE);
  assign txn_mode_o    = txn_q;
  assign txn_max_run_o = max_run_q;
  assign ev_triggered_o = ev_trig_r;
  assign ev_aborted_o   = ev_abort_r;
  assign ev_overflow_o  = ev_ovf_r;
  assign flush_o        = flush_pulse_q;
  assign word_count_lo_o = words_lo_q;
  assign word_count_hi_o = words_hi_q;
  assign pkt_words_o     = pkt_words_q;

  always_ff @(posedge clk) begin
    if (srst) begin
      state_q       <= S_IDLE;
      txn_q         <= 1'b0;
      stream_q      <= 1'b0;
      post_q        <= '0;
      timeout_q     <= '0;
      live_q        <= '0;
      pre_req_q     <= '0;
      max_run_q     <= 24'hFFFFFF;
      s_elapsed_q   <= '0;
      rem_q         <= '0;
      pkt_words_q   <= '0;
      words_lo_q    <= '0;
      words_hi_q    <= '0;
      cycles_q      <= '0;
      words_since_q <= '0;
      done_q        <= 1'b0;
      drain_cnt_q   <= '0;
      flush_pulse_q <= 1'b0;
      last_word_seen_q <= 1'b0;
      ev_trig_r     <= 1'b0;
      ev_abort_r    <= 1'b0;
      ev_ovf_r      <= 1'b0;
      trig_seen_q   <= 1'b0;
    end else begin
      // One-cycle pulse defaults.
      ev_trig_r     <= 1'b0;
      ev_abort_r    <= 1'b0;
      ev_ovf_r      <= 1'b0;
      flush_pulse_q <= 1'b0;
      done_q        <= 1'b0;

      if (arm_i && (state_q == S_IDLE)) begin
        txn_q           <= mode_enc_i;
        stream_q        <= stream_en_i;
        pre_req_q       <= pre_req_i;
        post_q          <= post_cnt_i;
        timeout_q       <= timeout_i;
        live_q          <= live_flush_cycles_i;
        max_run_q       <= txn_max_run_i;
        s_elapsed_q     <= '0;
        pkt_words_q     <= '0;
        words_lo_q      <= '0;
        words_hi_q      <= '0;
        cycles_q        <= '0;
        words_since_q   <= '0;
        drain_cnt_q     <= '0;
        last_word_seen_q <= 1'b0;
        trig_seen_q     <= 1'b0;
        // Stream captures run straight into CAPT: there is nothing to wait
        // for (the trigger is optional and only fires an event).
        if (stream_en_i) state_q <= S_CAPT;
        else             state_q <= S_ARMED;
      end else begin
        case (state_q)
          S_IDLE: begin
          end
          S_ARMED: begin
            if (abort_i) begin
              state_q <= S_IDLE;
              ev_abort_r <= 1'b1;
            end else if (!stream_q && timeout_q != 32'd0
                        && sample_valid_i
                        && (s_elapsed_q + 32'd1 >= timeout_q)) begin
              // Trigger timeout: abort.
              state_q <= S_IDLE;
              ev_abort_r <= 1'b1;
            end else if (trig_i) begin
              ev_trig_r <= 1'b1;
              state_q <= S_CAPT;
              if (!stream_q) begin
                // Burst: K = min(pre_req, samples seen so far); the ring
                // clamps identically, so the countdown matches its window.
                // POST counts from the trigger sample onward. The raw
                // packet word count is latched here so the packer can close
                // the packet on its final word (txn mode uses the encoder
                // flush instead).
                if (pre_req_q <= s_elapsed_q[15:0]) begin
                  rem_q <= {16'd0, pre_req_q} + post_q;
                  pkt_words_q <= txn_q ? 32'd0
                                       : (({16'd0, pre_req_q} + post_q + 32'd3) >> 2);
                end else begin
                  rem_q <= s_elapsed_q + post_q;
                  pkt_words_q <= txn_q ? 32'd0
                                       : ((s_elapsed_q + post_q + 32'd3) >> 2);
                end
              end
            end
            if (!stream_q && sample_valid_i) begin
              s_elapsed_q <= s_elapsed_q + 32'd1;
            end
          end
          S_CAPT: begin
            if (abort_i) begin
              state_q <= S_IDLE;
              ev_abort_r <= 1'b1;
            end else if (stream_q && trig_i && !trig_seen_q) begin
              // Optional live trigger: event only, one-shot per arm. A
              // continuously true trigger (e.g. mask/edges zero) must not
              // re-fire, or the IRQ status bit would never clear.
              ev_trig_r   <= 1'b1;
              trig_seen_q <= 1'b1;
            end else if (ring_overflow_i || fifo_ovf_i) begin
              // Overflow: never silent. Stop, deliver the partial stream.
              ev_ovf_r <= 1'b1;
              state_q <= S_DRAIN;
              flush_pulse_q <= 1'b1;
              drain_cnt_q <= '0;
              last_word_seen_q <= 1'b0;
            end else begin
              if (stream_q) begin
                // Live flush cadence.
                if (live_q != 32'd0 && cycles_q >= live_q) begin
                  flush_pulse_q <= 1'b1;
                  cycles_q <= '0;
                  words_since_q <= '0;
                end else begin
                  cycles_q <= cycles_q + 32'd1;
                end
                if (txn_q && word_valid_i && word_ready_i) begin
                  if (words_since_q == 16'hFFFF) begin
                    flush_pulse_q <= 1'b1;
                    words_since_q <= '0;
                  end else begin
                    words_since_q <= words_since_q + 16'd1;
                  end
                end
              end else begin
                // Burst countdown. The emission that empties the remaining
                // count is the last needed sample; the controller then stops
                // the ring (leftovers, e.g. the trigger sample of a
                // window-only capture, are discarded at the run drop) and
                // drains the encoder/packer tail.
                if (rem_q == 32'd0) begin
                  state_q <= S_DRAIN;   // K + POST = 0: nothing to capture
                  drain_cnt_q <= '0;
                  last_word_seen_q <= 1'b0;
                end else if (sample_emit_i) begin
                  rem_q <= rem_q - 32'd1;
                  if (rem_q == 32'd1) begin
                    state_q <= S_DRAIN;
                    flush_pulse_q <= 1'b1;
                    drain_cnt_q <= '0;
                    last_word_seen_q <= 1'b0;
                  end
                end
              end
            end
          end
          S_DRAIN: begin
            if (word_valid_i && word_ready_i && word_last_i) begin
              last_word_seen_q <= 1'b1;
            end
            if (last_word_seen_q || (drain_cnt_q >= DRAIN_BOUND)) begin
              state_q <= S_IDLE;
              done_q <= 1'b1;
            end else begin
              drain_cnt_q <= drain_cnt_q + 4'd1;
            end
          end
          default: state_q <= S_IDLE;
        endcase
      end

      // Word counter: capture spans ARMED..DRAIN (closing word included).
      if (state_q != S_IDLE) begin
        if (word_valid_i && word_ready_i) begin
          words_lo_q <= words_lo_q + 32'd1;
          if (words_lo_q == 32'hFFFFFFFF) words_hi_q <= words_hi_q + 32'd1;
        end
      end
    end
  end

endmodule : la_capture_ctrl
