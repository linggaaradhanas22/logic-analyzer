// Self-checking test for la_raw_packer (P2.6).
//
// Independent reconstruction scoreboard: every ACCEPTED sample is recorded
// in order; the emitted beats are recorded in order and the concatenated
// beat payloads (tkeep-truncated) must equal the accepted sample stream
// exactly. Structural checks: full beats carry tkeep=4'hF, partial beats
// carry the correct tail mask, tlast only on the last beat of a packet, no
// packet exceeds PACKET_WORDS words, and a flush with no partial beat defers
// tlast to the next complete word (never emits an empty beat).
//
// The DUT is instantiated with PACKET_WORDS = 16 so count-based tlast is
// exercised in simulation time.

`timescale 1ns/1ps

module tb_la_raw_packer;
  localparam int PACKET_WORDS = 16;
  localparam real CLK_PERIOD = 10.0;

  logic clk = 1'b0;
  logic srst = 1'b1;
  logic s_valid = 1'b0;
  logic s_ready;
  logic [7:0] s_data = 8'h00;
  logic flush_req = 1'b0;
  logic [31:0] pkt_words_i = 32'd0;
  logic m_valid;
  logic m_ready = 1'b1;
  logic [31:0] m_data;
  logic [3:0] m_tkeep;
  logic m_tlast;

  la_raw_packer #(
    .PACKET_WORDS(PACKET_WORDS)
  ) dut (
    .clk(clk),
    .srst(srst),
    .s_valid(s_valid),
    .s_ready(s_ready),
    .s_data(s_data),
    .flush_req(flush_req),
    .pkt_words_i(pkt_words_i),
    .m_valid(m_valid),
    .m_ready(m_ready),
    .m_data(m_data),
    .m_tkeep(m_tkeep),
    .m_tlast(m_tlast)
  );

  integer errors = 0;
  integer result_fd;
  string result_path;

  // Accepted sample stream and emitted beat log (max sizes for a scenario).
  logic [7:0] acc [0:8191];
  int acc_n = 0;
  logic [31:0] beats [0:4095];
  logic [3:0]  keeps [0:4095];
  logic        lasts [0:4095];
  int beat_n = 0;

  always #(CLK_PERIOD / 2.0) clk = ~clk;

  // Record events of the previous posedge: acceptance and emission.
  logic prev_s_valid = 1'b0;
  logic prev_m_valid = 1'b0;
  logic [7:0] prev_s_data = 8'h00;

  always @(posedge clk) begin
    if (!srst) begin
      if (s_valid && s_ready) begin
        if (acc_n < 8192) begin
          acc[acc_n] = s_data;
          acc_n = acc_n + 1;
        end
      end
      if (m_valid && m_ready) begin
        if (beat_n < 4096) begin
          beats[beat_n] = m_data;
          keeps[beat_n] = m_tkeep;
          lasts[beat_n] = m_tlast;
          beat_n = beat_n + 1;
        end
      end
    end
  end

  task automatic cycle(input logic drive_valid, input logic [7:0] value,
                       input logic drive_flush, input logic drive_ready);
    @(negedge clk);
    s_valid = drive_valid;
    s_data = value;
    flush_req = drive_flush;
    m_ready = drive_ready;
  endtask

  task automatic scenario_reset();
    srst = 1'b1;
    s_valid = 1'b0;
    flush_req = 1'b0;
    m_ready = 1'b1;
    pkt_words_i = 32'd0;
    repeat (3) @(posedge clk);
    srst = 1'b0;
    @(posedge clk);
    acc_n = 0;
    beat_n = 0;
  endtask

  task automatic drain_packet();
    int guard;
    guard = 0;
    @(negedge clk);
    s_valid = 1'b0;
    flush_req = 1'b0;
    m_ready = 1'b1;
    while (guard < 64) begin
      @(negedge clk);
      if (!m_valid) break;
      guard = guard + 1;
    end
  endtask

  // Structural + reconstruction checks for the accumulated scenario.
  task automatic check_recording(string tag, input logic allow_tail);
    int i;
    int a;
    int b;
    int bytes;
    int words_in_packet;
    logic beat_last;
    // Reconstruct accepted samples byte by byte from beats.
    a = 0;
    b = 0;
    bytes = 0;
    words_in_packet = 0;
    beat_last = 1'b1;
    while (b < beat_n) begin
      begin
        int k;
        for (k = 0; k < 4; k++) begin
          if (keeps[b][k]) begin
            if (a >= acc_n) begin
              $display("FAIL [%s] beat %0d byte %0d exceeds accepted stream",
                       tag, b, k);
              errors = errors + 1;
            end else begin
              if (beats[b][k*8 +: 8] !== acc[a]) begin
                $display("FAIL [%s] beat %0d byte %0d value %02h expected %02h",
                         tag, b, k, beats[b][k*8 +: 8], acc[a]);
                errors = errors + 1;
              end
            end
            a = a + 1;
          end
        end
        if (keeps[b] != 4'hF && !lasts[b]) begin
          $display("FAIL [%s] beat %0d partial tkeep without tlast", tag, b);
          errors = errors + 1;
        end
        if (lasts[b]) begin
          if (words_in_packet >= PACKET_WORDS) begin
            $display("FAIL [%s] packet longer than %0d words", tag,
                     PACKET_WORDS);
            errors = errors + 1;
          end
          words_in_packet = 0;
        end else begin
          words_in_packet = words_in_packet + 1;
        end
        b = b + 1;
      end
    end
    if (a != acc_n && !(allow_tail && acc_n - a <= 3 && acc_n - a >= 0)) begin
      $display("FAIL [%s] emitted %0d bytes, accepted %0d", tag, a, acc_n);
      errors = errors + 1;
    end
    if (acc_n == 0) begin
      $display("FAIL [%s] no samples were accepted (dead input path)", tag);
      errors = errors + 1;
    end
  endtask

  task automatic finish_scenario(string tag, input logic flush_at_end);
    if (flush_at_end) begin
      cycle(1'b0, 8'h00, 1'b1, 1'b1);
    end
    drain_packet();
    check_recording(tag, !flush_at_end);
  endtask

  initial begin
    int k;
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    result_fd = $fopen(result_path, "w");
    if (result_fd == 0) $fatal(1, "result file could not be opened");

    // ----------------------------------------------------------------
    // Scenario 1: continuous stream with word-aligned end (200 samples =
    // 50 full words) and count-based tlast every 16 words.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      for (k = 0; k < 200; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);
      end
      finish_scenario("continuous", 1'b0);
    end

    // ----------------------------------------------------------------
    // Scenario 2: 10 samples then a capture-end flush (partial tail word).
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      for (k = 0; k < 10; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);
      end
      finish_scenario("flush-tail", 1'b1);
    end

    // ----------------------------------------------------------------
    // Scenario 3: flush mid-stream at an aligned boundary defers tlast to
    // the next word; samples continue afterwards.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      for (k = 0; k < 12; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // 3 full words, aligned
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);      // flush at the boundary
      for (k = 12; k < 30; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);
      end
      finish_scenario("deferred-flush", 1'b1);
    end

    // ----------------------------------------------------------------
    // Scenario 4: m_ready stalls (back-pressure) with partial and full
    // groups; stream equality must hold across the stalls.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      for (k = 0; k < 3; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // 3 samples buffered
      end
      cycle(1'b1, 8'h03, 1'b0, 1'b0);      // 4th sample: word pending, no pop
      for (k = 0; k < 7; k++) begin
        logic [7:0] v;
        v = 4 + k;
        cycle(1'b1, v, 1'b0, 1'b0);         // more samples, still stalled
      end
      for (k = 0; k < 20; k++) begin
        logic [7:0] v;
        v = 11 + k;
        cycle(1'b1, v, 1'b0, 1'b1);         // m_ready returns
      end
      finish_scenario("backpressure", 1'b1);
    end

    // ----------------------------------------------------------------
    // Scenario 5: repeated flush with nothing buffered (no empty beats),
    // interleaved with samples; reset behavior at the end.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      cycle(1'b1, 8'hAA, 1'b0, 1'b1);
      cycle(1'b1, 8'hBB, 1'b0, 1'b1);
      cycle(1'b0, 8'h00, 1'b1, 1'b1);       // 2 samples buffered: tail beat
      cycle(1'b0, 8'h00, 1'b1, 1'b1);       // idle flush: nothing pending
      for (k = 0; k < 5; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);
      end
      finish_scenario("noop-flush", 1'b0);  // 5 samples -> tail? 5 % 4 = 1:
                                            // a full word + partial left over;
                                            // no capture flush, so the log
                                            // check happens before the reset.
      if (errors == 0) begin
        // Leave the partial buffered and reset: nothing may be emitted.
        srst = 1'b1;
        @(posedge clk);
        @(posedge clk);
        if (m_valid) begin
          $display("FAIL s5 reset with partial group left a beat pending");
          errors = errors + 1;
        end
        srst = 1'b0;
      end
    end

    // ----------------------------------------------------------------
    // Scenario 6: burst packet count (pkt_words_i = 3) with an aligned end.
    // The third word must carry tlast; the capture-end flush emits nothing.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      pkt_words_i = 32'd3;
      for (k = 0; k < 12; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // 3 full words
      end
      finish_scenario("burst-aligned", 1'b1);
      if (beat_n != 3) begin
        $display("FAIL [burst-aligned] expected 3 beats, got %0d", beat_n);
        errors = errors + 1;
      end
      if (!lasts[2] || lasts[0] || lasts[1]) begin
        $display("FAIL [burst-aligned] tlast not exactly on the last word");
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Scenario 7: burst packet count (pkt_words_i = 4) with a partial tail.
    // The fourth word is the partial beat and must carry tlast and tkeep.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      pkt_words_i = 32'd4;
      for (k = 0; k < 14; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // 3 full words + 2 samples
      end
      finish_scenario("burst-partial", 1'b1);
      if (beat_n != 4) begin
        $display("FAIL [burst-partial] expected 4 beats, got %0d", beat_n);
        errors = errors + 1;
      end
      if (!lasts[3] || keeps[3] != 4'h3 || lasts[0] || lasts[1] || lasts[2]) begin
        $display("FAIL [burst-partial] tail beat/tlast/keep wrong");
        errors = errors + 1;
      end
    end

    // ----------------------------------------------------------------
    // Scenario 8: a live-mode deferred tlast (pkt_words_i = 0) must not leak
    // into the next burst packet; the burst closes on its counted word.
    // ----------------------------------------------------------------
    scenario_reset();
    begin
      for (k = 0; k < 4; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // one full word
      end
      cycle(1'b0, 8'h00, 1'b1, 1'b1);      // live aligned flush: defers tlast
      pkt_words_i = 32'd2;
      for (k = 4; k < 12; k++) begin
        cycle(1'b1, k[7:0], 1'b0, 1'b1);   // two more words
      end
      finish_scenario("live-to-burst", 1'b0);
      if (beat_n != 3) begin
        $display("FAIL [live-to-burst] expected 3 beats, got %0d", beat_n);
        errors = errors + 1;
      end
      if (lasts[0] || lasts[1] || !lasts[2]) begin
        $display("FAIL [live-to-burst] tlast leaked or missing");
        errors = errors + 1;
      end
    end

    if (errors != 0) begin
      $fdisplay(result_fd, "FAIL: tb_la_raw_packer errors=%0d", errors);
      $fclose(result_fd);
      $fatal(1, "tb_la_raw_packer failed with %0d errors", errors);
    end
    $fdisplay(result_fd, "PASS: tb_la_raw_packer");
    $fclose(result_fd);
    $display("PASS: tb_la_raw_packer");
    $finish;
  end

  initial begin
    #2_000_000;
    $fatal(1, "tb_la_raw_packer timeout");
  end

endmodule : tb_la_raw_packer
