// P2.9 la_patgen
//
// Self-test pattern generator. Drives the 8 channel outputs with one of the
// PATGEN_CTRL modes:
//   0 counter       ch[7:0] = free-running 8-bit counter
//   1 walking-1     one-hot bit that walks
//   2 PRBS8         x^8+x^6+x^5+x^4+1 LFSR (seed 0x01, never the zero state)
//   3 UART 115200   ch0 = TX of the repeated frame "LA-TEST\n", 8N1
//   4 SPI 1 MHz     ch0 SCK, ch1 MOSI, ch2 CS_N; frame 0x4C 0x41 MSB first
//   5 I2C 400 kHz   ch3 SCL, ch4 SDA; repeated write: START, 0xA0, ACK,
//                   0x4C, ACK, 0x41, ACK, STOP
//
// Rates derive from the CLK_HZ parameter (default 100 MHz) and are scaled
// by (DIV+1) with DIV = PATGEN_CTRL[31:16]; DIV = 0 gives the nominal
// rates. Mode and DIV are latched at the en_i rising edge and all pattern
// state restarts there; while disabled the outputs are held low. In I2C
// mode the ACK bits are driven low so protocol decoders can be self-tested
// end to end without a real slave.
//
// Interface: single FCLK, synchronous active-high srst; en_i/mode_i/div_i
// come from PATGEN_CTRL.

