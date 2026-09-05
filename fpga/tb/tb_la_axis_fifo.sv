// Self-checking test for la_axis_fifo (P2.8).
//
// Checks: FIFO order and transport (data/tkeep/tlast) across full wrap;
// simultaneous write/read; read stalls (m_ready=0) that fill the FIFO and
// force s_ready low; non-compliant writes while full are dropped with the
// sticky overflow flag and the stream stays uncorrupted; overflow_clr and
// srst clear the flag; count tracks occupancy exactly.

`timescale 1ns/1ps

module tb_la_axis_fifo;
  localparam int D = 16;         // small depth for wrap in simulation time
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic s_valid = 1'b0;
  logic s_ready;
  logic [31:0] s_data = 32'd0;
  logic [3:0] s_tkeep = 4'hF;
  logic s_tlast = 1'b0;
  logic m_valid;
  logic m_ready = 1'b1;
  logic [31:0] m_data;
  logic [3:0] m_tkeep;
  logic m_tlast;
  logic full;
  logic overflow;
  logic overflow_clr = 1'b0;
  logic [4:0] count;

  la_axis_fifo #(
    .DEPTH(D),
    .LOG2D(4)
  ) dut (
    .clk(clk),
    .srst(srst),
    .s_valid(s_valid),
    .s_ready(s_ready),
    .s_data(s_data),
    .s_tkeep(s_tkeep),
    .s_tlast(s_tlast),
    .m_valid(m_valid),
    .m_ready(m_ready),
    .m_data(m_data),
    .m_tkeep(m_tkeep),
    .m_tlast(m_tlast),
    .full(full),
    .overflow(overflow),
    .overflow_clr(overflow_clr),
    .count(count)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  // Read side log (data, keep, last) for end-of-phase comparison.
  logic [31:0] rlog [0:2047];
  logic [3:0]  rkeep [0:2047];
  logic        rlast [0:2047];
  int rlog_n = 0;

  // Write side intent log.
  logic [31:0] wlog [0:2047];
  logic [3:0]  wkeep [0:2047];
  logic        wlast [0:2047];
  int wlog_n = 0;
  int wacc_n = 0;    // writes accepted by the FIFO

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  always @(posedge clk) begin
    if (!srst) begin
      if (s_valid && s_ready && wlog_n < 2048) begin
        wacc_n = wacc_n + 1;
      end
      if (m_valid && m_ready && rlog_n < 2048) begin
        rlog[rlog_n] = m_data;
        rkeep[rlog_n] = m_tkeep;
        rlast[rlog_n] = m_tlast;
        rlog_n = rlog_n + 1;
      end
    end
  end

  task automatic cycle(input logic wv, input logic [31:0] wd,
                       input logic [3:0] wk, input logic wl,
                       input logic rv);
    @(negedge clk);
    s_valid = wv;
    s_data = wd;
    s_tkeep = wk;
    s_tlast = wl;
    m_ready = rv;
    overflow_clr = 1'b0;
    if (wv && wlog_n < 2048) begin
      wlog[wlog_n] = wd;
      wkeep[wlog_n] = wk;
      wlast[wlog_n] = wl;
      wlog_n = wlog_n + 1;
    end
  endtask

  task automatic quiesce();
    @(negedge clk);
    s_valid = 1'b0;
    m_ready = 1'b0;
    overflow_clr = 1'b0;
  endtask

  task automatic drain();
    int guard;
    guard = 0;
    @(negedge clk);
    s_valid = 1'b0;
    m_ready = 1'b1;
    while (guard < 128) begin
      @(negedge clk);
      if (!m_valid) break;
      guard = guard + 1;
    end
    if (guard >= 128) begin
      $display("FAIL drain timeout m_valid=%b count=%0d", m_valid, count);
      errors = errors + 1;
    end
    m_ready = 1'b0;
  endtask

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  // Compare the read log against the first `n` accepted writes.
  task automatic check_order(string tag, int n);
    int i;
    if (rlog_n != n) begin
      $display("FAIL [%s] read %0d beats, expected %0d", tag, rlog_n, n);
      errors = errors + 1;
    end
    for (i = 0; i < n && i < rlog_n; i++) begin
      if (rlog[i] !== wlog[i]) begin
        $display("FAIL [%s] beat %0d data %08h expected %08h", tag, i,
                 rlog[i], wlog[i]);
        errors = errors + 1;
      end
      if (rkeep[i] !== wkeep[i] || rlast[i] !== wlast[i]) begin
        $display("FAIL [%s] beat %0d keep/last %b/%b expected %b/%b", tag, i,
                 rkeep[i], rlast[i], wkeep[i], wlast[i]);
        errors = errors + 1;
      end
    end
  endtask

  initial begin
    int i;
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ----------------------------------------------------------------
    // Phase 1: 60 beats written and read at 1:1 rate (read pointers wrap
    // several times over the depth of 16); transport fields carry through.
    // ----------------------------------------------------------------
    begin
      repeat (6) @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      rlog_n = 0;
      wlog_n = 0;
      wacc_n = 0;
      for (i = 0; i < 60; i++) begin
        cycle(1'b1, 32'h1000 + i, (i % 5 == 0) ? 4'h7 : 4'hF,
              (i == 59), 1'b1);
      end
      fail_check("never full at 1:1 rate", !full);
      quiesce();
      drain();
      check_order("wrap", 60);
      if (wacc_n != 60) begin
        $display("FAIL phase1 accepted %0d", wacc_n);
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Phase 2: read stall fills the FIFO; s_ready drops at full; one
    // non-compliant write is dropped and overflow becomes sticky; the
    // stream is uncorrupted afterwards.
    // ----------------------------------------------------------------
    begin
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      rlog_n = 0;
      wlog_n = 0;
      wacc_n = 0;
      // Fill with the reader stalled.
      for (i = 0; i < D + 3; i++) begin
        cycle(1'b1, 32'hA000 + i, 4'hF, 1'b0, 1'b0);
      end
      // At full, s_ready = 0; force a non-compliant beat.
      fail_check("fifo reports full", full);
      fail_check("s_ready low when full", !s_ready);
      quiesce();
      cycle(1'b1, 32'hDEAD, 4'hF, 1'b0, 1'b0);   // dropped: s_ready low
      quiesce();
      fail_check("overflow sticky after dropped beat", overflow);
      // Clear it, then drain and verify content integrity.
      @(negedge clk);
      overflow_clr = 1'b1;   // one-cycle clear
      @(negedge clk);
      overflow_clr = 1'b0;
      fail_check("overflow cleared", !overflow);
      drain();
      // Only the D compliant beats were stored.
      check_order("overflow", D);
      if (wacc_n != D) begin
        $display("FAIL phase2 accepted %0d expected %0d", wacc_n, D);
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Phase 3: count accuracy under simultaneous read/write and reset.
    // ----------------------------------------------------------------
    begin
      int writes;
      srst = 1'b1;
      @(posedge clk);
      srst = 1'b0;
      @(posedge clk);
      rlog_n = 0;
      wlog_n = 0;
      wacc_n = 0;
      for (i = 0; i < 40; i++) begin
        cycle(1'b1, 32'hB000 + i, 4'hF, (i == 39), 1'b1);   // 1:1 rate
      end
      quiesce();
      drain();
      check_order("one2one", 40);
      fail_check("count zero after drain", count == 0);
      // Write 5 with no reads, then read them all: the log tail must
      // match the last five writes.
      for (i = 0; i < 5; i++) begin
        cycle(1'b1, 32'hC000 + i, 4'h3, 1'b0, 1'b0);
      end
      @(negedge clk);      // let the final store commit
      fail_check("count is 5", count == 5);
      drain();
      for (i = 0; i < 5; i++) begin
        if (rlog[40 + i] !== wlog[40 + i]) begin
          $display("FAIL tail beat %0d data %08h expected %08h", i,
                   rlog[40 + i], wlog[40 + i]);
          errors = errors + 1;
        end
      end
      fail_check("count zero after tail drain", count == 0);
    end

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_axis_fifo errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_axis_fifo failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_axis_fifo");
    $fclose(result_fd);
    $display("PASS: tb_la_axis_fifo");
    $finish;
  end

  initial begin
    #2_000_000;
    $fatal(1, "tb_la_axis_fifo timeout");
  end

endmodule : tb_la_axis_fifo
