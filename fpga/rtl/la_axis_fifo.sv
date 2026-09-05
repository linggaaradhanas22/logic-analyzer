// P2.8 la_axis_fifo
//
// 32-bit AXIS FIFO with tkeep/tlast transport, count/full status and a
// sticky overflow flag. The nominal depth covers about 82 us of raw
// acquisition at 100 MS/s (2048 words = 8192 samples) or about 20 us of
// worst-case transition words; sampling cannot be paused, so exhaustion
// propagates s_ready = 0 upstream (the encoder stalls and the pre-trigger
// ring overflows, which capture_ctrl stops -- never silent loss).
//
// The write side honors the AXIS handshake: a beat is only stored when
// s_valid && s_ready. If a non-compliant writer drives s_valid while the
// FIFO is full (s_ready = 0), the beat is dropped and `overflow` becomes
// sticky (until srst or the next run edge driven by capture_ctrl), marking
// the gap explicitly instead of corrupting the stream.
//
// Read side: m_valid when words are stored; pop on m_ready. m_data/m_tkeep/
// m_tlast mirror the beat at the read pointer.

module la_axis_fifo #(
  parameter int DEPTH = 2048,
  parameter int LOG2D = 11
) (
  input  logic        clk,
  input  logic        srst,
  input  logic        s_valid,
  output logic        s_ready,
  input  logic [31:0] s_data,
  input  logic [3:0]  s_tkeep,
  input  logic        s_tlast,
  output logic        m_valid,
  input  logic        m_ready,
  output logic [31:0] m_data,
  output logic [3:0]  m_tkeep,
  output logic        m_tlast,
  output logic        full,
  output logic        overflow,   // sticky until srst or overflow_clr
  input  logic        overflow_clr,
  output logic [LOG2D:0] count
);

  initial begin
    if (DEPTH != (1 << LOG2D)) begin
      $error("la_axis_fifo: DEPTH must equal 2**LOG2D");
    end
  end

  logic [31:0] mem_data [DEPTH];
  logic [3:0]  mem_keep [DEPTH];
  logic        mem_last [DEPTH];
  logic [LOG2D-1:0] wptr = '0;
  logic [LOG2D-1:0] rptr = '0;
  logic [LOG2D:0]   cnt = '0;
  logic        ovf_q = 1'b0;

  assign full    = (cnt == DEPTH[LOG2D:0]);
  assign count   = cnt;
  assign s_ready = !full;

  always_ff @(posedge clk) begin
    if (srst) begin
      wptr <= '0;
      rptr <= '0;
      cnt  <= '0;
      ovf_q <= 1'b0;
    end else begin
      if (overflow_clr) ovf_q <= 1'b0;
      if (s_valid && s_ready) begin
        mem_data[wptr] <= s_data;
        mem_keep[wptr] <= s_tkeep;
        mem_last[wptr] <= s_tlast;
        wptr <= wptr + 1'b1;
      end else if (s_valid && full) begin
        ovf_q <= 1'b1;              // dropped beat: protocol violation, marked
      end
      if (m_valid && m_ready) begin
        rptr <= rptr + 1'b1;
      end
      if (s_valid && s_ready && m_valid && m_ready) begin
        cnt <= cnt;
      end else if (s_valid && s_ready) begin
        cnt <= cnt + 1'b1;
      end else if (m_valid && m_ready) begin
        cnt <= cnt - 1'b1;
      end
    end
  end

  assign m_valid = (cnt != '0);
  assign m_data  = mem_data[rptr];
  assign m_tkeep = mem_keep[rptr];
  assign m_tlast = mem_last[rptr];
  assign overflow = ovf_q;

endmodule : la_axis_fifo
