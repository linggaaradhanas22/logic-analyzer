// P2.3 la_trigger
//
// Trigger condition evaluated once per valid sample:
//   level_ok = ((sample ^ trig_value) & trig_mask) == 0
//   rise_ok  = trig_rise == 0 ? 1 : (any rising transition on trig_rise channels)
//   fall_ok  = trig_fall == 0 ? 1 : (any falling transition on trig_fall channels)
//   trig     = sample_valid && level_ok && rise_ok && fall_ok
//
// An empty edge mask (register zero) makes that edge term unconditional,
// matching the P1 register map note "empty edge masks = true". Edge detection
// compares the current sample against the previous VALID sample, so it is
// correct for any sample_div. Because prev initializes to zero, samples before
// the first valid sample pair can never contribute a rising/falling term;
// a pure level trigger (both edge masks zero) is allowed from the first sample.
//
// All inputs are synchronous to clk; trig is a single-cycle pulse aligned
// with sample_valid.


module la_trigger #(
  parameter int CHANNELS = 8
) (
  input  logic                clk,
  input  logic                srst,
  input  logic                sample_valid,
  input  logic [CHANNELS-1:0] sample,
  input  logic [CHANNELS-1:0] trig_mask,
  input  logic [CHANNELS-1:0] trig_value,
  input  logic [CHANNELS-1:0] trig_rise,
  input  logic [CHANNELS-1:0] trig_fall,
  output logic                trig
);

  logic [CHANNELS-1:0] prev;
  logic                prev_valid;

  always_ff @(posedge clk) begin
    if (srst) begin
      prev       <= '0;
      prev_valid <= 1'b0;
    end else if (sample_valid) begin
      prev       <= sample;
      prev_valid <= 1'b1;
    end
  end

  logic level_ok;
  logic rise_ok;
  logic fall_ok;
  logic edge_required;

  assign level_ok = ((sample ^ trig_value) & trig_mask) == '0;
  assign rise_ok  = (trig_rise == '0) ? 1'b1
                  : (|(sample & ~prev & trig_rise));
  assign fall_ok  = (trig_fall == '0) ? 1'b1
                  : (|(~sample & prev & trig_fall));
  assign edge_required = (trig_rise != '0) || (trig_fall != '0);

  assign trig = sample_valid && level_ok
              && (!edge_required || (prev_valid && rise_ok && fall_ok));

endmodule : la_trigger

