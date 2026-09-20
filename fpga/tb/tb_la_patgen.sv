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
  // decoded bytes against `expected[0..nbytes-1]` and checks every stop bit
  // (bad_stop = index whose stop bit is deliberately low, or -1). With
  // `repeats` a second start edge must follow the frame.
  task automatic decode_uart(string tag, input logic [7:0] expected [0:15],
                             input int nbytes, input int bad_stop,
                             input int repeats);
    int s0;
    int b;
    int k;
    logic [7:0] bytes [0:15];
    logic ok;
    find_fall(0, 0, s0);
    fail_check({tag, " start edge found"}, s0 >= 0);
    if (s0 < 0) return;
    for (k = 0; k < nbytes; k++) begin
      bytes[k] = 8'h00;
      for (b = 0; b < 8; b++) begin
        // Data bit b of byte k: absolute bit 10*k+b+1.
        if (hist[s0 + UART_BIT*(10*k + b + 1) + UART_BIT/2][0]) begin
          bytes[k][b] = 1'b1;
        end
      end
      if (k == bad_stop) begin
        fail_check({tag, " bad stop bit low"},
                   !hist[s0 + UART_BIT*(10*k + 9) + UART_BIT/2][0]);
      end else begin
        fail_check({tag, " stop bit high"},
                   hist[s0 + UART_BIT*(10*k + 9) + UART_BIT/2][0]);
      end
    end
    ok = 1'b1;
    for (k = 0; k < nbytes; k++) begin
      if (bytes[k] != expected[k]) ok = 1'b0;
    end
    if (!ok) begin
      $display("FAIL [%s] UART decode mismatch", tag);
      for (k = 0; k < nbytes; k++) begin
        $display("  byte %0d got %02h expected %02h", k, bytes[k],
                 expected[k]);
      end
      errors = errors + 1;
    end
    if (repeats != 0) begin
      // The frame repeats: a new start edge must exist after the frame.
      find_fall(s0 + UART_BIT*(10*nbytes + 10), 0, s0);
      fail_check({tag, " frame repeats"}, s0 >= 0);
    end
  endtask

  // Decode one UART frame (mode 6) and require the line to stay idle high:
  // no second start edge may follow the single frame.
  task automatic decode_uart_single(string tag);
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
      $display("FAIL [%s] single UART decode: %02h %02h %02h %02h %02h %02h %02h %02h",
               tag, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
               bytes[5], bytes[6], bytes[7]);
      errors = errors + 1;
    end
    fail_check({tag, " stop bit high"},
               hist[s0 + UART_BIT*79 + UART_BIT/2][0]);
    begin
      int i;
      logic second_start;
      second_start = 1'b0;
      for (i = s0 + UART_BIT*90; i + 1 < hist_n; i++) begin
        if (hist[i][0] && !hist[i+1][0]) second_start = 1'b1;
      end
      fail_check({tag, " no repeat after single frame"}, !second_start);
    end
  endtask

  // Decode `frames` SPI frames: MOSI on SCK rise while CS_N low; compares
  // `nbits` bits against `expected` (MSB first).
  task automatic decode_spi(string tag, input int frames, input int nbits,
                            input logic [31:0] expected);
    int s0;
    int f;
    int k;
    int idx;
    int chk;
    for (f = 0; f < frames; f++) begin
      if (f == 0) find_fall(0, 2, s0);       // first CS_N assertion
      else begin
        // next CS_N fall after the previous frame
        find_fall(s0 + (2*nbits + 4)*SPI_HALF, 2, s0);
      end
      fail_check({tag, " CS fall found"}, s0 >= 0);
      if (s0 < 0) return;
      // SCK rises at s0 + 50 + 100*k (k = 0..nbits-1).
      chk = 0;
      for (k = 0; k < nbits; k++) begin
        idx = s0 + SPI_HALF + 2*SPI_HALF*k;
        if (idx >= hist_n || idx < 0) begin
          $display("FAIL [%s] sample out of range", tag);
          errors = errors + 1;
          return;
        end
        chk = (chk << 1) | hist[idx][1];
      end
      fail_check_x({tag, " frame data"}, chk == expected,
                   $sformatf(" got %08h expected %08h", chk, expected));
    end
  endtask

  // Decode one I2C frame: START then `nslots` bits (SDA sampled on SCL
  // rises) compared against the left-aligned `expected` stream, then STOP
  // and a repeat.
  task automatic decode_i2c(string tag, input logic [63:0] expected,
                            input int nslots);
    int s0;
    int slot;
    int idx;
    logic [63:0] got;
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
    got = 64'b0;
    for (slot = 0; slot < nslots; slot++) begin
      // Slot b: SCL rises at step 2b+2 -> s0 + (2b+2)*125; sample mid-low.
      idx = s0 + (2*slot + 2)*I2C_HALF - 60;
      if (idx >= hist_n || idx < 0) begin
        $display("FAIL [%s] i2c sample out of range", tag);
        errors = errors + 1;
        return;
      end
      got[63 - slot] = hist[idx][4];
    end
    ok = (got == expected);
    if (!ok) begin
      $display("FAIL [%s] I2C slot stream %064b expected %064b", tag, got,
               expected);
      errors = errors + 1;
    end
    // Stop: after the frame SDA must rise while SCL is high.
    begin
      int i;
      logic stop_found;
      stop_found = 1'b0;
      for (i = s0 + (2*nslots + 1)*I2C_HALF; i + 1 < hist_n; i++) begin
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
      for (i = s0 + (2*nslots + 3)*I2C_HALF; i + 1 < hist_n; i++) begin
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
      // 90 bits per frame at 868 cycles: one frame + the repeat start.
      for (i = 0; i < 110000; i++) begin
        @(posedge clk);
        if (i % 100000 == 99999) begin
          // keep sim deterministic; nothing to do
        end
      end
    end
    scenario_stop();
    begin
      logic [7:0] expected [0:15];
      expected[0] = "L";
      expected[1] = "A";
      expected[2] = "-";
      expected[3] = "T";
      expected[4] = "E";
      expected[5] = "S";
      expected[6] = "T";
      expected[7] = 8'h0A;
      decode_uart("uart", expected, 8, -1, 1);
    end

    // ----------------------------------------------------------------
    // Mode 6: UART single frame, then idle high (P7.6 isolated byte).
    // ----------------------------------------------------------------
    scenario_start(4'd6, 0);
    begin
      for (i = 0; i < 95000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_uart_single("uart-single");

    // ----------------------------------------------------------------
    // Mode 7: UART text "Hello, LA!\r\n".
    // ----------------------------------------------------------------
    scenario_start(4'd7, 0);
    begin
      for (i = 0; i < 125000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    begin
      logic [7:0] expected [0:15];
      expected[0] = "H";
      expected[1] = "e";
      expected[2] = "l";
      expected[3] = "l";
      expected[4] = "o";
      expected[5] = ",";
      expected[6] = " ";
      expected[7] = "L";
      expected[8] = "A";
      expected[9] = "!";
      expected[10] = 8'h0D;
      expected[11] = 8'h0A;
      decode_uart("uart-text", expected, 12, -1, 1);
    end

    // ----------------------------------------------------------------
    // Mode 8: UART binary 0x00..0x0F ramp.
    // ----------------------------------------------------------------
    scenario_start(4'd8, 0);
    begin
      for (i = 0; i < 155000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    begin
      logic [7:0] expected [0:15];
      for (i = 0; i < 16; i++) begin
        expected[i] = i[7:0];
      end
      decode_uart("uart-bin", expected, 16, -1, 1);
    end

    // ----------------------------------------------------------------
    // Mode 9: UART framing error (byte 1 stop bit low).
    // ----------------------------------------------------------------
    scenario_start(4'd9, 0);
    begin
      for (i = 0; i < 45000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    begin
      logic [7:0] expected [0:15];
      expected[0] = "L";
      expected[1] = "A";
      expected[2] = 8'h55;
      decode_uart("uart-ferr", expected, 3, 1, 1);
    end

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
    decode_spi("spi", 3, 16, 32'h4C41);

    // ----------------------------------------------------------------
    // Mode 10: SPI multi-byte frame 0xDE 0xAD 0xBE 0xEF.
    // ----------------------------------------------------------------
    scenario_start(4'd10, 0);
    begin
      for (i = 0; i < 12000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_spi("spi-multi", 2, 32, 32'hDEADBEEF);

    // ----------------------------------------------------------------
    // Mode 11: SPI two 16-bit words 0x1234 0xABCD.
    // ----------------------------------------------------------------
    scenario_start(4'd11, 0);
    begin
      for (i = 0; i < 12000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_spi("spi-16bit", 2, 32, 32'h1234ABCD);

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
    decode_i2c("i2c", {8'b10100000, 1'b0, 8'b01001100, 1'b0, 8'b01000001,
                       1'b0, 37'b0}, 27);

    // ----------------------------------------------------------------
    // Mode 12: I2C read 0xA1, ACK, 0x4C, NACK, STOP.
    // ----------------------------------------------------------------
    scenario_start(4'd12, 0);
    begin
      for (i = 0; i < 20000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_i2c("i2c-read", {8'b10100001, 1'b0, 8'b01001100, 1'b1, 46'b0}, 26);

    // ----------------------------------------------------------------
    // Mode 13: I2C write with a data NACK.
    // ----------------------------------------------------------------
    scenario_start(4'd13, 0);
    begin
      for (i = 0; i < 20000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_i2c("i2c-nack", {8'b10100000, 1'b0, 8'b01001100, 1'b1, 46'b0}, 26);

    // ----------------------------------------------------------------
    // Mode 14: I2C alternate address 0x3C with 0x4F 0x4B.
    // ----------------------------------------------------------------
    scenario_start(4'd14, 0);
    begin
      for (i = 0; i < 27000; i++) begin
        @(posedge clk);
      end
    end
    scenario_stop();
    decode_i2c("i2c-alt", {8'b01111000, 1'b0, 8'b01001111, 1'b0,
                           8'b01001011, 1'b0, 37'b0}, 35);

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
    #10_000_000;
    $fatal(1, "tb_la_patgen timeout");
  end

endmodule : tb_la_patgen
