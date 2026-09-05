// P2.2 la_sampler
//
// Divides FCLK to produce one sample every sample_div+1 cycles, so
// f_s = FCLK / (sample_div + 1). sample_div = 0 samples every cycle.
// The 8-bit sample is the synchronized input masked by chan_en: disabled
// channels read zero and therefore never change value, never contribute
// transitions and never participate in triggering downstream.
//
// sample_valid is a single-cycle pulse aligned with sample.


module la_sampler #(
  parameter int CHANNELS = 8
) (
  input  logic                clk,
  input  logic                srst,
  input  logic [CHANNELS-1:0] ch_sync,
  input  logic [31:0]         sample_div,
  input  logic [CHANNELS-1:0] chan_en,
  output logic [7:0]          sample,
  output logic                sample_valid
);

  logic [31:0] div_count;
  logic        tick;

  assign tick = (sample_div == 32'd0) ? 1'b1 : (div_count == 32'd0);

  always_ff @(posedge clk) begin
    if (srst) begin
      div_count <= '0;
    end else begin
      if (div_count == 32'd0) begin
        div_count <= sample_div;
      end else begin
        div_count <= div_count - 32'd1;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (srst) begin
      sample       <= '0;
      sample_valid <= 1'b0;
    end else begin
      sample_valid <= tick;
      if (tick) begin
        sample <= ch_sync & chan_en;
      end
    end
  end

endmodule : la_sampler

