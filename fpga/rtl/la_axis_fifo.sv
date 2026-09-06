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
// Read side: m_valid when a beat is presented; pop on m_ready. m_data/
// m_tkeep/m_tlast mirror the beat at the read pointer.
//
// P3 implementation note: the storage is one word-wide BRAM array
// {tlast, tkeep, data} written with per-byte write-enables derived from
// tkeep (partial tail beats) and read into a one-beat output register, so
// synthesis maps it to block RAM with byte-enables instead of the 6000
// distributed-RAM cells that an asynchronous-read FIFO would need. The AXIS
// protocol is unchanged; a beat written into an empty FIFO is presented one
// cycle later than the old asynchronous-read implementation, which only
// matters for latency, never for ordering or back-pressure.

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

  // Word layout: [36]=tlast, [35:32]=tkeep, [31:0]=data. Only bytes
  // selected by tkeep are meaningful after a partial (tail) beat; consumers
  // must use tkeep and never rely on the value of unkept bytes.
  (* ram_style = "block" *) logic [36:0] mem [DEPTH];
  logic [LOG2D-1:0] wptr = '0;
  logic [LOG2D-1:0] rptr = '0;
  logic [LOG2D:0]   cnt = '0;      // beats stored: memory + presented
  logic        o_has = 1'b0;       // output register holds a beat
  logic [36:0] o_word = '0;
  logic        ovf_q = 1'b0;

  logic        pop;
  logic        wr;
  logic [LOG2D:0] mem_pending;

  assign wr     = s_valid && s_ready;
  assign pop    = o_has && m_ready;
  // Beats still in the memory array (presented beat already left it).
  assign mem_pending = cnt - (o_has ? {{LOG2D{1'b0}}, 1'b1} : '0);
  // Load the output register whenever the presented slot is or will become
  // free and the memory still holds a beat. One-cycle read latency: the
  // memory address is registered by the RAM, the data by o_word.
  logic load;
  assign load = (mem_pending != '0) && (!o_has || pop);

  assign full    = (cnt == DEPTH[LOG2D:0]);
  assign count   = cnt;
  assign s_ready = !full;
  assign m_valid = o_has;
  assign m_data  = o_word[31:0];
  assign m_tkeep = o_word[35:32];
  assign m_tlast = o_word[36];
  assign overflow = ovf_q;

  always_ff @(posedge clk) begin
    if (srst) begin
      wptr <= '0;
      rptr <= '0;
      cnt  <= '0;
      o_has <= 1'b0;
      ovf_q <= 1'b0;
    end else begin
      if (overflow_clr) ovf_q <= 1'b0;
      if (wr) begin
        // tkeep-conditioned byte lanes + unconditional tlast/tkeep lane:
        // this is the pattern block RAM byte-enable inference expects.
        mem[wptr][36:32] <= {s_tlast, s_tkeep};
        if (s_tkeep[0]) mem[wptr][7:0]   <= s_data[7:0];
        if (s_tkeep[1]) mem[wptr][15:8]  <= s_data[15:8];
        if (s_tkeep[2]) mem[wptr][23:16] <= s_data[23:16];
        if (s_tkeep[3]) mem[wptr][31:24] <= s_data[31:24];
        wptr <= wptr + 1'b1;
      end else if (s_valid && full) begin
        ovf_q <= 1'b1;              // dropped beat: protocol violation, marked
      end
      if (load) begin
        o_word <= mem[rptr];
        rptr   <= rptr + 1'b1;
        o_has  <= 1'b1;
      end else if (pop) begin
        o_has  <= 1'b0;
      end
      if (wr && pop) begin
        cnt <= cnt;
      end else if (wr) begin
        cnt <= cnt + 1'b1;
      end else if (pop) begin
        cnt <= cnt - 1'b1;
      end
    end
  end

endmodule : la_axis_fifo
