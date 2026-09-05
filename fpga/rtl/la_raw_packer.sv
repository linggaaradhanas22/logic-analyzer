// P2.6 la_raw_packer
//
// Packs the sample stream 4 samples per 32-bit word. Sample order is
// preserved with the OLDEST sample of the group in bits [7:0]:
//     word = {s[3], s[2], s[1], s[0]}   (s[0] arrived first)
//
// AXIS output carries tkeep (valid bytes of the beat) and tlast. A word is
// emitted when four samples are buffered, or when a flush closes a packet
// with 1..3 buffered samples (tkeep marks the tail), or when the word count
// of the current packet reaches PACKET_WORDS (65536 by default: the 65536th
// word carries tlast and the count restarts). A flush with no partial beat
// defers tlast to the next complete word (at DIV=0 the deferral is at most
// three FCLK cycles), so no empty or retro-tagged beat is ever created.
//
// The beat slot is a single register; m_valid is asserted whenever a beat is
// available and the beat leaves on m_ready. While the slot is busy the
// sample input stalls (s_ready = 0), propagating back-pressure to the
// pre-trigger ring. A flush that arrives while the slot is busy is parked in
// flush_wait_q and executed when the slot empties; samples are blocked in
// between, so a packet boundary never splits sample groups or races a beat.
//
// Interface: single FCLK, synchronous active-high srst. s_valid/s_ready is
// the sample handshake; m_valid/m_ready/m_data/m_tkeep/m_tlast the word
// stream; flush_req is a single-cycle packet-boundary pulse.

module la_raw_packer #(
  parameter int PACKET_WORDS = 65536
) (
  input  logic        clk,
  input  logic        srst,
  input  logic        s_valid,
  output logic        s_ready,
  input  logic [7:0]  s_data,
  input  logic        flush_req,
  output logic        m_valid,
  input  logic        m_ready,
  output logic [31:0] m_data,
  output logic [3:0]  m_tkeep,
  output logic        m_tlast
);

  localparam int LAST = PACKET_WORDS - 1;

  logic [7:0]  buf0_q;
  logic [7:0]  buf1_q;
  logic [7:0]  buf2_q;
  logic [1:0]  n_q;              // 0..3 buffered samples
  logic        beat_valid_q;
  logic [31:0] beat_data_q;
  logic [3:0]  beat_keep_q;
  logic        beat_last_q;
  logic        flush_wait_q;
  logic        last_pending_q;
  logic [15:0] words_q;          // words emitted in the current packet

  logic flush_act;
  logic [31:0] partial_data;
  logic [3:0]  partial_keep;

  assign flush_act = flush_wait_q || flush_req;
  // Samples are accepted while no flush is being processed. Buffering
  // (n < 3) never needs the beat slot; the sample that completes a word
  // does, so it waits for the slot to be empty or popped this cycle.
  assign s_ready = !flush_act
                 && ((n_q != 2'd3) || !beat_valid_q || m_ready);

  function automatic logic [3:0] keep_for(input logic [1:0] n);
    case (n)
      2'd1: keep_for = 4'h1;
      2'd2: keep_for = 4'h3;
      2'd3: keep_for = 4'h7;
      default: keep_for = 4'h0;
    endcase
  endfunction

  function automatic logic [31:0] partial_word(input logic [7:0] b0,
                                               input logic [7:0] b1,
                                               input logic [7:0] b2,
                                               input logic [1:0] n);
    case (n)
      2'd1: partial_word = {24'h000000, b0};
      2'd2: partial_word = {16'h0000, b1, b0};
      2'd3: partial_word = {8'h00, b2, b1, b0};
      default: partial_word = 32'h00000000;
    endcase
  endfunction

  always_ff @(posedge clk) begin
    if (srst) begin
      buf0_q         <= '0;
      buf1_q         <= '0;
      buf2_q         <= '0;
      n_q            <= '0;
      beat_valid_q   <= 1'b0;
      beat_data_q    <= '0;
      beat_keep_q    <= '0;
      beat_last_q    <= 1'b0;
      flush_wait_q   <= 1'b0;
      last_pending_q <= 1'b0;
      words_q        <= '0;
    end else begin
      // Park a flush request that cannot execute while the slot is busy.
      if (flush_req && beat_valid_q && !m_ready) flush_wait_q <= 1'b1;

      // Sample accumulation (only while no flush is being processed).
      if (s_valid && s_ready) begin
        case (n_q)
          2'd0: buf0_q <= s_data;
          2'd1: buf1_q <= s_data;
          2'd2: buf2_q <= s_data;
          default: ;
        endcase
        if (n_q != 2'd3) n_q <= n_q + 2'd1;
      end

      // Beat slot: writable when empty or being popped.
      if (!beat_valid_q || m_ready) begin
        if (flush_act) begin
          // Flush executes. A partial beat closes the packet with tkeep
          // and tlast; a word-aligned flush defers tlast to the next word.
          flush_wait_q <= 1'b0;
          if (n_q != 2'd0) begin
            beat_valid_q <= 1'b1;
            beat_data_q  <= partial_word(buf0_q, buf1_q, buf2_q, n_q);
            beat_keep_q  <= keep_for(n_q);
            beat_last_q  <= 1'b1;
            n_q          <= 2'd0;
            words_q      <= '0;
            last_pending_q <= 1'b0;
          end else begin
            last_pending_q <= 1'b1;
            if (beat_valid_q) beat_valid_q <= 1'b0;
          end
        end else if (s_valid && s_ready && (n_q == 2'd3)) begin
          // The 4th buffered sample completes a word.
          beat_valid_q <= 1'b1;
          beat_data_q  <= {s_data, buf2_q, buf1_q, buf0_q};
          beat_keep_q  <= 4'hF;
          beat_last_q  <= last_pending_q || (words_q == LAST[15:0]);
          if (last_pending_q || (words_q == LAST[15:0])) words_q <= '0;
          else words_q <= words_q + 16'd1;
          last_pending_q <= 1'b0;
          n_q <= 2'd0;
        end else begin
          if (beat_valid_q) beat_valid_q <= 1'b0;    // pop without refill
        end
      end
    end
  end

  assign m_valid = beat_valid_q;
  assign m_data  = beat_data_q;
  assign m_tkeep = beat_keep_q;
  assign m_tlast = beat_last_q;

endmodule : la_raw_packer
