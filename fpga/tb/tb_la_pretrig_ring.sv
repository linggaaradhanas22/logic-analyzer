// Self-checking test for la_pretrig_ring (P2.4).
//
// Independent scoreboard: every emitted sample must equal the next expected
// sample index (window start trig_index-K, then strictly consecutive), so
// duplicates, losses and seam errors all fail. A pending/drop mirror checks
// the overflow rule and drop count. Scenarios: stream pass-through with
// stalls and a status trigger; burst with K < pre-req; trigger before the
// requested depth (K = trig_index); history wrap beyond DEPTH; K clamped to
// DEPTH-1 with a full ring (same-cycle read/write at full capacity);
// overflow during a downstream stall with exact drop count and a clean
// second run; double trigger during a stalled replay; abort mid-replay.
//
// The DUT is instantiated at DEPTH=512 (2**9) so full-capacity behavior is
// exercised in simulation time.

`timescale 1ns/1ps

module tb_la_pretrig_ring;
  localparam int D = 512;
  localparam int LOG2D = 9;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic run = 1'b0;
  logic in_valid = 1'b0;
  logic [7:0] in_sample = 8'h00;
  logic trig = 1'b0;
  logic [15:0] pre_req = 16'd0;
  logic out_ready = 1'b1;

  logic out_valid;
  logic [7:0] out_sample;
  logic [15:0] pre_len;
  logic [31:0] trig_index;
  logic overflow;

  la_pretrig_ring #(
    .DEPTH(D),
    .LOG2D(LOG2D)
  ) dut (
    .clk(clk),
    .srst(srst),
    .run(run),
    .in_valid(in_valid),
    .in_sample(in_sample),
    .trig(trig),
    .pre_req(pre_req),
    .out_ready(out_ready),
    .out_valid(out_valid),
    .out_sample(out_sample),
    .pre_len(pre_len),
    .trig_index(trig_index),
    .overflow(overflow)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;
  integer tb_pulse = 0;

  // ------------------------------------------------------------------
  // Scoreboard model (clocked; all reads are pre-edge values).
  //
  // The model mirrors admit/drop/pending and keeps its own value FIFO of
  // admitted samples. Every ring emission must pop the FIFO front and match
  // its value, so order, seam and gap behavior are checked end to end:
  // duplicates, losses and wrong window starts all fail.
  // ------------------------------------------------------------------
  logic        m_run_d = 1'b0;
  logic        m_stream = 1'b0;
  logic        m_latched = 1'b0;
  logic [31:0] m_count = '0;
  logic [15:0] m_pre_req = '0;
  logic [LOG2D:0] m_pend = '0;
  logic [7:0]  m_qmem [D];        // admitted sample values, FIFO
  integer      m_qh = 0;          // read index (first admitted sample)
  integer      m_qt = 0;          // write index
  integer      m_emitted = 0;     // emissions observed
  integer      m_dropped = 0;     // drops observed
  integer      model_errors = 0;
  logic        m_overflow_seen = 1'b0;

  task automatic model_error(string tag, logic condition, string extra);
    if (!condition) begin
      $display("FAIL [%0t] %s%s", $time, tag, extra);
      model_errors = model_errors + 1;
    end
  endtask

  always @(posedge clk) begin
    logic emit;
    logic admit;
    logic drop;
    logic [15:0] k;
    if (srst) begin
      m_run_d <= 1'b0;
      m_stream <= 1'b0;
      m_latched <= 1'b0;
      m_count <= '0;
      m_pre_req <= '0;
      m_pend <= '0;
      m_qh <= 0;
      m_qt <= 0;
      m_emitted <= 0;
      m_dropped <= 0;
      m_overflow_seen <= 1'b0;
    end else begin
      m_run_d <= run;
      if (run && !m_run_d) begin
        m_stream <= (pre_req == 16'd0);
        m_latched <= 1'b0;
        m_count <= '0;
        m_pre_req <= pre_req;
        m_pend <= '0;
        m_qh <= 0;
        m_qt <= 0;
        m_emitted <= 0;
        m_dropped <= 0;
        m_overflow_seen <= 1'b0;
      end else if (run) begin
        emit  = out_valid && out_ready;
        admit = in_valid && (m_stream || m_latched)
                && ((m_pend < D[LOG2D:0]) || emit);
        drop  = in_valid && (m_stream || m_latched) && !((m_pend < D[LOG2D:0]) || emit);

        // Push admitted samples; pre-trigger burst history rolls over.
        if (in_valid) begin
          m_count <= m_count + 32'd1;
          if (m_stream || m_latched) begin
            if (admit) begin
              m_qmem[m_qt % D] <= in_sample;
              m_qt <= m_qt + 1;
            end
          end else begin
            if (m_qt - m_qh == D) m_qh <= m_qh + 1;   // rolling history
            m_qmem[m_qt % D] <= in_sample;
            m_qt <= m_qt + 1;
          end
        end

        // Latch: the queue tail now holds the trigger sample; set the head
        // so the queue becomes exactly [T-K .. T] (window + trigger sample).
        // Runs after the push above so this assignment wins the edge.
        if (trig && in_valid && !m_stream && !m_latched) begin
          k = (m_pre_req < m_count[15:0]) ? m_pre_req : m_count[15:0];
          if (k > (D - 1)) k = D - 1;
          m_latched <= 1'b1;
          m_pend <= {1'b0, k} + 17'd1;      // window + trigger sample
          m_qh <= m_qt - k;
        end

        if (emit) begin
          model_error("emission must match the admitted stream",
                      m_qt - m_qh > 0, "");
          model_error("emitted sample value",
                      out_sample == m_qmem[m_qh % D],
                      $sformatf(" got %02h expected %02h", out_sample,
                                m_qmem[m_qh % D]));
          m_qh <= m_qh + 1;
          m_emitted <= m_emitted + 1;
        end
        if (admit && !emit) m_pend <= m_pend + 17'd1;
        else if (!admit && emit) m_pend <= m_pend - 17'd1;
        if (drop) begin
          m_dropped <= m_dropped + 1;
          m_overflow_seen <= 1'b1;
        end
        model_error("overflow flag matches model",
                    overflow == m_overflow_seen, "");
      end
    end
  end

  // ------------------------------------------------------------------
  // Helpers.
  // ------------------------------------------------------------------
  always #(CLK_PERIOD / 2.0) clk = ~clk;

  // Drive `n` valid samples spaced `gap` cycles apart (value = pulse mod 256),
  // optionally asserting trig on sample number `trig_at` (-1 disables).
  task automatic drive_samples(int n, int gap, int trig_at, input logic en_ready,
                               output integer drops_after);
    int c;
    int p;
    p = 0;
    while (p < n) begin
      // idle gap cycles
      for (c = 0; c < gap; c++) begin
        @(negedge clk);
        in_valid = 1'b0;
        trig = 1'b0;
        out_ready = en_ready;
      end
      @(negedge clk);
      in_valid = 1'b1;
      trig = (p == trig_at);
      in_sample = tb_pulse[7:0];
      out_ready = en_ready;
      tb_pulse = tb_pulse + 1;
      p = p + 1;
    end
    // Quiesce: never leave in_valid asserted past the final pulse.
    @(negedge clk);
    in_valid = 1'b0;
    trig = 1'b0;
    drops_after = m_dropped;
  endtask

  task automatic quiet(int n);
    int c;
    for (c = 0; c < n; c++) begin
      @(negedge clk);
      in_valid = 1'b0;
      trig = 1'b0;
      in_sample = 8'h00;
    end
  endtask

  // Hold run until the ring has fully drained, then drop it.
  task automatic finish_run();
    int c;
    c = 0;
    while (c < 8) begin
      @(posedge clk);
      if (out_valid) c = 0;
      else c = c + 1;
    end
    run = 1'b0;
  endtask

  task automatic check_status(input string tag, input int exp_pre_len,
                              input int exp_trig_index);
    // Latched outputs are stable one cycle after the trigger sample edge.
    @(negedge clk);
    if (pre_len !== exp_pre_len[15:0]) begin
      $display("FAIL [%0t] %s pre_len=%0d expected %0d", $time, tag,
               pre_len, exp_pre_len);
      errors = errors + 1;
    end
    if (trig_index !== exp_trig_index[31:0]) begin
      $display("FAIL [%0t] %s trig_index=%0d expected %0d", $time, tag,
               trig_index, exp_trig_index);
      errors = errors + 1;
    end
  endtask

  task automatic end_phase(string tag, input int exp_emitted, input int exp_dropped);
    @(negedge clk);
    if (m_emitted != exp_emitted) begin
      $display("FAIL [%0t] %s emitted=%0d expected %0d", $time, tag,
               m_emitted, exp_emitted);
      errors = errors + 1;
    end
    if (m_dropped != exp_dropped) begin
      $display("FAIL [%0t] %s dropped=%0d expected %0d", $time, tag,
               m_dropped, exp_dropped);
      errors = errors + 1;
    end
  endtask

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // Reset.
    repeat (6) @(posedge clk);
    srst = 1'b0;
    repeat (3) @(posedge clk);
    if (out_valid || overflow || pre_len !== 16'd0 || trig_index !== 32'd0) begin
      $display("FAIL reset did not clear ring outputs");
      errors = errors + 1;
    end

    // ----------------------------------------------------------------
    // Phase 1: stream pass-through with stalls and a status trigger.
    // 300 pulses, irregular gaps, two 10-cycle downstream stalls.
    // ----------------------------------------------------------------
    begin
      int d;
      pre_req = 16'd0;
      run = 1'b1;
      @(negedge clk);
      // Segment A: 130 pulses, no stalls, trigger at pulse 55.
      tb_pulse = 0;
      quiet(2);
      drive_samples(130, 1, 55, 1'b1, d);
      check_status("s1 trig latch", 0, 55);
      // Segment B: stall window 1 (10 cycles, arrivals continue at DIV=0).
      begin
        int c;
        for (c = 0; c < 10; c++) begin
          @(negedge clk);
          in_valid = 1'b1;
          trig = 1'b0;
          in_sample = tb_pulse[7:0];
          tb_pulse = tb_pulse + 1;
          out_ready = 1'b0;          // downstream stall
        end
        @(negedge clk);
        in_valid = 1'b0;
        trig = 1'b0;
      end
      // Segment C: 60 pulses with irregular gaps.
      drive_samples(60, 0, -1, 1'b1, d);   // every cycle
      // Segment D: stall window 2.
      begin
        int c;
        for (c = 0; c < 10; c++) begin
          @(negedge clk);
          in_valid = 1'b1;
          trig = 1'b0;
          in_sample = tb_pulse[7:0];
          tb_pulse = tb_pulse + 1;
          out_ready = 1'b0;
        end
        @(negedge clk);
        in_valid = 1'b0;
        trig = 1'b0;
      end
      // Segment E: 90 more pulses with gaps of 2.
      drive_samples(90, 2, -1, 1'b1, d);
      // Total pulses 130+10+60+10+90 = 300.
      finish_run();
      quiet(2);
      end_phase("stream", 300, 0);
      if (overflow) begin
        $display("FAIL stream overflow unexpectedly set");
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Phase 2a: burst, trig at 50 with pre_req 30 -> window [20..49].
    // ----------------------------------------------------------------
    begin
      int d;
      run = 1'b0;
      quiet(2);
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      pre_req = 16'd30;
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      // No emission may occur before the trigger.
      @(negedge clk);
      if (out_valid) begin
        $display("FAIL burst emitted before trigger");
        errors = errors + 1;
      end
      drive_samples(50, 1, 49, 1'b1, d);   // trigger sample index 49
      check_status("s2a latch", 30, 49);
      drive_samples(20, 1, -1, 1'b1, d);
      // Window 30 + trigger sample + 20 post = 51, no drops.
      finish_run();
      quiet(2);
      end_phase("burst-small", 51, 0);
    end

    // ----------------------------------------------------------------
    // Phase 2b: trigger early; K clamps to trig_index = 7.
    // ----------------------------------------------------------------
    begin
      int d;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      pre_req = 16'd100;
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      drive_samples(7, 1, 6, 1'b1, d);     // trigger at index 6
      check_status("s2b latch", 6, 6);
      drive_samples(10, 1, -1, 1'b1, d);
      finish_run();
      quiet(2);
      end_phase("burst-early", 6 + 11, 0);
    end

    // ----------------------------------------------------------------
    // Phase 3: history wrap (2000 samples > DEPTH 512) and K clamped to
    // DEPTH-1 with the ring exactly full: window 511 + trigger sample,
    // arrivals and emissions every cycle, no drops.
    // ----------------------------------------------------------------
    begin
      int d;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      pre_req = 16'd512;                   // exceeds DEPTH-1 by design
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      drive_samples(2000, 0, 1999, 1'b1, d);  // trigger sample index 1999
      check_status("s3 clamp", 511, 1999);
      drive_samples(300, 0, -1, 1'b1, d);     // full ring, every cycle
      finish_run();
      quiet(2);
      end_phase("burst-full", 511 + 301, 0);
      if (overflow) begin
        $display("FAIL full-ring burst overflowed unexpectedly");
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Phase 4: downstream stall until overflow. Window K=100 (pending 101),
    // free = 512-101 = 411, then 460 arrival-only stall cycles -> 49 drops,
    // sticky overflow, then drain with 200 clean pulses.
    // ----------------------------------------------------------------
    begin
      int d;
      int c;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      pre_req = 16'd100;
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      // out_ready stays 0 through the trigger and the stall, so the free
      // space is exactly DEPTH-(K+1) = 411 and 49 samples drop.
      drive_samples(150, 1, 149, 1'b0, d);  // window [50..149]
      // 460 arrival cycles with out_ready=0.
      for (c = 0; c < 460; c++) begin
        @(negedge clk);
        in_valid = 1'b1;
        trig = 1'b0;
        in_sample = tb_pulse[7:0];
        tb_pulse = tb_pulse + 1;
        out_ready = 1'b0;
      end
      @(negedge clk);
      in_valid = 1'b0;
      trig = 1'b0;
      quiet(2);
      if (!overflow) begin
        $display("FAIL overflow not sticky after stall drops");
        errors = errors + 1;
      end
      drive_samples(200, 0, -1, 1'b1, d);
      // Emitted: window 100 + stored post (661 arrivals - 49 drops) = 712.
      finish_run();
      quiet(2);
      end_phase("overflow", 100 + 661 - 49, 49);
    end

    // ----------------------------------------------------------------
    // Phase 5: second run clears overflow; double trigger during a
    // stalled replay refreshes trig_index but must not corrupt the stream.
    // ----------------------------------------------------------------
    begin
      int d;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      if (overflow) begin
        $display("FAIL srst did not clear overflow");
        errors = errors + 1;
      end
      pre_req = 16'd20;
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      drive_samples(20, 1, 19, 1'b1, d);   // window [0..18], K = 19
      check_status("s5 first latch", 19, 19);
      // Stalled replay (no emissions for 8 cycles, arrivals continue).
      begin
        int c;
        for (c = 0; c < 8; c++) begin
          @(negedge clk);
          in_valid = 1'b1;
          trig = (c == 4);                  // second trigger during replay
          in_sample = tb_pulse[7:0];
          tb_pulse = tb_pulse + 1;
          out_ready = 1'b0;
        end
        @(negedge clk);
        in_valid = 1'b0;
        trig = 1'b0;
      end
      quiet(2);
      // trig_index refreshed by the second trigger (pulse 19+5 = 24);
      // pre_len keeps the first window length.
      @(negedge clk);
      if (pre_len !== 16'd19) begin
        $display("FAIL s5 pre_len changed by second trigger: %0d", pre_len);
        errors = errors + 1;
      end
      if (trig_index !== 32'd24) begin
        $display("FAIL s5 trig_index not refreshed: %0d", trig_index);
        errors = errors + 1;
      end
      drive_samples(30, 0, -1, 1'b1, d);
      // Emitted: window 19 + (39 arrivals, 0 drops) = 58; drops 0.
      finish_run();
      quiet(2);
      end_phase("double-trigger", 58, 0);
    end

    // ----------------------------------------------------------------
    // Phase 6: abort mid-replay: run drops while the window is pending;
    // emissions stop and the next capture starts clean.
    // ----------------------------------------------------------------
    begin
      int d;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      pre_req = 16'd40;
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      drive_samples(10, 1, 9, 1'b1, d);     // K = min(40,10) = 10
      quiet(1);
      run = 1'b0;                           // abort while window pending
      @(negedge clk);
      quiet(4);
      if (out_valid) begin
        $display("FAIL emissions after abort");
        errors = errors + 1;
      end
      // Rerun: a full burst still works after the abort.
      run = 1'b1;
      @(negedge clk);
      tb_pulse = 0;
      quiet(2);
      drive_samples(12, 1, 11, 1'b1, d);
      check_status("s6 relatch", 11, 11);
      drive_samples(8, 1, -1, 1'b1, d);
      finish_run();
      quiet(2);
      end_phase("abort-rerun", 20, 0);
    end

    if (model_errors != 0 || errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_pretrig_ring errors=%0d model=%0d",
                errors, model_errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_pretrig_ring failed with %0d errors (model %0d)",
             errors, model_errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_pretrig_ring");
    $fclose(result_fd);
    $display("PASS: tb_la_pretrig_ring");
    $finish;
  end

  initial begin
    #5_000_000;
    $fatal(1, "tb_la_pretrig_ring timeout");
  end

endmodule : tb_la_pretrig_ring




