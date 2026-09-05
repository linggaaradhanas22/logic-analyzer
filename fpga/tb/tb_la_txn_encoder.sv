// Self-checking test for la_txn_encoder (P2.5).
//
// The scoreboard is event-driven: from the ACCEPTED sample stream and flush
// events it predicts the exact ordered list of output words (data + last)
// and every emission must match and pop it. Sample accounting is additionally
// verified: the sum of emitted runs equals the number of accepted samples,
// zero-length runs never occur, and m_last appears only on flush words.
//
// Timing model: values are read at negedges, where combinational outputs
// still equal the pre-posedge values, so each cycle observes the exact
// handshake events of the preceding posedge.
//
// Scenarios: alternating values at full rate; a long constant run split by
// max_run and closed by a capture-end flush; flush mid-run, a no-op flush
// while idle, and flush splitting an unchanged run (no false edge);
// m_ready stalls at a change boundary; flush requested while a word is
// waiting for m_ready (samples after the request must not straddle the
// boundary); max_run = 1; reset.

`timescale 1ns/1ps

module tb_la_txn_encoder;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic [23:0] max_run = 24'hFFFFFF;
  logic s_valid = 1'b0;
  logic s_ready;
  logic [7:0] s_data = 8'h00;
  logic flush_req = 1'b0;
  logic m_valid;
  logic m_ready = 1'b1;
  logic [31:0] m_data;
  logic m_last;

  la_txn_encoder dut (
    .clk(clk),
    .srst(srst),
    .max_run(max_run),
    .s_valid(s_valid),
    .s_ready(s_ready),
    .s_data(s_data),
    .flush_req(flush_req),
    .m_valid(m_valid),
    .m_ready(m_ready),
    .m_data(m_data),
    .m_last(m_last)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  // ------------------------------------------------------------------
  // Event model (clocked: reads pre-edge values, exactly what the DUT
  // evaluated at the same edge).
  // ------------------------------------------------------------------
  logic [31:0] q_data [0:15];
  logic        q_last [0:15];
  int q_w = 0;
  int q_r = 0;
  logic [23:0] m_run = '0;
  logic [7:0]  m_cur = '0;
  logic        m_cur_valid = 1'b0;
  integer      m_accepted = 0;
  integer      m_run_total = 0;
  integer      model_errors = 0;

  task automatic enqueue(input logic [31:0] w, input logic last);
    if (q_w - q_r >= 16) begin
      $display("FAIL model queue overflow");
      model_errors = model_errors + 1;
    end else begin
      q_data[q_w % 16] = w;
      q_last[q_w % 16] = last;
      q_w = q_w + 1;
    end
  endtask

  task automatic mcheck(string tag, logic cond, string extra);
    if (!cond) begin
      $display("FAIL [%0t] %s%s", $time, tag, extra);
      model_errors = model_errors + 1;
    end
  endtask

  always @(posedge clk) begin
    if (srst) begin
      q_w <= 0;
      q_r <= 0;
      m_cur_valid <= 1'b0;
      m_run <= '0;
      m_accepted <= 0;
      m_run_total <= 0;
    end else begin
      if (m_valid && m_ready) begin
        if (q_w == q_r) begin
          mcheck("word emitted with no prediction", 1'b0,
                 $sformatf(" word=%08h last=%b", m_data, m_last));
        end else begin
          mcheck("word data", m_data == q_data[q_r % 16],
                 $sformatf(" got %08h expected %08h", m_data, q_data[q_r % 16]));
          mcheck("word last", m_last == q_last[q_r % 16],
                 $sformatf(" got %b expected %b", m_last, q_last[q_r % 16]));
          mcheck("run is positive", m_data[31:8] != 24'd0, "");
          m_run_total <= m_run_total + m_data[31:8];
          q_r <= q_r + 1;
        end
      end
      if (flush_req) begin
        if (m_cur_valid) begin
          enqueue({m_run, m_cur}, 1'b1);
          m_cur_valid <= 1'b0;
          m_run <= '0;
        end
      end else if (s_valid && s_ready) begin
        m_accepted <= m_accepted + 1;
        if (!m_cur_valid) begin
          m_cur_valid <= 1'b1;
          m_cur <= s_data;
          m_run <= 24'd1;
        end else if (s_data == m_cur) begin
          if (m_run == max_run) begin
            enqueue({max_run, m_cur}, 1'b0);
            m_run <= 24'd1;
          end else begin
            m_run <= m_run + 24'd1;
          end
        end else begin
          enqueue({m_run, m_cur}, 1'b0);
          m_cur <= s_data;
          m_run <= 24'd1;
        end
      end
    end
  end

  // ------------------------------------------------------------------
  // Cycle-accurate driver (drive at negedge; combinational outputs are
  // read by the clocked model above at the posedge, so driving order
  // cannot skew acceptance).
  // ------------------------------------------------------------------
  always #(CLK_PERIOD / 2.0) clk = ~clk;

  task automatic cycle(input logic drive_valid, input logic [7:0] value,
                       input logic drive_flush, input logic drive_ready);
    @(negedge clk);
    m_ready = drive_ready;
    s_valid = drive_valid;
    s_data = value;
    flush_req = drive_flush;
  endtask

  task automatic drain_words();
    int guard;
    guard = 0;
    // Quiesce inputs immediately so no stale s_valid creates a duplicate.
    @(negedge clk);
    s_valid = 1'b0;
    flush_req = 1'b0;
    m_ready = 1'b1;
    while (guard < 64) begin
      @(negedge clk);
      if (!m_valid) break;
      guard = guard + 1;
    end
    if (guard >= 64) begin
      $display("FAIL encoder did not drain");
      errors = errors + 1;
    end
  endtask

  task automatic scenario_reset(input logic [23:0] mr);
    srst = 1'b1;
    max_run = mr;
    s_valid = 1'b0;
    flush_req = 1'b0;
    m_ready = 1'b1;
    repeat (3) @(posedge clk);
    srst = 1'b0;
    @(posedge clk);
    q_w = 0;
    q_r = 0;
    m_cur_valid = 1'b0;
    m_run = '0;
    m_accepted = 0;
    m_run_total = 0;
    model_errors = 0;
  endtask

  task automatic close_scenario(string tag);
    if (q_w != q_r) begin
      $display("FAIL [%s] %0d predicted words left over", tag, q_w - q_r);
      errors = errors + 1;
    end
    if (m_run_total != m_accepted) begin
      $display("FAIL [%s] run total %0d != accepted %0d", tag,
               m_run_total, m_accepted);
      errors = errors + 1;
    end
    if (model_errors != 0) begin
      errors = errors + 1;
    end
    model_errors = 0;
  endtask

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ----------------------------------------------------------------
    // Scenario 1: alternating values at full rate, closed by a flush.
    // 200 samples -> 199 change words + 1 close word = 200.
    // ----------------------------------------------------------------
    scenario_reset(24'hFFFF);
    begin
      int k;
      for (k = 0; k < 200; k++) begin
        cycle(1'b1, (k[0] ? 8'hAA : 8'h55), 1'b0, 1'b1);
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);   // capture-end flush
      drain_words();
      close_scenario("alternating");
    end

    // ----------------------------------------------------------------
    // Scenario 2: 60000 constant samples, max_run=100: 599 splits plus the
    // closing word of the flush = 600 words; run total 60000.
    // ----------------------------------------------------------------
    scenario_reset(24'd100);
    begin
      int k;
      for (k = 0; k < 60000; k++) begin
        cycle(1'b1, 8'h55, 1'b0, 1'b1);
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);
      drain_words();
      close_scenario("constant-run");
    end

    // ----------------------------------------------------------------
    // Scenario 3: flush mid-run (splits unchanged value), then idle no-op.
    // ----------------------------------------------------------------
    scenario_reset(24'hFFFF);
    begin
      int k;
      for (k = 0; k < 50; k++) begin
        cycle(1'b1, 8'hA5, (k == 37), 1'b1);   // flush while run == 37
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);          // close the final run
      cycle(1'b0, 8'h00, 1'b1, 1'b1);          // idle no-op flush
      drain_words();
      close_scenario("mid-run-flush");
    end

    // ----------------------------------------------------------------
    // Scenario 4: m_ready stall at a change boundary. A change to 0x03
    // arrives while {10,0x01} waits in the output slot; s_ready must drop
    // and the sample must wait until the slot frees.
    // ----------------------------------------------------------------
    scenario_reset(24'hFFFF);
    begin
      int k;
      logic stalled_seen;
      stalled_seen = 1'b0;
      for (k = 0; k < 10; k++) begin
        cycle(1'b1, 8'h01, 1'b0, 1'b1);
      end
      // Change to 0x02 with m_ready low: slot is empty, so the change word
      // is accepted and queued in the register.
      cycle(1'b1, 8'h02, 1'b0, 1'b0);
      // Another change (0x03) must now stall until m_ready returns.
      for (k = 0; k < 6; k++) begin
        cycle(1'b1, 8'h03, 1'b0, 1'b0);
        if (k < 3 && !s_ready) stalled_seen = 1'b1;
      end
      if (!stalled_seen) begin
        $display("FAIL s4 input did not stall while the slot was busy");
        errors = errors + 1;
      end
      // m_ready returns: the queued word pops and the pending change lands
      // in the same cycle; further 0x03 samples absorb.
      for (k = 0; k < 8; k++) begin
        cycle(1'b1, 8'h03, 1'b0, 1'b1);
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);
      drain_words();
      close_scenario("change-stall");
    end

    // ----------------------------------------------------------------
    // Scenario 5: flush while a word waits for m_ready. The queued word
    // leaves first and samples after the request restart the packet.
    // ----------------------------------------------------------------
    scenario_reset(24'hFFFF);
    begin
      int k;
      for (k = 0; k < 3; k++) begin
        cycle(1'b1, 8'h11, 1'b0, 1'b1);
      end
      cycle(1'b1, 8'h22, 1'b0, 1'b0);     // change {3,0x11} queued, slot busy
      cycle(1'b1, 8'h22, 1'b1, 1'b0);     // flush request behind the word
      cycle(1'b1, 8'h22, 1'b0, 1'b1);     // m_ready: word, then flush emit
      for (k = 0; k < 3; k++) begin
        cycle(1'b1, 8'h22, 1'b0, 1'b1);   // post-flush samples: new run
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);     // close the final run
      drain_words();
      close_scenario("flush-behind-word");
    end

    // ----------------------------------------------------------------
    // Scenario 6: max_run = 1, repeated and alternating values.
    // ----------------------------------------------------------------
    scenario_reset(24'd1);
    begin
      int k;
      for (k = 0; k < 5; k++) begin
        cycle(1'b1, 8'h77, 1'b0, 1'b1);
      end
      for (k = 0; k < 5; k++) begin
        cycle(1'b1, (k[0] ? 8'h01 : 8'h02), 1'b0, 1'b1);
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);
      drain_words();
      close_scenario("maxrun-1");
    end

    // ----------------------------------------------------------------
    // Scenario 7: reset.
    // ----------------------------------------------------------------
    scenario_reset(24'hFFFF);
    begin
      cycle(1'b1, 8'h42, 1'b0, 1'b1);
      cycle(1'b1, 8'h43, 1'b0, 1'b1);
      // Reset mid-capture while a change word is pending: outputs clear.
      srst = 1'b1;
      @(posedge clk);
      @(posedge clk);
      if (m_valid || !s_ready) begin
        $display("FAIL s7 reset state (m_valid=%b s_ready=%b)", m_valid, s_ready);
        errors = errors + 1;
      end
      srst = 1'b0;
    end

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_txn_encoder errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_txn_encoder failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_txn_encoder");
    $fclose(result_fd);
    $display("PASS: tb_la_txn_encoder");
    $finish;
  end

  initial begin
    #3_000_000;
    $fatal(1, "tb_la_txn_encoder timeout");
  end

endmodule : tb_la_txn_encoder
