// P2.4 la_pretrig_ring
//
// Pre-trigger history + elastic sample stream with an exact seam.
//
// ---------------------------------------------------------------------------
// Specification (required by docs/plan.md P2.4)
// ---------------------------------------------------------------------------
// Roles and schedule
//   The module keeps a DEPTH-sample circular history of the valid sample
//   stream while a capture runs. Two regimes exist:
//
//   * Stream mode (pre_req == 0, latched at run rising): the module is a
//     pure FIFO pass-through. Every stored sample is emitted in order with
//     one cycle of latency; `trig` only latches trig_index for status.
//
//   * Burst mode (pre_req > 0): until `trig` is seen the history rolls over
//     (oldest sample overwritten) and nothing is emitted. On `trig` (which
//     must coincide with in_valid, as la_trigger only pulses on valid
//     samples) the sample stream is logically cut:
//         K    = min(pre_req, trig_index, DEPTH-1)
//         pre  = samples [trig_index-K, trig_index-1]   (the retained window)
//         post = samples from trig_index onward, first post sample included
//     and the module then emits the window followed by the live post stream
//     in exact sample order. No sample is duplicated, dropped or invented at
//     the seam; the first emitted sample after the window is the trigger
//     sample itself, so POST_SAMPLES includes the trigger sample.
//
//   The window remains intact no matter how long the armed phase lasts: with
//   DEPTH a power of two greater than max(K)+1, a later write never revisits
//   a window address (evictions before the trigger only touch indices below
//   trig_index-K).
//
// Write/read concurrency
//   Stored-but-unemitted samples are tracked by `pending`; the ring is full
//   when pending == DEPTH. Writes and reads advance independently (dual-port
//   RAM schedule): an arriving sample is stored when pending < DEPTH, or when
//   an emission in the same cycle frees the drained slot (pending == DEPTH,
//   read-then-write of the same address in one cycle; the read port must
//   return the pre-write data, which holds for inferred dual-port RAM in
//   simulation and for Xilinx simple dual-port BRAM read-during-write).
//
// Overflow and the stall bound
//   Sampling cannot be paused, so an emission stall (out_ready deasserted by
//   the consumer) lets pending grow. The worst-case no-loss stall budget
//   after a trigger is DEPTH - (K+1) samples at DIV=0, i.e. (DEPTH-K-1) FCLK
//   cycles. When an arrival finds no free slot and no same-cycle emission,
//   the sample is dropped (a stream gap), `overflow` is set sticky until the
//   next run rising edge or srst, and the module keeps going so the consumer
//   can stop the capture rather than silently corrupting time/sample
//   continuity. capture_ctrl maps this to the FIFO_OVF stop policy.
//
// Capture lifecycle
//   run rising latches pre_req, clears counters, history, pending, overflow
//   and the trigger latch. While run is high samples are consumed; after the
//   controller reaches its emission target it deasserts run. Per the
//   controller protocol the pending count is zero at that point (the last
//   needed sample has been emitted); any remainder left by an overflow stop
//   is discarded at the next run edge. Pre-trigger samples before run are
//   never captured.
//
// Status outputs
//   trig_index : sample index (count of valid samples since run rising, 0
//                based) of the trigger sample, latched on trig.
//   pre_len    : K actually retained, latched on trig.
//   overflow   : sticky drop indicator described above.
//
// Timing/interface
//   Single FCLK; synchronous active-high srst. out_valid is asserted while a
//   sample is available; the pair (out_valid, out_ready) transfers one
//   sample per cycle at full rate. in_valid/out stream rates need not match.

