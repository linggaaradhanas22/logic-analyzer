// Self-checking test for la_trigger (P2.3).
//
// Table-driven scenarios verify the P1 register semantics:
//   level match on TRIG_MASK/TRIG_VALUE channels;
//   "empty edge mask = true" for RISE and FALL;
//   rise/fall detection between consecutive VALID samples (divider tolerant);
//   no spurious edges on the first sample after reset;
//   level-only trigger may fire on the first sample;
//   repeated level hits re-trigger on every qualifying valid sample.
//
// A structural reference model mirrors the spec in the testbench; every
// expected trigger pulse is checked against the mirror and named scenario
// assertions confirm specific semantics.

`timescale 1ns/1ps

module tb_la_trigger;
  localparam int CHANNELS = 8;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic sample_valid = 1'b0;
  logic [CHANNELS-1:0] sample = '0;
  logic [CHANNELS-1:0] trig_mask = '0;
  logic [CHANNELS-1:0] trig_value = '0;
  logic [CHANNELS-1:0] trig_rise = '0;
  logic [CHANNELS-1:0] trig_fall = '0;
  logic trig;

  la_trigger #(
    .CHANNELS(CHANNELS)
  ) dut (
    .clk(clk),
    .srst(srst),
    .sample_valid(sample_valid),
    .sample(sample),
    .trig_mask(trig_mask),
    .trig_value(trig_value),
    .trig_rise(trig_rise),
    .trig_fall(trig_fall),
    .trig(trig)
  );

  // Structural reference mirror of the spec (module-level assigns, matching
  // the RTL structure).
  logic [CHANNELS-1:0] ref_prev = '0;
  logic ref_prev_valid = 1'b0;
  logic ref_level_ok;
  logic ref_rise_ok;
  logic ref_fall_ok;
  logic ref_edge_required;
  logic ref_trig;

  always_ff @(posedge clk) begin
    if (srst) begin
      ref_prev <= '0;
      ref_prev_valid <= 1'b0;
    end else if (sample_valid) begin
      ref_prev <= sample;
      ref_prev_valid <= 1'b1;
    end
  end

  assign ref_level_ok = ((sample ^ trig_value) & trig_mask) == '0;
  assign ref_rise_ok = (trig_rise == '0) ? 1'b1
                     : (|(sample & ~ref_prev & trig_rise));
  assign ref_fall_ok = (trig_fall == '0) ? 1'b1
                     : (|(~sample & ref_prev & trig_fall));
  assign ref_edge_required = (trig_rise != '0) || (trig_fall != '0);
  assign ref_trig = sample_valid && ref_level_ok
                  && (!ref_edge_required
                      || (ref_prev_valid && ref_rise_ok && ref_fall_ok));

  integer errors = 0;
  integer result_fd;
  string result_path;
  int mismatch_count = 0;

  // Every-cycle comparison of trig against the mirror.
  always @(posedge clk) begin
    if (!srst && trig !== ref_trig) begin
      if (mismatch_count < 10)
        $display("FAIL [%0t] trig=%b ref=%b sample=%02h mask=%02h value=%02h rise=%02h fall=%02h",
                 $time, trig, ref_trig, sample, trig_mask, trig_value,
                 trig_rise, trig_fall);
      mismatch_count = mismatch_count + 1;
    end
  end

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  task automatic apply_sample(input logic [7:0] value, input int gap);
    int g;
    for (g = 0; g < gap; g++) begin
      @(negedge clk);
      sample_valid = 1'b0;
    end
    @(negedge clk);
    sample = value;
    sample_valid = 1'b1;
  endtask

  task automatic pause_samples(int gap);
    int g;
    for (g = 0; g < gap; g++) begin
      @(negedge clk);
      sample_valid = 1'b0;
    end
  endtask

  task automatic reset_dut();
    srst = 1'b1;
    sample_valid = 1'b0;
    trig_mask = '0;
    trig_value = '0;
    trig_rise = '0;
    trig_fall = '0;
    repeat (4) @(posedge clk);
    srst = 1'b0;
    @(posedge clk);
  endtask

  // Named pulse capture helper: returns true if trig pulsed during the
  // next `win` cycles after the pending sample edge.
  task automatic expect_pulse(input string tag, input logic should);
    logic saw;
    int i;
    saw = 1'b0;
    for (i = 0; i < 4; i++) begin
      @(posedge clk);
      if (trig) saw = 1'b1;
    end
    fail_check(tag, saw == should);
  endtask

  initial begin
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ---- Scenario 1: level-only trigger (both edge masks empty). ----
    reset_dut();
    trig_mask = 8'hFF;
    trig_value = 8'hAA;
    begin
      int i;
      for (i = 0; i < 6; i++) begin
        sample = (i % 2 == 0) ? 8'hAA : 8'h55;
        @(negedge clk);
        sample_valid = 1'b1;
        expect_pulse($sformatf("s1 level match sample %02h", sample),
                     sample == 8'hAA);
        pause_samples(2);
      end
    end

    // ---- Scenario 2: level + rise only; empty fall mask is true. ----
    // Level on ch[1:0] == 1, rise required on ch4. ch4 toggling does not
    // disturb the level field.
    reset_dut();
    trig_mask = 8'h03;
    trig_value = 8'h01;
    trig_rise = 8'h10;    // ch4 rising required
    trig_fall = 8'h00;    // empty: unconditional
    sample = 8'h01;       // level ok, no rise (prev invalid yet)
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s2 first sample cannot fire with edge mask", 1'b0);
    pause_samples(2);
    sample = 8'h11;       // rise on ch4, level still ok
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s2 rising edge fires", 1'b1);
    pause_samples(2);
    sample = 8'h01;       // ch4 falls; rise term now false
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s2 falling edge does not satisfy rise", 1'b0);
    pause_samples(2);
    sample = 8'h11;       // rise again while holding level
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s2 second rise fires", 1'b1);
    pause_samples(2);

    // ---- Scenario 3: rise AND fall on different channels. ----
    // Level == 0x01 on all channels; fire requires ch0 rising and ch7
    // falling on the same sample: the only transition is 0x80 -> 0x01.
    reset_dut();
    trig_mask = 8'hFF;
    trig_value = 8'h01;
    trig_rise = 8'h01;    // ch0 rising
    trig_fall = 8'h80;    // ch7 falling
    sample = 8'h01;       // prev invalid: no fire
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3a prev invalid blocks", 1'b0);
    pause_samples(2);
    sample = 8'h80;       // level fails; edges are rise/fall of ch7/ch0
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3b level fails", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x80: ch0 rises, ch7 falls, level ok: FIRE
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3c simultaneous rise+fall fires", 1'b1);
    pause_samples(2);
    sample = 8'h80;       // from 0x01: ch0 falls, ch7 rises: no terms
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3d opposite transition inert", 1'b0);
    pause_samples(2);
    sample = 8'h81;       // from 0x80: ch0 rises only; level fails
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3e partial transition level fails", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x81: no rise (ch0 held), fall ch7 only
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3f fall alone does not fire", 1'b0);
    pause_samples(2);
    sample = 8'h80;       // reset state for retrigger
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3g setup step", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x80: fires again (retrigger)
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s3h repeated fire", 1'b1);
    pause_samples(2);

    // ---- Scenario 4: divider tolerance (gaps between valid samples). ----
    reset_dut();
    trig_mask = 8'hFF;
    trig_value = 8'h10;   // level: only ch4 high
    trig_rise = 8'h10;    // ch4
    trig_fall = 8'h00;
    sample = 8'h10;       // first sample after reset: edge state undefined
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s4 first sample no edge", 1'b0);
    pause_samples(20);
    sample = 8'h00;       // ch4 falls; level fails
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s4 fall ignored", 1'b0);
    pause_samples(17);
    sample = 8'h10;       // rise after a long divider gap still detected
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s4 rise after gap fires", 1'b1);
    pause_samples(2);

    // ---- Scenario 5: channel masks keep non-participating bits inert. ----
    reset_dut();
    trig_mask = 8'h03;
    trig_value = 8'h02;
    trig_rise = 8'h00;
    trig_fall = 8'h00;
    sample = 8'hFF;       // bits 1:0 = 11 != 10 -> no trigger
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s5 level mismatch ignored others", 1'b0);
    pause_samples(2);
    sample = 8'h5A;       // low bits 10 -> match regardless of upper bits
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s5 level match fires", 1'b1);
    pause_samples(2);

    // ---- Scenario 6: rise ch0 AND fall ch1, level == 0x01. ----
    // Fires only on 0x02 -> 0x01 transitions.
    reset_dut();
    trig_mask = 8'hFF;
    trig_value = 8'h01;
    trig_rise = 8'h01;
    trig_fall = 8'h02;
    sample = 8'h01;       // prev invalid
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6a prev invalid", 1'b0);
    pause_samples(2);
    sample = 8'h02;       // ch1 rises (not falls); level fails
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6b no qualifying edge", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x02: ch0 rises, ch1 falls, level ok: FIRE
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6c simultaneous edges fire", 1'b1);
    pause_samples(2);
    sample = 8'h02;       // from 0x01: ch0 falls, ch1 rises: no terms
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6d opposite transition inert", 1'b0);
    pause_samples(2);
    sample = 8'h03;       // level fails
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6e level fails", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x03: fall ch1 ok? ch1 1->1 no; ch0 held
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6f no simultaneous edge", 1'b0);
    pause_samples(2);
    sample = 8'h02;       // from 0x01
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6g setup", 1'b0);
    pause_samples(2);
    sample = 8'h01;       // from 0x02: fires again
    @(negedge clk);
    sample_valid = 1'b1;
    expect_pulse("s6h repeated fire", 1'b1);
    pause_samples(2);

    if (mismatch_count != 0) begin
      $display("FAIL total mirror mismatches=%0d", mismatch_count);
      errors = errors + 1;
    end
    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_trigger errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_trigger failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_trigger");
    $fclose(result_fd);
    $display("PASS: tb_la_trigger");
    $finish;
  end

  initial begin
    #2_000_000;
    $fatal(1, "tb_la_trigger timeout");
  end

endmodule : tb_la_trigger
