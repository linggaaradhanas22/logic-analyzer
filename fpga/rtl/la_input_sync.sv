// P2.1 la_input_sync
//
// Two-flop synchronizer per input channel. When MAJORITY=1, the synchronized
// value additionally passes a 3-tap 2-of-3 majority filter so single-cycle
// glitches on an otherwise stable line are removed at the cost of one extra
// cycle of latency.
//
// Latency (in FCLK cycles): 2 when MAJORITY=0, 3 when MAJORITY=1.
//
// Timing conventions are documented for the P3 constraint step: only ch_in is
// asynchronous; every other port is synchronous to clk and timing-closed
// internally. ch_sync is the first CDC-safe level for downstream logic.
//
// The first two pipeline stages carry (* async_reg *) so Vivado keeps them in
// adjacent CLB flip-flops (reduced CDC MTBF; P3.2 placement review point).


module la_input_sync #(
  parameter int CHANNELS = 8,
  parameter bit MAJORITY = 1'b0
) (
  input  logic                 clk,
  input  logic                 srst,
  input  logic [CHANNELS-1:0]  ch_in,
  output logic [CHANNELS-1:0]  ch_sync
);

  (* async_reg = "true" *) logic [CHANNELS-1:0] stage1;
  (* async_reg = "true" *) logic [CHANNELS-1:0] stage2;
  logic [CHANNELS-1:0] tap1;
  logic [CHANNELS-1:0] tap2;

  always_ff @(posedge clk) begin
    if (srst) begin
      stage1 <= '0;
      stage2 <= '0;
      tap1   <= '0;
      tap2   <= '0;
    end else begin
      stage1 <= ch_in;
      stage2 <= stage1;
      tap1   <= stage2;
      tap2   <= tap1;
    end
  end

  if (MAJORITY) begin : gen_filter
    // 2-of-3 majority of the three most recent synchronized samples.
    for (genvar g = 0; g < CHANNELS; g++) begin : gen_bit
      always_comb begin
        logic [2:0] vote;
        vote = {stage2[g], tap1[g], tap2[g]};
        ch_sync[g] = (vote[0] & vote[1]) | (vote[1] & vote[2]) | (vote[0] & vote[2]);
      end
    end
  end else begin : gen_nofilter
    assign ch_sync = stage2;
  end

endmodule : la_input_sync

