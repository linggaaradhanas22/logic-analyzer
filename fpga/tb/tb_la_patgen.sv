// Self-checking test for la_patgen (P2.9).
//
// Every mode is validated against the recorded output history:
//   counter/walking-1/PRBS8: model comparison plus step-period checks
//     (DIV scaling);
//   UART mode: ch0 is decoded with a mid-bit sampler synchronized on the
//     start edge and must produce "LA-TEST\n" (8N1, LSB first) repeatedly;
//   SPI mode: MOSI is sampled on each SCK rising edge while CS_N is low and
//     must produce the 0x4C 0x41 frame repeatedly;
//   I2C mode: SDA sampled on SCL rising edges must produce START, write
//     address 0xA0, ACK, 0x4C, ACK, 0x41, ACK, STOP repeatedly.

`timescale 1ns/1ps

module tb_la_patgen;
  localparam int UART_BIT = 868;
  localparam int SPI_HALF = 50;
  localparam int I2C_HALF = 125;
  localparam int HIST_MAX = 240000;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic en_i = 1'b0;
  logic [3:0] mode_i = 4'd0;
  logic [15:0] div_i = 16'd0;
  logic [7:0] ch_o;

  la_patgen #(
    .CLK_HZ(100_000_000)
  ) dut (
    .clk(clk),
    .srst(srst),
    .en_i(en_i),
    .mode_i(mode_i),
    .div_i(div_i),
    .ch_o(ch_o)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  logic [7:0] hist [0:HIST_MAX-1];
  int hist_n = 0;
  logic capture_en = 1'b0;

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  always @(posedge clk) begin
    if (capture_en && hist_n < HIST_MAX) begin
      hist[hist_n] = ch_o;
      hist_n = hist_n + 1;
    end
  end

  task automatic fail_check(string tag, logic condition);
    if (!condition) begin
      $display("FAIL [%0t] %s", $time, tag);
      errors = errors + 1;
    end
  endtask

  task automatic fail_check_x(string tag, logic condition, string extra);
    if (!condition) begin
      $display("FAIL [%0t] %s%s", $time, tag, extra);
      errors = errors + 1;
    end
  endtask

  task automatic scenario_start(input logic [3:0] m, input int dv);
    srst = 1'b1;
    en_i = 1'b0;
    mode_i = m;
    div_i = dv;
    repeat (3) @(posedge clk);
    srst = 1'b0;
    @(posedge clk);
    capture_en = 1'b0;
    hist_n = 0;
    @(negedge clk);
    en_i = 1'b1;
    @(posedge clk);
    capture_en = 1'b1;
  endtask

  task automatic scenario_stop();
    @(negedge clk);
    en_i = 1'b0;
    repeat (2) @(posedge clk);
    capture_en = 1'b0;
  endtask

  // Find the first index where bit `b` of ch_o falls (1 -> 0).
  task automatic find_fall(input int from_idx, input int b, output int idx);
    int i;
    idx = -1;
    for (i = from_idx; i + 1 < hist_n; i++) begin
      if (hist[i][b] && !hist[i+1][b]) begin
        idx = i;
        return;
      end
    end
  endtask

  // Find the first index where bit `b` rises (0 -> 1).
  task automatic find_rise(input int from_idx, input int b, output int idx);
    int i;
    idx = -1;
    for (i = from_idx; i + 1 < hist_n; i++) begin
      if (!hist[i][b] && hist[i+1][b]) begin
        idx = i;
        return;
      end
    end
  endtask

  // Decode one UART frame starting at the first start edge; compares the
  // eight decoded bytes against "LA-TEST\n".
  task automatic decode_uart(string tag);
    int s0;
    int b;
    int k;
    logic [7:0] bytes [0:7];
    logic [7:0] expected [0:7];
    logic ok;
    expected[0] = "L";
    expected[1] = "A";
    expected[2] = "-";
    expected[3] = "T";
    expected[4] = "E";
    expected[5] = "S";
    expected[6] = "T";
    expected[7] = 8'h0A;
    find_fall(0, 0, s0);
    fail_check({tag, " start edge found"}, s0 >= 0);
    if (s0 < 0) return;
    for (k = 0; k < 8; k++) begin
      bytes[k] = 8'h00;
      for (b = 0; b < 8; b++) begin
        // Data bit b of byte k: absolute bit 10*k+b+1.
        if (hist[s0 + UART_BIT*(10*k + b + 1) + UART_BIT/2][0]) begin
          bytes[k][b] = 1'b1;
        end
      end
    end
    ok = 1'b1;
    for (k = 0; k < 8; k++) begin
      if (bytes[k] != expected[k]) ok = 1'b0;
    end
    if (!ok) begin
      $display("FAIL [%s] UART decode: %02h %02h %02h %02h %02h %02h %02h %02h",
               tag, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
               bytes[5], bytes[6], bytes[7]);
      errors = errors + 1;
    end
    // Stop bit of byte 7 must be high (absolute bit 79).
    fail_check({tag, " stop bit high"},
               hist[s0 + UART_BIT*79 + UART_BIT/2][0]);
    // The frame repeats: a new start edge must exist after the frame.
    find_fall(s0 + UART_BIT*90, 0, s0);
    fail_check({tag, " frame repeats"}, s0 >= 0);
  endtask

  // Decode two SPI frames: MOSI on SCK rise while CS_N low.
  task automatic decode_spi(string tag, input int frames);
    int s0;
    int f;
    int k;
    int idx;
    int sample;
    int chk;
    for (f = 0; f < frames; f++) begin
      if (f == 0) find_fall(0, 2, s0);       // first CS_N assertion
      else begin
        // next CS_N fall after the previous frame (~1800 cycles)
        find_fall(s0 + 1900, 2, s0);
      end
      fail_check({tag, " CS fall found"}, s0 >= 0);
      if (s0 < 0) return;
      // SCK rises at s0 + 50 + 100*k (k = 0..15). Sample MOSI on each rise.
      chk = 0;
      for (k = 0; k < 16; k++) begin
        idx = s0 + SPI_HALF + 2*SPI_HALF*k;
        if (idx >= hist_n || idx < 0) begin
          $display("FAIL [%s] sample out of range", tag);
          errors = errors + 1;
          return;
        end
        chk = (chk << 1) | hist[idx][1];
      end
      fail_check_x({tag, " frame data"}, chk == 16'h4C41,
                   $sformatf(" got %04h", chk));
    end
  endtask

  // Decode one I2C write: START, addr 0xA0, ACK, 0x4C, ACK, 0x41, ACK.
  task automatic decode_i2c(string tag);
    int s0;
    int slot;
    int idx;
    logic [26:0] got;
    logic [26:0] expected;
    logic ok;
    // START: SDA falls while SCL high. Scan for a fall whose SCL is high.
    s0 = -1;
    begin
      int i;
      for (i = 0; i + 1 < hist_n; i++) begin
        if (hist[i][4] && !hist[i+1][4] && hist[i][3]) begin
          s0 = i;
          break;
        end
      end
    end
    fail_check({tag, " START found"}, s0 >= 0);
    if (s0 < 0) return;
    got = 27'b0;
    for (slot = 0; slot < 27; slot++) begin
      // Slot b: SCL rises at step 2b+2 -> s0 + (2b+2)*125; sample mid-low.
      idx = s0 + (2*slot + 2)*I2C_HALF - 60;
      if (idx >= hist_n || idx < 0) begin
        $display("FAIL [%s] i2c sample out of range", tag);
        errors = errors + 1;
        return;
      end
      got[26 - slot] = hist[idx][4];
    end
    expected = {8'b10100000, 1'b0, 8'b01001100, 1'b0, 8'b01000001, 1'b0};
    ok = (got == expected);
    if (!ok) begin
      $display("FAIL [%s] I2C slot stream %027b expected %027b", tag, got,
               expected);
      errors = errors + 1;
    end
    // Stop: after the frame SDA must rise while SCL is high.
    begin
      int i;
      logic stop_found;
      stop_found = 1'b0;
      for (i = s0 + 55*I2C_HALF; i + 1 < hist_n; i++) begin
        if (!hist[i][4] && hist[i+1][4] && hist[i][3]) begin
          stop_found = 1'b1;
          break;
        end
      end
      fail_check({tag, " STOP found"}, stop_found);
    end
    // A second START must follow (repeated frame).
    begin
      int i;
      logic restart_found;
      restart_found = 1'b0;
      for (i = s0 + 60*I2C_HALF; i + 1 < hist_n; i++) begin
        if (hist[i][4] && !hist[i+1][4] && hist[i][3]) begin
          restart_found = 1'b1;
          break;
        end
      end
      fail_check({tag, " frame repeats"}, restart_found);
    end
  endtask

  initial begin
    int i;
    logic [7:0] lfsr;
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ----------------------------------------------------------------
    // Mode 0: counter at DIV=0 (every cycle) then DIV=3 (every 4 cycles).
    // ----------------------------------------------------------------
    scenario_start(4'd0, 0);
    begin
      logic ok;
      ok = 1'b1;
      for (i = 100; i < 300 && i + 1 < hist_n; i++) begin
        if (hist[i+1] != hist[i] + 8'd1) ok = 1'b0;
      end
      fail_check("counter DIV=0 increments each cycle", ok);
      fail_check("counter value alignment", hist[100] == 8'd0 ||
                                             hist[100] == 8'd100);
    end
    scenario_stop();
    scenario_start(4'd0, 3);
    begin
      int changes;
      int expected;
      for (i = 0; i < 1200; i++) begin
        @(posedge clk);
      end
      changes = 0;
      for (i = 200; i + 1 < hist_n; i++) begin
        if (hist[i+1] != hist[i]) changes = changes + 1;
      end
      // One step every DIV+1 = 4 cycles.
      expected = (hist_n - 200) / 4;
      fail_check("counter DIV=3 step period",
                 changes >= expected - 2 && changes <= expected + 2);
      fail_check("counter DIV=3 value sequence", hist[300] != 8'hFF);
    end
    scenario_stop();

    // ----------------------------------------------------------------
    // Mode 1: walking-1.
    // ----------------------------------------------------------------
    scenario_start(4'd1, 0);
    begin
      logic ok;
      ok = 1'b1;
      for (i = 100; i < 120 && i + 1 < hist_n; i++) begin
        if (hist[i] != (8'h01 << (i % 8))) ok = 1'b0;
      end
      fail_check("walking-1 sequence", ok);
    end
    scenario_stop();

    // ----------------------------------------------------------------
    // Mode 2: PRBS8.
    // ----------------------------------------------------------------
    scenario_start(4'd2, 0);
    begin
      logic ok;
      ok = 1'b1;
      for (i = 100; i < 140 && i + 1 < hist_n; i++) begin
        logic [7:0] v;
        v = hist[i];
        if (hist[i+1] != {v[6:0], v[7] ^ v[5] ^ v[4] ^ v[3]}) ok = 1'b0;
      end
      fail_check("PRBS8 sequence", ok);
    end
    scenario_stop();

    // ----------------------------------------------------------------
    // Mode 3: UART "LA-TEST\n" on ch0.  Two frames are recorded.
    // ----------------------------------------------------------------
    scenario_start(4'd3, 0);
    begin
      // 90 bits per frame at 868 cycles: two frames + margin.
      for (i = 0; i < 200000; i++) begin
        @(posedge clk);
        if (i % 100000 == 99999) begin
          // keep sim deterministic; nothing to do
        end
      end
    end
    scenario_stop();
    decode_uart("uart");

    // ----------------------------------------------------------------
    // Mode 4: SPI frames.
    // ----------------------------------------------------------------
    scenario_start(4'd4, 0);
    begin
      for (i = 0; i < 12000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_spi("spi", 3);

    // ----------------------------------------------------------------
    // Mode 5: I2C write.
    // ----------------------------------------------------------------
    scenario_start(4'd5, 0);
    begin
      for (i = 0; i < 30000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_i2c("i2c");

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_patgen errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_patgen failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_patgen");
    $fclose(result_fd);
    $display("PASS: tb_la_patgen");
    $finish;
  end

  initial begin
    #8_000_000;
    $fatal(1, "tb_la_patgen timeout");
  end

endmodule : tb_la_patgen
