// P2.5 la_txn_encoder
//
// Transition encoder producing {run[23:0], sample[7:0]} words (run = number
// of consecutive VALID samples holding `sample`; run in bits [31:8], sample
// in bits [7:0]). Runs are positive counts; zero-length runs are never
// emitted.
//
// Word triggers
//   - value change: the closed run of the old value leaves as one word;
//   - run limit: when a run reaches max_run, the arriving same-value sample
//     starts the next segment, emitting {max_run, sample} first;
//   - packet flush (flush_req or capture end): the pending run leaves with
//     m_last. A flush that closes a run mid-value is legal: consecutive
//     words with the same sample (from splits and timer flushes) never
//     create a false edge for the decoder, which only reacts to value
//     changes. Sample accounting is exact: every accepted sample is counted
//     exactly once across such words.
//
// Flush semantics
//   flush_req executes when the output slot is free. While a word is waiting
//   for m_ready the request is parked in flush_wait_q and the sample input
//   is blocked (s_ready = 0), so no sample ever straddles a packet
//   boundary: the flushed word contains exactly the samples that arrived
//   before the flush request. A flush with no pending run (idle after a
//   flush or before the first sample) emits nothing; the controller does
//   not request empty packets.
//
// Back-pressure
//   Same-value samples with room in the run are absorbed while m_ready is
//   low (the run extends), so steady inputs never stall. A value change or
//   run-limit split requires an output beat: s_ready drops until m_ready,
//   propagating the stall to the pre-trigger ring, whose overflow is never
//   silent. Sampling cannot be paused; capture_ctrl stops the capture on
//   FIFO exhaustion.
//
// Interface
//   Single FCLK, synchronous active-high srst. s_valid/s_ready/s_data is the
//   sample handshake; m_valid/m_ready/m_data/m_last the word handshake;
//   flush_req is a single-cycle pulse.

module la_txn_encoder (
  input  logic        clk,
  input  logic        srst,
  input  logic [23:0] max_run,       // TXN_MAX_RUN, 1..2^24-1
  input  logic        s_valid,
  output logic        s_ready,
  input  logic [7:0]  s_data,
  input  logic        flush_req,
  output logic        m_valid,
  input  logic        m_ready,
  output logic [31:0] m_data,
  output logic        m_last
);

  logic [23:0] run_q;
  logic [7:0]  cur_q;
  logic        cur_valid_q;
  logic        out_valid_q;
  logic [31:0] out_data_q;
  logic        out_last_q;
  logic        flush_wait_q;

  logic push_need;

  assign push_need = cur_valid_q
                   && ((s_data != cur_q) || (run_q == max_run));

  assign s_ready = !flush_req && !flush_wait_q
                 && (!push_need || !out_valid_q || m_ready);
  assign m_valid = out_valid_q;
  assign m_data  = out_data_q;
  assign m_last  = out_last_q;

  always_ff @(posedge clk) begin
    if (srst) begin
      run_q        <= '0;
      cur_q        <= '0;
      cur_valid_q  <= 1'b0;
      out_valid_q  <= 1'b0;
      out_data_q   <= '0;
      out_last_q   <= 1'b0;
      flush_wait_q <= 1'b0;
    end else begin
      // Park a flush request that cannot execute yet (word pending).
      if (flush_req && out_valid_q && !m_ready) flush_wait_q <= 1'b1;

      // State-only transitions that never need the output slot: the first
      // sample of a run and same-value absorption.
      if (s_valid && s_ready && !flush_wait_q) begin
        if (!cur_valid_q) begin
          cur_valid_q <= 1'b1;
          cur_q       <= s_data;
          run_q       <= 24'd1;
        end else if ((s_data == cur_q) && (run_q != max_run)) begin
          run_q <= run_q + 24'd1;
        end
      end

      // Output slot updates. The slot is writable this cycle when it is
      // empty or being popped (m_ready).
      if (!out_valid_q || m_ready) begin
        if (flush_wait_q || flush_req) begin
          // Flush executes: emit the pending run with m_last, or nothing.
          flush_wait_q <= 1'b0;
          if (cur_valid_q) begin
            out_valid_q <= 1'b1;
            out_data_q  <= {run_q, cur_q};
            out_last_q  <= 1'b1;
            cur_valid_q <= 1'b0;
            run_q       <= '0;
          end else begin
            out_valid_q <= 1'b0;
            out_last_q  <= 1'b0;
          end
        end else if (s_valid && s_ready && cur_valid_q
                     && ((s_data != cur_q) || (run_q == max_run))) begin
          // Value change (or run-limit split when s_data == cur_q): the
          // completed run leaves; the arriving sample continues/restarts.
          out_valid_q <= 1'b1;
          out_data_q  <= {run_q, cur_q};
          out_last_q  <= 1'b0;
          cur_q       <= s_data;
          run_q       <= 24'd1;
        end else begin
          if (out_valid_q) out_valid_q <= 1'b0;   // pop without refill
        end
      end
    end
  end

endmodule : la_txn_encoder
