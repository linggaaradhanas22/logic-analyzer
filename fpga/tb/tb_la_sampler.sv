// Self-checking test for la_sampler (P2.2).
//
// Checks: sample period is sample_div+1 FCLK cycles for DIV in {0,1,9,99};
// DIV=0 samples every cycle; sample is the chan_en-masked channel value at
// the tick; sample_valid is a one-cycle pulse aligned with sample; reset
// clears the outputs; chan_en changes mask the stream.
//
// A clocked reference model mirrors the divider rule exactly; samples are
// compared on every sample_valid pulse.

`timescale 1ns/1ps

module tb_la_sampler;
  localparam int CHANNELS = 8;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic [CHANNELS-1:0] ch_sync = '0;
  logic [31:0] sample_div = 32'd0;
  logic [CHANNELS-1:0] chan_en = 8'hFF;

  logic [7:0] sample;
  logic sample_valid;

  la_sampler #(
    .CHANNELS(CHANNELS)
  ) dut (
    .clk(clk),
    .srst(srst),
    .ch_sync(ch_sync),
    .sample_div(sample_div),
    .chan_en(chan_en),
    .sample(sample),
    .sample_valid(sample_valid)
  );

  // Clocked reference mirror of the divider rule.
  logic [31:0] ref_div = '0;
  logic [7:0] ref_sample = '0;
  logic ref_tick;

  always_ff @(posedge clk) begin
    if (srst) begin
      ref_div <= '0;
    end else begin
      if (ref_div == 32'd0) begin
        ref_div <= sample_div;
      end else begin
        ref_div <= ref_div - 32'd1;
      end
    end
  end

  assign ref_tick = (sample_div == 32'd0) ? 1'b1 : (ref_div == 32'd0);

  always_ff @(posedge clk) begin
    if (srst) begin
      ref_sample <= '0;
    end else if (ref_tick) begin
      ref_sample <= ch_sync & chan_en;
    end
  end

  integer errors = 0;
  integer result_fd;
  string result_path;
  logic check_en = 1'b0;

  // Counters for period verification.
  int last_tick_cycle = 0;
  int cycle_count = 0;
  int tick_count = 0;
  int period_errors = 0;

  always @(posedge clk) begin
    if (check_en) begin
      if (sample_valid) begin
        int since;
        since = cycle_count - last_tick_cycle;
        if (tick_count > 0 && since != $signed(sample_div) + 1) begin
          $display("FAIL [%0t] period %0d cycles expected %0d",
                   $time, since, $signed(sample_div) + 1);
          period_errors = period_errors + 1;
        end
        last_tick_cycle = cycle_count;
        tick_count = tick_count + 1;
      end
      cycle_count = cycle_count + 1;
    end
  end

  always @(posedge clk) begin
    if (check_en) begin
      if (sample !== ref_sample) begin
        $display("FAIL [%0t] sample=%02h ref=%02h", $time, sample, ref_sample);
        errors = errors + 1;
      end
    end
  end

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  // Reset the sampler and mirror together, then arm the next divider.
  // The first pulse after reset is not period-checked (tick_count == 0).
  task automatic phase_reset(input logic [31:0] div, input logic [7:0] mask);
    srst = 1'b1;
    repeat (2) @(posedge clk);
    sample_div = div;
    chan_en = mask;
    ch_sync = 8'h00;
    tick_count = 0;
    period_errors = 0;
    srst = 1'b0;
    @(posedge clk);
    wait_pulses(1);      // consume the first clean pulse of this phase
  endtask

  task automatic wait_pulses(int n);
    int seen;
    seen = 0;
    while (seen < n) begin
      @(posedge clk);
      if (sample_valid) seen = seen + 1;
    end
  endtask

  // Deterministic channel stimulus.
  function automatic logic [7:0] next_pattern(input logic [31:0] seed);
    logic [31:0] x;
    x = seed * 32'h9E3779B1 + 32'h1234567;
    next_pattern = {x[1], x[2], x[5], x[6], x[9], x[12], x[19], x[23]};
  endfunction

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    ch_sync = 8'h00;
    repeat (8) @(posedge clk);
    srst = 1'b0;
    repeat (4) @(posedge clk);
    check_en = 1'b1;
    wait_pulses(1);      // align DUT and mirror from the same post-reset state

    // Phase A: DIV=0 -> sample every cycle, all channels.
    begin
      logic [31:0] state;
      int i;
      int pulses_before;
      phase_reset(32'd0, 8'hFF);
      pulses_before = tick_count;
      state = 32'h13579BDF;
      for (i = 0; i < 200; i++) begin
        logic [7:0] v;
        v = next_pattern(state);
        state = state * 32'h1E85 + 32'h5B1;
        @(negedge clk);
        ch_sync = v;
        @(posedge clk);
      end
      if (period_errors != 0) begin
        $display("FAIL phase A period errors=%0d", period_errors);
        errors = errors + 1;
      end
      if (tick_count - pulses_before != 200) begin
        $display("FAIL phase A pulse count %0d expected 200",
                 tick_count - pulses_before);
        errors = errors + 1;
      end
    end

    // Phase B: DIV=1 -> every 2 cycles, channel masking.
    begin
      logic [31:0] state;
      int i;
      int pulses_before;
      phase_reset(32'd1, 8'h55);
      pulses_before = tick_count;
      state = 32'h2468ACE0;
      for (i = 0; i < 240; i++) begin
        logic [7:0] v;
        v = next_pattern(state);
        state = state * 32'h1E85 + 32'h5B1;
        @(negedge clk);
        ch_sync = v;
        @(posedge clk);
      end
      if (period_errors != 0) begin
        $display("FAIL phase B period errors=%0d", period_errors);
        errors = errors + 1;
      end
      if (tick_count - pulses_before < 100 || tick_count - pulses_before > 140) begin
        $display("FAIL phase B pulse count %0d", tick_count - pulses_before);
        errors = errors + 1;
      end
    end

    // Phase C: DIV=9 -> every 10 cycles, all channels on.
    begin
      logic [31:0] state;
      int i;
      int pulses_before;
      phase_reset(32'd9, 8'hFF);
      pulses_before = tick_count;
      state = 32'h0DEADF00;
      for (i = 0; i < 300; i++) begin
        logic [7:0] v;
        v = next_pattern(state);
        state = state * 32'h1E85 + 32'h5B1;
        @(negedge clk);
        ch_sync = v;
        @(posedge clk);
      end
      if (period_errors != 0) begin
        $display("FAIL phase C period errors=%0d", period_errors);
        errors = errors + 1;
      end
      if (tick_count - pulses_before < 28 || tick_count - pulses_before > 31) begin
        $display("FAIL phase C pulse count %0d expected ~29-30",
                 tick_count - pulses_before);
        errors = errors + 1;
      end
    end

    // Phase D: DIV=99 -> every 100 cycles, channel mask; then reset checks.
    begin
      logic [31:0] state;
      int i;
      int pulses_before;
      phase_reset(32'd99, 8'hF0);
      pulses_before = tick_count;
      state = 32'hCAFEF00D;
      for (i = 0; i < 400; i++) begin
        logic [7:0] v;
        v = next_pattern(state);
        state = state * 32'h1E85 + 32'h5B1;
        @(negedge clk);
        ch_sync = v;
        @(posedge clk);
      end
      if (period_errors != 0) begin
        $display("FAIL phase D period errors=%0d", period_errors);
        errors = errors + 1;
      end
      if (tick_count - pulses_before < 2 || tick_count - pulses_before > 5) begin
        $display("FAIL phase D pulse count %0d expected ~3",
                 tick_count - pulses_before);
        errors = errors + 1;
      end
      // Reset while running: sample_valid must stop and sample must clear.
      srst = 1'b1;
      @(posedge clk);
      @(posedge clk);
      if (sample_valid || sample !== 8'h00) begin
        $display("FAIL reset does not clear sampler outputs");
        errors = errors + 1;
      end
      srst = 1'b0;
      repeat (3) @(posedge clk);
      check_en = 1'b0;
    end

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_sampler errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_sampler failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_sampler");
    $fclose(result_fd);
    $display("PASS: tb_la_sampler");
    $finish;
  end

  initial begin
    #2_000_000;
    $fatal(1, "tb_la_sampler timeout");
  end

endmodule : tb_la_sampler