module la_pretrig_ring #(
  parameter int DEPTH  = 65536,        // history depth; must be 2**LOG2D
  parameter int LOG2D  = 16
) (
  input  logic        clk,
  input  logic        srst,
  input  logic        run,             // capture active; config latched on rising edge
  input  logic        in_valid,        // one valid sample per pulse
  input  logic [7:0]  in_sample,
  input  logic        trig,            // trigger pulse; only meaningful with in_valid
  input  logic [15:0] pre_req,         // requested pre samples (P1.1 PRE_SAMPLES)
  input  logic        out_ready,
  output logic        out_valid,
  output logic [7:0]  out_sample,
  output logic [15:0] pre_len,         // K retained at trigger
  output logic [31:0] trig_index,      // trigger sample index
  output logic        overflow         // sticky until srst or next run edge
);

  // K = min(pre_req, trig_index, DEPTH-1); pre_req and the clamp never
  // exceed 16 bits and trig_index is compared before any truncation.
  function automatic logic [15:0] min16(input logic [15:0] a,
                                        input logic [31:0] b,
                                        input int c);
    logic [31:0] m;
    if (b >= a) m = a;
    else        m = b;
    if (m > c)  m = c;
    min16 = m[15:0];
  endfunction

  localparam int DEPTHM1 = DEPTH - 1;

  logic [7:0] mem [DEPTH];

  logic        run_d;
  logic        run_rise;
  logic [15:0] pre_req_q;
  logic        stream_mode;      // pre_req == 0: FIFO pass-through from run
  logic        latched;          // burst mode window cut taken
  logic [31:0] count;
  logic [LOG2D-1:0] wptr;
  logic [LOG2D-1:0] rptr;
  logic [LOG2D:0]   pending;     // stored-but-unemitted samples, 0..DEPTH
  logic [15:0] pre_len_q;
  logic [31:0] trig_index_q;
  logic        overflow_q;

  assign run_rise = run && !run_d;

  always_ff @(posedge clk) begin
    run_d <= run;
    if (srst) begin
      run_d        <= 1'b0;
      pre_req_q    <= '0;
      stream_mode  <= 1'b0;
      latched      <= 1'b0;
      count        <= '0;
      wptr         <= '0;
      rptr         <= '0;
      pending      <= '0;
      pre_len_q    <= '0;
      trig_index_q <= '0;
      overflow_q   <= 1'b0;
    end else if (run_rise) begin
      pre_req_q    <= pre_req;
      stream_mode  <= (pre_req == 16'd0);
      latched      <= 1'b0;
      count        <= '0;
      wptr         <= '0;
      rptr         <= '0;
      pending      <= '0;
      pre_len_q    <= '0;
      trig_index_q <= '0;
      overflow_q   <= 1'b0;
    end else if (run) begin
      // Latch the window cut on the trigger sample. The clamp to DEPTH-1
      // guarantees a request of DEPTH can never destroy its own window head;
      // capture_ctrl validates pre_req against the P1.1 maximum (0x8000).
      // A second trigger after the latch only refreshes the status fields.
      if (trig && in_valid) begin
        trig_index_q <= count;
        if (!stream_mode && !latched) begin
          rptr      <= wptr - min16(pre_req_q, count, DEPTHM1);
          pending   <= {1'b0, min16(pre_req_q, count, DEPTHM1)} + 17'd1;
          pre_len_q <= min16(pre_req_q, count, DEPTHM1);
          latched   <= 1'b1;
        end
      end
      // Store path and emission path with a single pending update.
      // store_ok: guarded-store admitted (free slot, or the slot freed by an
      // emission in this same cycle); rolling history stores are not
      // pending-tracked and admitted unconditionally.
      if (in_valid) begin
        if (stream_mode || latched) begin
          if ((pending < DEPTH[LOG2D:0]) || (out_valid && out_ready)) begin
            mem[wptr]  <= in_sample;
            wptr       <= wptr + 1'b1;
          end else begin
            overflow_q <= 1'b1;   // drop: stream gap, never silent
          end
        end else begin
          mem[wptr]  <= in_sample;   // rolling history
          wptr       <= wptr + 1'b1;
        end
        count <= count + 32'd1;
      end
      if (out_valid && out_ready) begin
        rptr <= rptr + 1'b1;
      end
      if (in_valid && (stream_mode || latched)
          && ((pending < DEPTH[LOG2D:0]) || (out_valid && out_ready))
          && !(out_valid && out_ready)) begin
        pending <= pending + 17'd1;
      end else if (!(in_valid && (stream_mode || latched)
                     && ((pending < DEPTH[LOG2D:0])
                         || (out_valid && out_ready)))
                   && (out_valid && out_ready)) begin
        pending <= pending - 17'd1;
      end
    end
  end

  assign out_valid  = run && (stream_mode || latched) && (pending != '0);
  assign out_sample = mem[rptr];
  assign pre_len    = pre_len_q;
  assign trig_index = trig_index_q;
  assign overflow   = overflow_q;

endmodule : la_pretrig_ring
