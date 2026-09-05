// Self-checking test for la_input_sync (P2.1).
//
// Checks: reset clears all outputs; 2-cycle latency without the majority
// filter; 3-cycle latency with it; isolated single-cycle glitches are removed
// by the 2-of-3 filter but propagate through the plain synchronizer;
// two-cycle pulses pass the filter; all eight channels behave independently.
//
// Structure: clocked reference mirrors of both configurations are compared
// against the DUTs every cycle at negedge (post-posedge stable values).
// Explicit monitors additionally assert the semantic latency and filter
// behavior with named checks.

`timescale 1ns/1ps

module tb_la_input_sync;
  localparam int CHANNELS = 8;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic [CHANNELS-1:0] ch_in = '0;

  logic [CHANNELS-1:0] sync2;    // MAJORITY=0
  logic [CHANNELS-1:0] sync3;    // MAJORITY=1

  la_input_sync #(
    .CHANNELS(CHANNELS),
    .MAJORITY(1'b0)
  ) dut_plain (
    .clk(clk),
    .srst(srst),
    .ch_in(ch_in),
    .ch_sync(sync2)
  );

  la_input_sync #(
    .CHANNELS(CHANNELS),
    .MAJORITY(1'b1)
  ) dut_filtered (
    .clk(clk),
    .srst(srst),
    .ch_in(ch_in),
    .ch_sync(sync3)
  );

  // Clocked reference models (2-cycle and 3-cycle structural mirrors).
  logic [CHANNELS-1:0] ref_s1 = '0;
  logic [CHANNELS-1:0] ref_s2 = '0;
  logic [CHANNELS-1:0] ref_t1 = '0;
  logic [CHANNELS-1:0] ref_t2 = '0;
  logic [CHANNELS-1:0] ref2;
  logic [CHANNELS-1:0] ref3;

  always_ff @(posedge clk) begin
    if (srst) begin
      ref_s1 <= '0;
      ref_s2 <= '0;
      ref_t1 <= '0;
      ref_t2 <= '0;
    end else begin
      ref_s1 <= ch_in;
      ref_s2 <= ref_s1;
      ref_t1 <= ref_s2;
      ref_t2 <= ref_t1;
    end
  end

  assign ref2 = ref_s2;
  assign ref3 = (ref_s2 & ref_t1) | (ref_t1 & ref_t2) | (ref_s2 & ref_t2);

  integer errors = 0;
  integer result_fd;
  string result_path;

  // Monitor state for the semantic checks.
  logic monitor_armed = 1'b0;
  logic [CHANNELS-1:0] glitch_mask = '0;
  logic [CHANNELS-1:0] glitch_sync2 = '0;
  logic [CHANNELS-1:0] glitch_sync3 = '0;
  logic two_cycle_sync2_seen = 1'b0;
  logic two_cycle_sync3_seen = 1'b0;
  logic two_cycle_monitor = 1'b0;

  always @(posedge clk) begin
    if (monitor_armed) begin
      glitch_sync2 <= glitch_sync2 | (sync2 & glitch_mask);
      glitch_sync3 <= glitch_sync3 | (sync3 & glitch_mask);
    end
    if (two_cycle_monitor) begin
      if (sync2 == 8'hA5) two_cycle_sync2_seen <= 1'b1;
      if (sync3 == 8'hA5) two_cycle_sync3_seen <= 1'b1;
    end
  end

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  task automatic compare_cycle();
    if (monitor_armed && !srst) begin
      if (sync2 !== ref2) begin
        $display("FAIL [%0t] sync2=%h ref2=%h", $time, sync2, ref2);
        errors = errors + 1;
      end
      if (sync3 !== ref3) begin
        $display("FAIL [%0t] sync3=%h ref3=%h", $time, sync3, ref3);
        errors = errors + 1;
      end
    end
  endtask

  // Deterministic stimulus source.
  function automatic logic [CHANNELS-1:0] next_pattern(input logic [31:0] seed);
    logic [31:0] x;
    x = seed * 32'h9E3779B1 + 32'h1234567;
    next_pattern = {x[0], x[3], x[5], x[7], x[11], x[13], x[17], x[19]};
  endfunction

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // Reset phase: drive activity, expect outputs cleared.
    ch_in = 8'hFF;
    repeat (6) @(posedge clk);
    fail_check("reset clears plain synchronizer", sync2 == 8'h00);
    fail_check("reset clears filtered synchronizer", sync3 == 8'h00);
    srst = 1'b0;

    // Latency checks: a step at ch_in appears after exactly 2 (plain) and
    // 3 (filtered) FCLK cycles.
    begin
      int ticks;
      ch_in = 8'h01;
      for (ticks = 0; ticks < 8; ticks++) begin
        @(negedge clk);
        case (ticks)
          0: begin
            fail_check("quiet before plain latency", sync2 == 8'h00);
            fail_check("quiet before filtered latency", sync3 == 8'h00);
          end
          1: begin
            fail_check("no output at cycle 1", sync2 == 8'h00 && sync3 == 8'h00);
          end
          2: begin
            fail_check("plain latency is 2 cycles", sync2 == 8'h01);
            fail_check("filtered quiet at cycle 2", sync3 == 8'h00);
          end
          3: begin
            fail_check("plain still stable", sync2 == 8'h01);
            fail_check("filtered latency is 3 cycles", sync3 == 8'h01);
          end
          default: begin
            fail_check("outputs hold level", sync2 == 8'h01 && sync3 == 8'h01);
          end
        endcase
        @(posedge clk);
      end
    end

    // Traffic phase: random-ish independent per-channel activity.
    begin
      logic [31:0] state;
      int cycle;
      state = 32'hCAFEBABE;
      monitor_armed = 1'b1;
      for (cycle = 0; cycle < 600; cycle++) begin
        logic [CHANNELS-1:0] applied;
        @(negedge clk);
        compare_cycle();
        applied = next_pattern(state);
        state = state * 32'h1E85 + 32'h5B1;
        ch_in = applied;
        @(posedge clk);
      end
    end

    // Quiet, then one-cycle glitch per channel.
    ch_in = 8'h00;
    repeat (10) @(posedge clk);
    begin
      int k;
      for (k = 0; k < 8; k++) begin
        logic [7:0] mask;
        mask = 8'h01 << k;
        glitch_mask = mask;
        glitch_sync2 = '0;
        glitch_sync3 = '0;
        @(negedge clk);
        ch_in = mask;             // one full FCLK cycle pulse
        @(negedge clk);
        ch_in = 8'h00;
        repeat (8) @(posedge clk);
        repeat (2) @(negedge clk);
        fail_check($sformatf("glitch ch%0d reached plain path", k),
                   glitch_sync2 == mask);
        fail_check($sformatf("glitch ch%0d removed by majority path", k),
                   glitch_sync3 == 8'h00);
      end
    end

    // Two-cycle pulse on all channels: passes both paths.
    ch_in = 8'h00;
    repeat (10) @(posedge clk);
    two_cycle_sync2_seen = 1'b0;
    two_cycle_sync3_seen = 1'b0;
    two_cycle_monitor = 1'b1;
    @(negedge clk);
    ch_in = 8'hA5;              // held across two FCLK cycles
    @(negedge clk);
    @(negedge clk);
    ch_in = 8'h00;
    repeat (10) @(posedge clk);
    repeat (2) @(negedge clk);
    two_cycle_monitor = 1'b0;
    fail_check("two-cycle pulse reaches plain path", two_cycle_sync2_seen);
    fail_check("two-cycle pulse passes majority filter", two_cycle_sync3_seen);

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_input_sync errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_input_sync failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_input_sync");
    $fclose(result_fd);
    $display("PASS: tb_la_input_sync");
    $finish;
  end

  initial begin
    #2_000_000;
    $fatal(1, "tb_la_input_sync timeout");
  end

endmodule : tb_la_input_sync
