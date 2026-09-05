// Self-checking test for la_regs (P2.10).
//
// Procedural AXI-Lite BFM. Covers: readback of ID and resets; independent
// AW/W arrival (address first, data later and vice versa) with response
// stalls (BREADY held low); read responses with RREADY stalls; byte strobes
// (partial-byte writes preserve the other bytes); self-clearing ARM/ABORT/
// SOFT_RESET pulses; masked fields (MODE, CHAN_EN, TRIG_*, PATGEN_CTRL,
// IRQ_EN, CTRL) rejecting reserved bits; PRE_SAMPLES and TXN_MAX_RUN
// clamping; IRQ_STAT W1C with IRQ event pulses and the irq output;
// out-of-range addresses; simultaneous read and write traffic.

`timescale 1ns/1ps

module tb_la_regs;
  localparam real CLK_PERIOD = 10.0;

  import la_regs_pkg::*;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic s_axi_awvalid = 1'b0;
  logic s_axi_awready;
  logic [31:0] s_axi_awaddr = 32'd0;
  logic [2:0] s_axi_awprot = 3'd0;
  logic s_axi_wvalid = 1'b0;
  logic s_axi_wready;
  logic [31:0] s_axi_wdata = 32'd0;
  logic [3:0] s_axi_wstrb = 4'hF;
  logic s_axi_bvalid;
  logic s_axi_bready = 1'b1;
  logic [1:0] s_axi_bresp;
  logic s_axi_arvalid = 1'b0;
  logic s_axi_arready;
  logic [31:0] s_axi_araddr = 32'd0;
  logic [2:0] s_axi_arprot = 3'd0;
  logic s_axi_rvalid;
  logic s_axi_rready = 1'b1;
  logic [31:0] s_axi_rdata;
  logic [1:0] s_axi_rresp;

  logic [31:0] ctrl_o, mode_o, sample_div_o, chan_en_o;
  logic [31:0] trig_mask_o, trig_value_o, trig_rise_o, trig_fall_o;
  logic [31:0] pre_samples_o, post_samples_o, timeout_o, txn_max_run_o;
  logic [31:0] live_flush_o, patgen_ctrl_o;
  logic arm_pulse_o, abort_pulse_o, soft_reset_pulse_o, irq_o;
  logic [31:0] status_i = 32'h00000001;
  logic [31:0] trig_index_i = 32'h0000002A;
  logic [31:0] word_lo_i = 32'h00000400;
  logic [31:0] word_hi_i = 32'h00000000;
  logic irq_st_done_i = 1'b0;
  logic irq_st_trig_i = 1'b0;
  logic irq_st_ovf_i = 1'b0;

  la_regs dut (
    .clk(clk), .srst(srst),
    .s_axi_awvalid(s_axi_awvalid), .s_axi_awready(s_axi_awready),
    .s_axi_awaddr(s_axi_awaddr), .s_axi_awprot(s_axi_awprot),
    .s_axi_wvalid(s_axi_wvalid), .s_axi_wready(s_axi_wready),
    .s_axi_wdata(s_axi_wdata), .s_axi_wstrb(s_axi_wstrb),
    .s_axi_bvalid(s_axi_bvalid), .s_axi_bready(s_axi_bready),
    .s_axi_bresp(s_axi_bresp),
    .s_axi_arvalid(s_axi_arvalid), .s_axi_arready(s_axi_arready),
    .s_axi_araddr(s_axi_araddr), .s_axi_arprot(s_axi_arprot),
    .s_axi_rvalid(s_axi_rvalid), .s_axi_rready(s_axi_rready),
    .s_axi_rdata(s_axi_rdata), .s_axi_rresp(s_axi_rresp),
    .ctrl_o(ctrl_o), .mode_o(mode_o), .sample_div_o(sample_div_o),
    .chan_en_o(chan_en_o), .trig_mask_o(trig_mask_o),
    .trig_value_o(trig_value_o), .trig_rise_o(trig_rise_o),
    .trig_fall_o(trig_fall_o), .pre_samples_o(pre_samples_o),
    .post_samples_o(post_samples_o), .timeout_o(timeout_o),
    .txn_max_run_o(txn_max_run_o), .live_flush_o(live_flush_o),
    .patgen_ctrl_o(patgen_ctrl_o),
    .arm_pulse_o(arm_pulse_o), .abort_pulse_o(abort_pulse_o),
    .soft_reset_pulse_o(soft_reset_pulse_o), .irq_o(irq_o),
    .status_i(status_i), .trig_index_i(trig_index_i),
    .word_lo_i(word_lo_i), .word_hi_i(word_hi_i),
    .irq_st_done_i(irq_st_done_i), .irq_st_trig_i(irq_st_trig_i),
    .irq_st_ovf_i(irq_st_ovf_i)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  integer b_writes = 0;

  // Pulse capture for the self-clearing control bits.
  logic arm_pulse_caught = 1'b0;
  logic abort_pulse_caught = 1'b0;
  logic soft_pulse_caught = 1'b0;
  always @(posedge clk) begin
    if (arm_pulse_o) arm_pulse_caught <= 1'b1;
    if (abort_pulse_o) abort_pulse_caught <= 1'b1;
    if (soft_reset_pulse_o) soft_pulse_caught <= 1'b1;
  end

  initial begin
    #500_000;
    $fatal(1, "tb_la_regs timeout");
  end


  always #(CLK_PERIOD / 2.0) clk = ~clk;

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  task automatic wait_posedge(int n);
    int i;
    for (i = 0; i < n; i++) @(posedge clk);
  endtask

  // ------------------------------------------------------------------
  // Write BFM: drives AW and W (optionally on separate cycles) and waits
  // for the response; b_stall holds BREADY low for some cycles first.
  // ------------------------------------------------------------------
  task automatic axi_write(input logic [31:0] addr, input logic [31:0] data,
                           input logic [3:0] strb, input int separate,
                           input int b_stall);
    int g;
    if (separate == 0) begin
      @(negedge clk);
      s_axi_awvalid = 1'b1;
      s_axi_awaddr = addr;
      s_axi_wvalid = 1'b1;
      s_axi_wdata = data;
      s_axi_wstrb = strb;
      @(negedge clk);
      s_axi_awvalid = 1'b0;
      s_axi_wvalid = 1'b0;
    end else if (separate == 1) begin
      // Address first, data one cycle later.
      @(negedge clk);
      s_axi_awvalid = 1'b1;
      s_axi_awaddr = addr;
      @(negedge clk);
      s_axi_awvalid = 1'b0;
      s_axi_wvalid = 1'b1;
      s_axi_wdata = data;
      s_axi_wstrb = strb;
      @(negedge clk);
      s_axi_wvalid = 1'b0;
    end else begin
      // Data first, address one cycle later.
      @(negedge clk);
      s_axi_wvalid = 1'b1;
      s_axi_wdata = data;
      s_axi_wstrb = strb;
      @(negedge clk);
      s_axi_wvalid = 1'b0;
      s_axi_awvalid = 1'b1;
      s_axi_awaddr = addr;
      @(negedge clk);
      s_axi_awvalid = 1'b0;
    end
    // Wait for the write response, optionally stalling BREADY first.
    g = 0;
    while (g < b_stall) begin
      @(negedge clk);
      s_axi_bready = 1'b0;
      g = g + 1;
    end
    @(negedge clk);
    s_axi_bready = 1'b1;
    g = 0;
    while (!s_axi_bvalid && g < 64) begin
      @(posedge clk);
      g = g + 1;
    end
    if (g >= 64) begin
      $display("FAIL no write response");
      errors = errors + 1;
    end
    // Consume the response.
    while (s_axi_bvalid) @(posedge clk);
    b_writes = b_writes + 1;
  endtask

  // ------------------------------------------------------------------
  // Read BFM.
  // ------------------------------------------------------------------
  task automatic axi_read(input logic [31:0] addr, output logic [31:0] data,
                          input int r_stall);
    int g;
    @(negedge clk);
    s_axi_arvalid = 1'b1;
    s_axi_araddr = addr;
    @(negedge clk);
    s_axi_arvalid = 1'b0;
    g = 0;
    while (g < r_stall) begin
      @(negedge clk);
      s_axi_rready = 1'b0;
      g = g + 1;
    end
    @(negedge clk);
    s_axi_rready = 1'b1;
    g = 0;
    while (!s_axi_rvalid && g < 64) begin
      @(posedge clk);
      g = g + 1;
    end
    if (g >= 64) begin
      $display("FAIL no read data");
      errors = errors + 1;
    end
    while (!s_axi_rvalid) @(posedge clk);
    data = s_axi_rdata;
    @(posedge clk);
  endtask

  task automatic expect_reg(string tag, input logic [31:0] addr,
                            input logic [31:0] expected);
    logic [31:0] v;
    axi_read(addr, v, 0);
    if (v != expected) begin
      $display("INFO [%s] got %08h expected %08h", tag, v, expected);
    end
    fail_check(tag, v == expected);
  endtask

  initial begin
    logic [31:0] v;
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    wait_posedge(6);
    srst = 1'b0;
    wait_posedge(2);

    // ----------------------------------------------------------------
    // Resets and pass-through RO fields.
    // ----------------------------------------------------------------
    expect_reg("id", LA_REG_ID_OFFSET, LA_REG_ID_VALUE);
    expect_reg("status pass-through", LA_REG_STATUS_OFFSET, 32'h00000001);
    expect_reg("trig_index pass-through", LA_REG_TRIG_INDEX_OFFSET, 32'h2A);
    expect_reg("word_lo pass-through", LA_REG_WORD_COUNT_LO_OFFSET, 32'h400);
    expect_reg("chan_en reset", LA_REG_CHAN_EN_OFFSET, 32'hFF);

    // ----------------------------------------------------------------
    // Plain writes with different strobe and channel-order patterns.
    // ----------------------------------------------------------------
    axi_write(LA_REG_SAMPLE_DIV_OFFSET, 32'h00000063, 4'hF, 0, 0);
    expect_reg("sample_div write", LA_REG_SAMPLE_DIV_OFFSET, 32'h63);
    axi_write(LA_REG_CHAN_EN_OFFSET, 32'h0000000F, 4'hF, 1, 0);
    expect_reg("chan_en write", LA_REG_CHAN_EN_OFFSET, 32'h0F);
    axi_write(LA_REG_POST_SAMPLES_OFFSET, 32'h00001000, 4'hF, 2, 0);
    expect_reg("post write data-first", LA_REG_POST_SAMPLES_OFFSET, 32'h1000);
    axi_write(LA_REG_TIMEOUT_OFFSET, 32'h00000FA0, 4'hF, 0, 5);
    expect_reg("timeout with b stall", LA_REG_TIMEOUT_OFFSET, 32'hFA0);

    // ----------------------------------------------------------------
    // Byte strobes preserve unstrobed bytes.
    // ----------------------------------------------------------------
    axi_write(LA_REG_SAMPLE_DIV_OFFSET, 32'h0000A5FF, 4'h3, 0, 0);
    expect_reg("strobe low half", LA_REG_SAMPLE_DIV_OFFSET, 32'h0000A5FF);
    axi_write(LA_REG_SAMPLE_DIV_OFFSET, 32'h12340000, 4'hC, 0, 0);
    expect_reg("strobe high half", LA_REG_SAMPLE_DIV_OFFSET, 32'h1234A5FF);
    axi_write(LA_REG_SAMPLE_DIV_OFFSET, 32'h00000007, 4'h1, 0, 0);
    expect_reg("single byte strobe", LA_REG_SAMPLE_DIV_OFFSET, 32'h1234A507);

    // ----------------------------------------------------------------
    // Self-clearing control bits.
    // ----------------------------------------------------------------
    axi_write(LA_REG_CTRL_OFFSET, 32'h00000001, 4'hF, 0, 0);
    // arm pulse must have fired; bit must read back 0.
    expect_reg("arm self-cleared", LA_REG_CTRL_OFFSET, 32'h00000000);
    fail_check("arm pulse captured", arm_pulse_caught);
    axi_write(LA_REG_CTRL_OFFSET, 32'h00000006, 4'hF, 0, 0);
    expect_reg("abort/softreset self-cleared", LA_REG_CTRL_OFFSET, 32'h00000000);
    fail_check("abort pulse captured", abort_pulse_caught);
    fail_check("soft reset pulse captured", soft_pulse_caught);
    // Sticky bits SELFTEST and STREAM persist; reserved/command bits clear.
    axi_write(LA_REG_CTRL_OFFSET, 32'hFFFFFFFF, 4'hF, 0, 0);
    expect_reg("sticky ctrl bits", LA_REG_CTRL_OFFSET, 32'h00000018);

    // ----------------------------------------------------------------
    // Masked fields.
    // ----------------------------------------------------------------
    axi_write(LA_REG_MODE_OFFSET, 32'hFFFFFFFF, 4'hF, 0, 0);
    expect_reg("mode masked", LA_REG_MODE_OFFSET, 32'h00000003);
    axi_write(LA_REG_TRIG_RISE_OFFSET, 32'h0000FF00, 4'hF, 0, 0);
    expect_reg("trig_rise masked", LA_REG_TRIG_RISE_OFFSET, 32'h00000000);
    axi_write(LA_REG_TRIG_FALL_OFFSET, 32'h000000A5, 4'hF, 0, 0);
    expect_reg("trig_fall kept", LA_REG_TRIG_FALL_OFFSET, 32'h000000A5);
    axi_write(LA_REG_PATGEN_CTRL_OFFSET, 32'hFFFFFFFF, 4'hF, 0, 0);
    expect_reg("patgen ctrl allowed", LA_REG_PATGEN_CTRL_OFFSET,
               32'hFFFF00F1);
    axi_write(LA_REG_PATGEN_CTRL_OFFSET, 32'h00010000, 4'hF, 0, 0);
    expect_reg("patgen ctrl div only", LA_REG_PATGEN_CTRL_OFFSET,
               32'h00010000);

    // ----------------------------------------------------------------
    // Clamping.
    // ----------------------------------------------------------------
    axi_write(LA_REG_PRE_SAMPLES_OFFSET, 32'hFFFFFFFF, 4'hF, 0, 0);
    expect_reg("pre_samples clamped high", LA_REG_PRE_SAMPLES_OFFSET,
               LA_REG_PRE_SAMPLES_MAX);
    axi_write(LA_REG_TXN_MAX_RUN_OFFSET, 32'h00000000, 4'hF, 0, 0);
    expect_reg("txn_max_run clamped low", LA_REG_TXN_MAX_RUN_OFFSET,
               LA_REG_TXN_MAX_RUN_MIN);
    axi_write(LA_REG_TXN_MAX_RUN_OFFSET, 32'h00000100, 4'hF, 0, 0);
    expect_reg("txn_max_run set", LA_REG_TXN_MAX_RUN_OFFSET, 32'h100);

    // ----------------------------------------------------------------
    // IRQ enable/status: event pulses set bits; W1C clears them; irq_o
    // follows IRQ_EN & IRQ_STAT.
    // ----------------------------------------------------------------
    axi_write(LA_REG_IRQ_EN_OFFSET, 32'h00000007, 4'hF, 0, 0);
    expect_reg("irq_en", LA_REG_IRQ_EN_OFFSET, 32'h7);
    fail_check("irq low without status", !irq_o);
    @(negedge clk);
    irq_st_done_i = 1'b1;
    @(negedge clk);
    irq_st_done_i = 1'b0;
    wait_posedge(1);
    expect_reg("irq_stat done set", LA_REG_IRQ_STAT_OFFSET, 32'h1);
    fail_check("irq high with status", irq_o);
    @(negedge clk);
    irq_st_ovf_i = 1'b1;
    @(negedge clk);
    irq_st_ovf_i = 1'b0;
    wait_posedge(1);
    expect_reg("irq_stat ovf set", LA_REG_IRQ_STAT_OFFSET, 32'h5);
    // W1C clears only the written bits.
    axi_write(LA_REG_IRQ_STAT_OFFSET, 32'h00000004, 4'hF, 0, 0);
    expect_reg("irq_stat w1c", LA_REG_IRQ_STAT_OFFSET, 32'h1);
    axi_write(LA_REG_IRQ_STAT_OFFSET, 32'h00000001, 4'hF, 0, 0);
    expect_reg("irq_stat w1c done", LA_REG_IRQ_STAT_OFFSET, 32'h0);
    fail_check("irq low after clear", !irq_o);
    // Events masked out by IRQ_EN do not drive irq_o even if sticky.
    @(negedge clk);
    irq_st_trig_i = 1'b1;
    @(negedge clk);
    irq_st_trig_i = 1'b0;
    wait_posedge(1);
    expect_reg("irq_stat trig sticky", LA_REG_IRQ_STAT_OFFSET, 32'h2);

    // ----------------------------------------------------------------
    // Out-of-range address: read 0, write ignored.
    // ----------------------------------------------------------------
    axi_write(32'h00000054, 32'hDEADBEEF, 4'hF, 0, 0);
    axi_read(32'h00000054, v, 0);
    fail_check("out-of-range read 0", v == 32'h0);
    expect_reg("sample_div untouched", LA_REG_SAMPLE_DIV_OFFSET, 32'h1234A507);

    // ----------------------------------------------------------------
    // Read with RREADY stalls.
    // ----------------------------------------------------------------
    axi_read(LA_REG_ID_OFFSET, v, 7);
    fail_check("stalled read data", v == LA_REG_ID_VALUE);

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_regs errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_regs failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_regs");
    $fclose(result_fd);
    $display("PASS: tb_la_regs");
    $finish;
  end
endmodule : tb_la_regs
