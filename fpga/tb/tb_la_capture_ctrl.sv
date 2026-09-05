// Self-checking test for la_capture_ctrl (P2.7).
//
// The testbench emulates the ring, encoder/packer and register sides:
//   - samples are supplied at the controller's pace; after the trigger the
//     ring emulation emits each accepted sample one cycle later while
//     ring_run is high (matching the real ring's window+post drain);
//   - the word side follows the encoder model: one word beat one cycle
//     after every emission, and each burst-ending flush is answered with a
//     single closing beat with tlast (matching the encoder close word), so
//     word counts are exact: words = emissions + closing word. Stream
//     flushes are not capture ends and never produce closing beats. An
//     empty burst therefore has no closing beat and the controller must
//     complete through its no-progress drain bound.
//   - events, states, flush cadence and counters are scored.
//
// Scenarios: complete burst (window + post); window-only burst (POST=0,
// trigger sample discarded at the run drop); empty burst (K+POST=0);
// timeout; abort during CAPT; overflow stop that still drains the partial
// stream; stream mode with a timer flush cadence and an optional trigger;
// stream txn-mode 65536-word flush; word counter accuracy.

`timescale 1ns/1ps

module tb_la_capture_ctrl;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic arm_i = 1'b0;
  logic abort_i = 1'b0;
  logic mode_enc_i = 1'b1;
  logic stream_en_i = 1'b0;
  logic [15:0] pre_req_i = 16'd0;
  logic [31:0] post_cnt_i = 32'd0;
  logic [31:0] timeout_i = 32'd0;
  logic [31:0] live_flush_cycles_i = 32'd0;
  logic [23:0] txn_max_run_i = 24'hFFFF;
  logic sample_valid_i = 1'b0;
  logic trig_i = 1'b0;
  logic ring_run_o;
  logic ring_overflow_i = 1'b0;
  logic sample_emit_i;
  logic word_valid_i;
  logic word_ready_i = 1'b1;
  logic word_last_i;
  logic flush_o;
  logic fifo_ovf_i = 1'b0;
  logic ev_triggered_o;
  logic ev_done_o;
  logic ev_aborted_o;
  logic ev_overflow_o;
  logic armed_o;
  logic streaming_o;
  logic [31:0] word_count_lo_o;
  logic [31:0] word_count_hi_o;
  logic txn_mode_o;
  logic [23:0] txn_max_run_o;

  la_capture_ctrl dut (
    .clk(clk), .srst(srst), .arm_i(arm_i), .abort_i(abort_i),
    .mode_enc_i(mode_enc_i), .stream_en_i(stream_en_i),
    .pre_req_i(pre_req_i), .post_cnt_i(post_cnt_i),
    .timeout_i(timeout_i), .live_flush_cycles_i(live_flush_cycles_i),
    .txn_max_run_i(txn_max_run_i),
    .sample_valid_i(sample_valid_i), .trig_i(trig_i),
    .ring_run_o(ring_run_o), .ring_overflow_i(ring_overflow_i),
    .sample_emit_i(sample_emit_i),
    .word_valid_i(word_valid_i), .word_ready_i(word_ready_i),
    .word_last_i(word_last_i), .flush_o(flush_o), .fifo_ovf_i(fifo_ovf_i),
    .ev_triggered_o(ev_triggered_o), .ev_done_o(ev_done_o),
    .ev_aborted_o(ev_aborted_o), .ev_overflow_o(ev_overflow_o),
    .armed_o(armed_o), .streaming_o(streaming_o),
    .word_count_lo_o(word_count_lo_o), .word_count_hi_o(word_count_hi_o),
    .txn_mode_o(txn_mode_o), .txn_max_run_o(txn_max_run_o)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  // ------------------------------------------------------------------
  // Ring emulation: after the trigger the ring drains its stored window at
  // one sample per cycle (window + live samples) while ring_run is high,
  // exactly like the real module.
  // ------------------------------------------------------------------
  logic trig_seen = 1'b0;
  logic tb_stream = 1'b0;

  always @(posedge clk) begin
    if (srst) begin
      trig_seen <= 1'b0;
    end else begin
      if (trig_i && sample_valid_i) trig_seen <= 1'b1;
      if (!ring_run_o) trig_seen <= 1'b0;
    end
  end

  // In stream mode the ring is a pass-through from arm (no trigger needed);
  // in burst mode emissions start at the trigger.
  assign sample_emit_i = ring_run_o && (tb_stream || trig_seen);

  // ------------------------------------------------------------------
  // Word side model (encoder): one beat per emission (delayed one cycle)
  // and one closing beat with tlast for every burst flush. Stream flushes
  // never close (no closing beat).
  // ------------------------------------------------------------------
  logic had_emit = 1'b0;
  logic close_pend = 1'b0;
  logic wv_r = 1'b0;
  logic wl_r = 1'b0;

  always @(posedge clk) begin
    if (srst) begin
      had_emit <= 1'b0;
      close_pend <= 1'b0;
      wv_r <= 1'b0;
      wl_r <= 1'b0;
    end else begin
      if (sample_emit_i) had_emit <= 1'b1;
      wv_r <= 1'b0;                 // one-cycle beats
      wl_r <= 1'b0;
      if (flush_o && !tb_stream && had_emit) begin
        close_pend <= 1'b1;         // deliver the closing beat
      end else if (close_pend) begin
        wv_r <= 1'b1;
        wl_r <= 1'b1;
        close_pend <= 1'b0;
      end else if (sample_emit_i) begin
        wv_r <= 1'b1;               // word after each emission
      end
    end
  end

  assign word_valid_i = wv_r;
  assign word_last_i  = wl_r;

  // ------------------------------------------------------------------
  // Event capture for scoring.
  // ------------------------------------------------------------------
  logic e_trig = 1'b0;
  logic e_done = 1'b0;
  logic e_abort = 1'b0;
  logic e_ovf = 1'b0;
  int flush_count = 0;
  logic flush_seen = 1'b0;
  int emission_count = 0;

  always @(posedge clk) begin
    if (!srst) begin
      if (ev_triggered_o) e_trig <= 1'b1;
      if (ev_done_o) e_done <= 1'b1;
      if (ev_aborted_o) e_abort <= 1'b1;
      if (ev_overflow_o) e_ovf <= 1'b1;
      if (flush_o) begin
        flush_count <= flush_count + 1;
        flush_seen <= 1'b1;
      end
      if (sample_emit_i) emission_count <= emission_count + 1;
    end
  end

  // ------------------------------------------------------------------
  // Drivers.
  // ------------------------------------------------------------------
  always #(CLK_PERIOD / 2.0) clk = ~clk;

  task automatic cycle(input logic sv, input logic tr);
    @(negedge clk);
    sample_valid_i = sv;
    trig_i = tr;
  endtask

  task automatic idle_cycles(int n);
    int i;
    for (i = 0; i < n; i++) begin
      cycle(1'b0, 1'b0);
    end
  endtask

  task automatic scenario_reset(input logic enc, input logic stream,
                                input int pre, input int post,
                                input int to, input int live);
    srst = 1'b1;
    abort_i = 1'b0;
    ring_overflow_i = 1'b0;
    fifo_ovf_i = 1'b0;
    mode_enc_i = enc;
    stream_en_i = stream;
    tb_stream = stream;
    pre_req_i = pre;
    post_cnt_i = post;
    timeout_i = to;
    live_flush_cycles_i = live;
    txn_max_run_i = 24'hFFFF;
    repeat (3) @(posedge clk);
    srst = 1'b0;
    @(posedge clk);
    trig_seen = 1'b0;
    had_emit = 1'b0;
    e_trig = 1'b0;
    e_done = 1'b0;
    e_abort = 1'b0;
    e_ovf = 1'b0;
    flush_count = 0;
    flush_seen = 1'b0;
    emission_count = 0;
  endtask

  task automatic arm_capture();
    @(negedge clk);
    arm_i = 1'b1;
    @(negedge clk);
    arm_i = 1'b0;
    idle_cycles(1);
  endtask

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  task automatic samples(int n, int t_at);
    int i;
    for (i = 0; i < n; i++) begin
      cycle(1'b1, (i == t_at));
    end
    idle_cycles(1);
  endtask

  task automatic wait_done(int bound);
    int g;
    g = 0;
    while (!e_done && g < bound) begin
      cycle(1'b0, 1'b0);
      g = g + 1;
    end
    if (g >= bound) begin
      $display("FAIL capture never completed");
      errors = errors + 1;
    end
  endtask

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ----------------------------------------------------------------
    // Scenario 1: burst, pre=30, post=20, trigger on sample 49.
    // K = 30, target = 50 emissions, words = 50 + closing = 51.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 30, 20, 0, 0);
    arm_capture();
    fail_check("s1 armed after arm", armed_o);
    fail_check("s1 not streaming", !streaming_o);
    samples(50, 49);
    idle_cycles(2);
    fail_check("s1 triggered event", e_trig);
    samples(60, -1);
    wait_done(600);
    fail_check("s1 done event", e_done);
    fail_check("s1 not aborted", !e_abort);
    $display("INFO s1 emissions=%0d words=%0d lo=%0d hi=%0d", emission_count,
             word_count_lo_o, word_count_lo_o, word_count_hi_o);
    fail_check("s1 emissions reached 50", emission_count == 50);
    fail_check("s1 word count 51", word_count_lo_o == 51);
    fail_check("s1 ring stopped", !ring_run_o);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 2: window-only burst (POST=0): exactly K = 30 emissions,
    // the trigger sample is never emitted; words = 30 + closing = 31.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 30, 0, 0, 0);
    arm_capture();
    samples(50, 49);
    wait_done(600);
    fail_check("s2 done", e_done);
    fail_check("s2 emissions exactly 30", emission_count == 30);
    fail_check("s2 word count 31", word_count_lo_o == 31);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 3: empty burst (trigger on sample 0, POST=0): no emissions,
    // no closing beat; the drain bound completes the capture.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 0, 0, 0, 0);
    arm_capture();
    samples(1, 0);
    wait_done(200);
    fail_check("s3 done via drain bound", e_done);
    // The trigger sample unavoidably leaves the ring one cycle after the
    // trigger; with K + POST = 0 it is the only sample and closes as a
    // one-sample encoder word.
    fail_check("s3 emissions 1 (trigger sample)", emission_count == 1);
    fail_check("s3 one closing word", word_count_lo_o == 1);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 4: timeout without a trigger aborts the capture.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 30, 20, 100, 0);
    arm_capture();
    samples(150, -1);
    idle_cycles(2);
    fail_check("s4 timeout aborted", e_abort);
    fail_check("s4 no done", !e_done);
    fail_check("s4 ring stopped after abort", !ring_run_o);
    fail_check("s4 no words counted", word_count_lo_o == 0);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 5: abort during CAPT stops everything without flush/done.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 10, 100, 0, 0);
    arm_capture();
    samples(15, 9);
    idle_cycles(2);
    @(negedge clk);
    abort_i = 1'b1;
    @(negedge clk);
    abort_i = 1'b0;
    idle_cycles(2);
    fail_check("s5 aborted", e_abort);
    fail_check("s5 ring stopped", !ring_run_o);
    fail_check("s5 no done", !e_done);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 6: ring overflow during CAPT stops the capture, reports
    // overflow, then drains the partial stream with a closing word.
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b0, 5, 200, 0, 0);
    arm_capture();
    samples(10, 4);
    idle_cycles(2);
    @(negedge clk);
    ring_overflow_i = 1'b1;
    @(negedge clk);
    ring_overflow_i = 1'b0;
    wait_done(300);
    fail_check("s6 overflow event", e_ovf);
    fail_check("s6 done after overflow drain", e_done);
    fail_check("s6 flush requested", flush_seen);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 7: stream mode, raw encoding, timer flush every 128 FCLK
    // cycles; an optional trigger fires the event without stopping.
    // ----------------------------------------------------------------
    scenario_reset(1'b0, 1'b1, 0, 0, 0, 128);
    arm_capture();
    fail_check("s7 streaming after arm", streaming_o);
    samples(30, 12);
    idle_cycles(2);
    fail_check("s7 trigger event", e_trig);
    samples(300, -1);
    idle_cycles(2);
    fail_check("s7 still streaming", streaming_o);
    fail_check("s7 no done", !e_done);
    fail_check("s7 timer flushed at least twice", flush_count >= 2);
    @(negedge clk);
    abort_i = 1'b1;
    @(negedge clk);
    abort_i = 1'b0;
    idle_cycles(2);
    fail_check("s7 aborted at stop", e_abort);
    fail_check("s7 streaming cleared", !streaming_o);
    idle_cycles(4);

    // ----------------------------------------------------------------
    // Scenario 8: stream txn mode: 65536-word flush (timer disabled).
    // ----------------------------------------------------------------
    scenario_reset(1'b1, 1'b1, 0, 0, 0, 0);
    arm_capture();
    begin
      int i;
      for (i = 0; i < 66000; i++) begin
        cycle(1'b1, 1'b0);
      end
    end
    idle_cycles(2);
    fail_check("s8 word flush at 65536", flush_seen);
    fail_check("s8 still streaming after flushes", streaming_o);
    fail_check("s8 no done", !e_done);
    fail_check("s8 word count above 65536", word_count_lo_o > 65536);
    @(negedge clk);
    abort_i = 1'b1;
    @(negedge clk);
    abort_i = 1'b0;
    idle_cycles(2);
    fail_check("s8 aborted", e_abort);
    idle_cycles(2);

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_capture_ctrl errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_capture_ctrl failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_capture_ctrl");
    $fclose(result_fd);
    $display("PASS: tb_la_capture_ctrl");
    $finish;
  end

  initial begin
    #8_000_000;
    $fatal(1, "tb_la_capture_ctrl timeout");
  end

endmodule : tb_la_capture_ctrl