module la_patgen #(
  parameter longint CLK_HZ = 100_000_000
) (
  input  logic        clk,
  input  logic        srst,
  input  logic        en_i,
  input  logic [3:0]  mode_i,
  input  logic [15:0] div_i,
  output logic [7:0]  ch_o
);

  localparam longint UART_BIT = CLK_HZ / 115200;      // 868 at 100 MHz
  localparam longint SPI_HALF = CLK_HZ / 2_000_000;   // 50 at 100 MHz
  localparam longint I2C_HALF = CLK_HZ / 800_000;     // 125 at 100 MHz

  // ------------------------------------------------------------------
  // Control: configuration latched on the enable edge.
  // ------------------------------------------------------------------
  logic       en_d;
  logic       en_rise;
  logic       running;
  logic [3:0] mode_q;
  logic [15:0] div_q;

  always_ff @(posedge clk) begin
    if (srst) begin
      en_d    <= 1'b0;
      running <= 1'b0;
      mode_q  <= '0;
      div_q   <= '0;
    end else begin
      en_d <= en_i;
      if (en_i && !en_d) begin
        running <= 1'b1;
        mode_q  <= mode_i;
        div_q   <= div_i;
      end else if (!en_i) begin
        running <= 1'b0;
      end
    end
  end

  assign en_rise = en_i && !en_d;

  // Scale factor (DIV+1) and derived period counters.
  logic [31:0] scale;
  assign scale = {16'd0, div_q} + 32'd1;

  // ------------------------------------------------------------------
  // Step-timed modes: counter, walking-1, PRBS8 (one step per DIV+1
  // cycles).
  // ------------------------------------------------------------------
  logic [7:0]  lfsr_q;
  logic [7:0]  counter_q;
  logic [2:0]  walk_q;
  logic [31:0] gen_cnt;

  always_ff @(posedge clk) begin
    if (srst) begin
      lfsr_q    <= 8'h01;
      counter_q <= '0;
      walk_q    <= '0;
      gen_cnt   <= '0;
    end else if (en_rise || !running) begin
      lfsr_q    <= 8'h01;
      counter_q <= '0;
      walk_q    <= '0;
      gen_cnt   <= '0;
    end else if (mode_q[3:2] == 2'd0) begin
      if (gen_cnt >= scale - 32'd1) begin
        gen_cnt <= '0;
        case (mode_q[1:0])
          2'd0: counter_q <= counter_q + 8'd1;   // counter
          2'd1: walk_q    <= walk_q + 3'd1;      // walking-1
          default: lfsr_q <= {lfsr_q[6:0],
                              lfsr_q[7] ^ lfsr_q[5] ^ lfsr_q[4] ^ lfsr_q[3]};
        endcase
      end else begin
        gen_cnt <= gen_cnt + 32'd1;
      end
    end
  end

  // ------------------------------------------------------------------
  // UART TX bit-bang on ch0: repeated "LA-TEST\n", 8N1, LSB first.
  // Bit periods are scale * UART_BIT cycles; idle is high.
  // ------------------------------------------------------------------
  logic [31:0] uart_cnt;
  logic [31:0] uart_byte;     // frame byte index 0..7
  logic [31:0] uart_bit;      // 0 start, 1..8 data, 9 stop, then frame gap
  logic        uart_tx;

  function automatic logic [7:0] uart_frame_byte(input logic [31:0] idx);
    case (idx)
      32'd0: uart_frame_byte = "L";
      32'd1: uart_frame_byte = "A";
      32'd2: uart_frame_byte = "-";
      32'd3: uart_frame_byte = "T";
      32'd4: uart_frame_byte = "E";
      32'd5: uart_frame_byte = "S";
      32'd6: uart_frame_byte = "T";
      default: uart_frame_byte = 8'h0A;
    endcase
  endfunction

  logic [31:0] uart_period_m1;
  assign uart_period_m1 = scale * UART_BIT[31:0] - 32'd1;

  always_ff @(posedge clk) begin
    if (srst) begin
      uart_cnt  <= '0;
      uart_byte <= '0;
      uart_bit  <= '0;
      uart_tx   <= 1'b1;
    end else if (en_rise) begin
      uart_cnt  <= '0;
      uart_byte <= '0;
      uart_bit  <= '0;
      uart_tx   <= 1'b1;
    end else if (running && (mode_q == 4'd3)) begin
      if (uart_cnt >= uart_period_m1) begin
        uart_cnt <= '0;
        if (uart_byte < 32'd8) begin
          if (uart_bit == 32'd0) begin
            uart_tx <= 1'b0;                     // start
            uart_bit <= 32'd1;
          end else if (uart_bit <= 32'd8) begin
            uart_tx <= uart_frame_byte(uart_byte)[uart_bit - 32'd1];
            uart_bit <= uart_bit + 32'd1;
          end else begin                         // stop bit
            uart_tx <= 1'b1;
            if (uart_byte == 32'd7) begin
              uart_byte <= 32'd8;                // frame done, go to gap
              uart_bit  <= '0;
            end else begin
              uart_byte <= uart_byte + 32'd1;    // next byte, back to back
              uart_bit  <= '0;
            end
          end
        end else begin
          // Inter-frame gap of 10 bit times, then repeat the frame.
          if (uart_bit < 32'd10) begin
            uart_tx <= 1'b1;
            uart_bit <= uart_bit + 32'd1;
          end else begin
            uart_byte <= '0;
            uart_bit  <= '0;
          end
        end
      end else begin
        uart_cnt <= uart_cnt + 32'd1;
      end
    end
  end

  // ------------------------------------------------------------------
  // SPI bit-bang: ch0 SCK, ch1 MOSI, ch2 CS_N. Frame 0x4C 0x41 MSB first,
  // CPOL=0 CPHA=0: MOSI settles while SCK is low, sampled on the rise.
  // ------------------------------------------------------------------
  logic [31:0] spi_cnt;
  logic [31:0] spi_step;    // 0..35 per frame
  logic        spi_sck;
  logic        spi_mosi;
  logic        spi_cs;

  function automatic logic spi_frame_bit(input logic [31:0] b);
    logic [15:0] frame;
    frame = 16'h4C41;
    spi_frame_bit = frame[15 - b[3:0]];
  endfunction

  logic [31:0] spi_half_m1;
  assign spi_half_m1 = scale * SPI_HALF[31:0] - 32'd1;

  always_ff @(posedge clk) begin
    if (srst) begin
      spi_cnt  <= '0;
      spi_step <= '0;
      spi_sck  <= 1'b0;
      spi_mosi <= 1'b0;
      spi_cs   <= 1'b1;
    end else if (en_rise) begin
      spi_cnt  <= '0;
      spi_step <= '0;
      spi_sck  <= 1'b0;
      spi_mosi <= 1'b0;
      spi_cs   <= 1'b1;
    end else if (running && (mode_q == 4'd4)) begin
      if (spi_cnt >= spi_half_m1) begin
        spi_cnt <= '0;
        // step 0: CS asserts, SCK low, MOSI = bit 15
        // steps 2b+1: SCK high (bit 15-b stable)
        // steps 2b   : SCK low, MOSI = bit 15-b (b = 1..15)
        // step 32   : trailing SCK low
        // step 33   : CS deasserts
        // step 34   : idle half period, then wrap
        if (spi_step == 32'd0) begin
          spi_cs   <= 1'b0;
          spi_sck  <= 1'b0;
          spi_mosi <= spi_frame_bit(32'd0);
        end else if (spi_step <= 32'd31) begin
          if (spi_step[0]) begin
            spi_sck <= 1'b1;
          end else begin
            spi_sck  <= 1'b0;
            spi_mosi <= spi_frame_bit(spi_step >> 1);
          end
        end else if (spi_step == 32'd32) begin
          spi_sck <= 1'b0;
        end else if (spi_step == 32'd33) begin
          spi_cs <= 1'b1;
        end
        if (spi_step >= 32'd34) spi_step <= '0;
        else spi_step <= spi_step + 32'd1;
      end else begin
        spi_cnt <= spi_cnt + 32'd1;
      end
    end
  end

  // ------------------------------------------------------------------
  // I2C bit-bang: ch3 SCL, ch4 SDA. Repeated master write with the ACK
  // bits driven low: START, 0xA0, ACK, 0x4C, ACK, 0x41, ACK, STOP.
  // Each step is half an SCL period (scale * I2C_HALF cycles).
  // ------------------------------------------------------------------
  logic [31:0] i2c_cnt;
  logic [31:0] i2c_step;    // 0 START; 1..54 slots; 55 STOP; 56 idle
  logic        i2c_scl;
  logic        i2c_sda;

  function automatic logic i2c_frame_bit(input logic [31:0] slot);
    logic [26:0] stream;
    logic [31:0] i;
    stream = {8'b10100000, 1'b0, 8'b01001100, 1'b0, 8'b01000001, 1'b0};
    i = 26 - slot;
    i2c_frame_bit = stream[i];
  endfunction

  logic [31:0] i2c_half_m1;
  assign i2c_half_m1 = scale * I2C_HALF[31:0] - 32'd1;

  always_ff @(posedge clk) begin
    if (srst) begin
      i2c_cnt  <= '0;
      i2c_step <= '0;
      i2c_scl  <= 1'b1;
      i2c_sda  <= 1'b1;
    end else if (en_rise) begin
      i2c_cnt  <= '0;
      i2c_step <= '0;
      i2c_scl  <= 1'b1;
      i2c_sda  <= 1'b1;
    end else if (running && (mode_q == 4'd5)) begin
      if (i2c_cnt >= i2c_half_m1) begin
        i2c_cnt <= '0;
        if (i2c_step == 32'd0) begin
          // START: SDA falls while SCL is high.
          i2c_scl <= 1'b1;
          i2c_sda <= 1'b0;
        end else if (i2c_step == 32'd55) begin
          // STOP: SDA rises while SCL is high.
          i2c_scl <= 1'b1;
          i2c_sda <= 1'b1;
        end else if (i2c_step >= 32'd56) begin
          i2c_scl <= 1'b1;
          i2c_sda <= 1'b1;
        end else begin
          // Slot (step-1)/2: odd steps drive SCL low with the bit value on
          // SDA; even steps raise SCL (data sampled on the rise).
          if (i2c_step[0]) begin
            i2c_scl <= 1'b0;
            i2c_sda <= i2c_frame_bit((i2c_step - 32'd1) >> 1);
          end else begin
            i2c_scl <= 1'b1;
          end
        end
        if (i2c_step >= 32'd56) i2c_step <= '0;
        else i2c_step <= i2c_step + 32'd1;
      end else begin
        i2c_cnt <= i2c_cnt + 32'd1;
      end
    end
  end

  // ------------------------------------------------------------------
  // Output mux (channel map per mode).
  // ------------------------------------------------------------------
  logic [7:0] ch_mux;

  always_comb begin
    ch_mux = '0;
    case (mode_q)
      4'd0: ch_mux = counter_q;
      4'd1: ch_mux = 8'h01 << walk_q;
      4'd2: ch_mux = lfsr_q;
      4'd3: ch_mux = {7'h00, uart_tx};
      4'd4: ch_mux = {5'h00, spi_cs, spi_mosi, spi_sck};
      4'd5: ch_mux = {3'h00, i2c_sda, i2c_scl, 3'h000};
      default: ch_mux = '0;
    endcase
  end

  always_ff @(posedge clk) begin
    if (srst) begin
      ch_o <= '0;
    end else if (running) begin
      ch_o <= ch_mux;
    end else begin
      ch_o <= '0;
    end
  end

endmodule : la_patgen
