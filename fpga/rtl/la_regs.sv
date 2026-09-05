// P2.10 la_regs
//
// AXI-Lite register slave implementing the P1.1 map (generated constants
// from la_regs_pkg). Behavior:
//   * CTRL[0] ARM, [1] ABORT and [2] SOFT_RESET are self-clearing: a
//     strobe-acknowledged write of 1 produces a single-cycle pulse and the
//     stored bit reads back 0. SELFTEST_EN [3] and STREAM [4] are sticky.
//   * IRQ_STAT is write-1-to-clear; the interrupt output is
//     (IRQ_EN & IRQ_STAT) != 0. External event pulses (irq_st_done/trig/
//     ovf) set the corresponding sticky bits.
//   * Writes apply byte strobes first (un-strobed bytes keep their value),
//     then field masks (MODE, CHAN_EN, TRIG_*, PATGEN_CTRL, IRQ_EN, CTRL)
//     keep only the allowed bits with reserved bits always reading 0;
//     PRE_SAMPLES clamps to its max and TXN_MAX_RUN clamps to
//     [1, 0xFFFFFF]. Out-of-range addresses read 0 and ignore writes
//     (responses stay OKAY).
//   * RO fields (ID, STATUS, TRIG_INDEX, WORD_COUNT_LO/HI) are pass-through
//     inputs (status_i, trig_index_i, word_lo_i, word_hi_i).
//
// Interface: single FCLK, synchronous active-high srst, AXI-Lite slave
// channels with independent write/read handling (the BFM exercises separate
// AW/W arrival and response stalls).

module la_regs (
  input  logic        clk,
  input  logic        srst,
  // write address channel
  input  logic        s_axi_awvalid,
  output logic        s_axi_awready,
  input  logic [31:0] s_axi_awaddr,
  input  logic [2:0]  s_axi_awprot,
  // write data channel
  input  logic        s_axi_wvalid,
  output logic        s_axi_wready,
  input  logic [31:0] s_axi_wdata,
  input  logic [3:0]  s_axi_wstrb,
  // write response channel
  output logic        s_axi_bvalid,
  input  logic        s_axi_bready,
  output logic [1:0]  s_axi_bresp,
  // read address channel
  input  logic        s_axi_arvalid,
  output logic        s_axi_arready,
  input  logic [31:0] s_axi_araddr,
  input  logic [2:0]  s_axi_arprot,
  // read data channel
  output logic        s_axi_rvalid,
  input  logic        s_axi_rready,
  output logic [31:0] s_axi_rdata,
  output logic [1:0]  s_axi_rresp,
  // register outputs
  output logic [31:0] ctrl_o,
  output logic [31:0] mode_o,
  output logic [31:0] sample_div_o,
  output logic [31:0] chan_en_o,
  output logic [31:0] trig_mask_o,
  output logic [31:0] trig_value_o,
  output logic [31:0] trig_rise_o,
  output logic [31:0] trig_fall_o,
  output logic [31:0] pre_samples_o,
  output logic [31:0] post_samples_o,
  output logic [31:0] timeout_o,
  output logic [31:0] txn_max_run_o,
  output logic [31:0] live_flush_o,
  output logic [31:0] patgen_ctrl_o,
  output logic        arm_pulse_o,
  output logic        abort_pulse_o,
  output logic        soft_reset_pulse_o,
  output logic        irq_o,
  // RO pass-through inputs
  input  logic [31:0] status_i,
  input  logic [31:0] trig_index_i,
  input  logic [31:0] word_lo_i,
  input  logic [31:0] word_hi_i,
  // IRQ event pulses
  input  logic        irq_st_done_i,
  input  logic        irq_st_trig_i,
  input  logic        irq_st_ovf_i
);

  import la_regs_pkg::*;

  // ------------------------------------------------------------------
  // Register storage.
  // ------------------------------------------------------------------
  logic [31:0] ctrl_q;
  logic [31:0] mode_q;
  logic [31:0] sample_div_q;
  logic [31:0] chan_en_q;
  logic [31:0] trig_mask_q;
  logic [31:0] trig_value_q;
  logic [31:0] trig_rise_q;
  logic [31:0] trig_fall_q;
  logic [31:0] pre_samples_q;
  logic [31:0] post_samples_q;
  logic [31:0] timeout_q;
  logic [31:0] txn_max_run_q;
  logic [31:0] live_flush_q;
  logic [31:0] patgen_ctrl_q;
  logic [31:0] irq_en_q;
  logic [31:0] irq_stat_q;

  logic arm_pulse_r;
  logic abort_pulse_r;
  logic soft_reset_r;
  logic [31:0] rdata_q;
  logic [31:0] rdata_comb;

  // ------------------------------------------------------------------
  // Write channel state.
  // ------------------------------------------------------------------
  logic        aw_pend;
  logic [31:0] aw_addr;
  logic        w_pend;
  logic [31:0] w_data;
  logic [3:0]  w_strb;
  logic        b_pend;

  // ------------------------------------------------------------------
  // Read channel state.
  // ------------------------------------------------------------------
  logic        ar_pend;
  logic [31:0] ar_addr;
  logic        r_pend;

  assign s_axi_awready = !aw_pend && !b_pend;
  assign s_axi_wready  = !w_pend;
  assign s_axi_arready = !ar_pend && !r_pend;
  assign s_axi_bvalid  = b_pend;
  assign s_axi_bresp   = 2'b00;
  assign s_axi_rvalid  = r_pend;
  assign s_axi_rresp   = 2'b00;
  assign s_axi_rdata   = rdata_q;
  assign irq_o         = (irq_en_q & irq_stat_q) != '0;

  assign ctrl_o          = ctrl_q;
  assign mode_o          = mode_q;
  assign sample_div_o    = sample_div_q;
  assign chan_en_o       = chan_en_q;
  assign trig_mask_o     = trig_mask_q;
  assign trig_value_o    = trig_value_q;
  assign trig_rise_o     = trig_rise_q;
  assign trig_fall_o     = trig_fall_q;
  assign pre_samples_o   = pre_samples_q;
  assign post_samples_o  = post_samples_q;
  assign timeout_o       = timeout_q;
  assign txn_max_run_o   = txn_max_run_q;
  assign live_flush_o    = live_flush_q;
  assign patgen_ctrl_o   = patgen_ctrl_q;
  assign arm_pulse_o        = arm_pulse_r;
  assign abort_pulse_o      = abort_pulse_r;
  assign soft_reset_pulse_o = soft_reset_r;

  // Strobed write application. Un-strobed bytes keep their value; bits
  // outside `keep` are cleared (reserved fields always read 0).
  function automatic logic [31:0] apply_wr(input logic [31:0] old,
                                           input logic [31:0] wdata,
                                           input logic [3:0]  strb,
                                           input logic [31:0] keep);
    logic [31:0] byte_sel;
    logic [31:0] byte_masked;
    logic [31:0] tmp;
    byte_sel = {8{strb[3]}} << 24 | {8{strb[2]}} << 16
             | {8{strb[1]}} << 8  | {8{strb[0]}};
    byte_masked = '0;
    for (int b = 0; b < 4; b++) begin
      if (strb[b]) byte_masked[b*8 +: 8] = wdata[b*8 +: 8];
    end
    tmp = (old & ~byte_sel) | byte_masked;
    apply_wr = tmp & keep;
  endfunction

  function automatic logic [31:0] clamp_unsigned(input logic [31:0] v,
                                                 input logic [31:0] lo,
                                                 input logic [31:0] hi);
    clamp_unsigned = (v < lo) ? lo : ((v > hi) ? hi : v);
  endfunction

  always_ff @(posedge clk) begin
    if (srst) begin
      ctrl_q         <= LA_REG_CTRL_RESET;
      mode_q         <= LA_REG_MODE_RESET;
      sample_div_q   <= LA_REG_SAMPLE_DIV_RESET;
      chan_en_q      <= LA_REG_CHAN_EN_RESET;
      trig_mask_q    <= LA_REG_TRIG_MASK_RESET;
      trig_value_q   <= LA_REG_TRIG_VALUE_RESET;
      trig_rise_q    <= LA_REG_TRIG_RISE_RESET;
      trig_fall_q    <= LA_REG_TRIG_FALL_RESET;
      pre_samples_q  <= LA_REG_PRE_SAMPLES_RESET;
      post_samples_q <= LA_REG_POST_SAMPLES_RESET;
      timeout_q      <= LA_REG_TIMEOUT_RESET;
      txn_max_run_q  <= LA_REG_TXN_MAX_RUN_RESET;
      live_flush_q   <= LA_REG_LIVE_FLUSH_CYCLES_RESET;
      patgen_ctrl_q  <= LA_REG_PATGEN_CTRL_RESET;
      irq_en_q       <= LA_REG_IRQ_EN_RESET;
      irq_stat_q     <= LA_REG_IRQ_STAT_RESET;
      arm_pulse_r    <= 1'b0;
      abort_pulse_r  <= 1'b0;
      soft_reset_r   <= 1'b0;
      aw_pend        <= 1'b0;
      w_pend         <= 1'b0;
      b_pend         <= 1'b0;
      aw_addr        <= '0;
      w_data         <= '0;
      w_strb         <= '0;
      ar_pend        <= 1'b0;
      r_pend         <= 1'b0;
      ar_addr        <= '0;
      rdata_q        <= '0;
    end else begin
      // ---- Write channel ----
      arm_pulse_r   <= 1'b0;
      abort_pulse_r <= 1'b0;
      soft_reset_r  <= 1'b0;
      if (aw_pend && w_pend) begin
        logic [31:0] nv;
        // Both halves available: apply the write and respond.
        case (aw_addr[15:0])
          LA_REG_CTRL_OFFSET[15:0]: begin
            nv = apply_wr(ctrl_q, w_data, w_strb,
                 LA_REG_CTRL_SELFTEST_EN_MASK | LA_REG_CTRL_STREAM_MASK);
            // Self-clearing command bits pulse and never store.
            if (w_strb[0]) begin
              arm_pulse_r   <= w_data[0];
              abort_pulse_r <= w_data[1];
              soft_reset_r  <= w_data[2];
            end
            ctrl_q <= nv & ~32'h00000007;
          end
          LA_REG_MODE_OFFSET[15:0]:
            mode_q <= apply_wr(mode_q, w_data, w_strb, LA_REG_MODE_ALLOWED_MASK);
          LA_REG_SAMPLE_DIV_OFFSET[15:0]:
            sample_div_q <= apply_wr(sample_div_q, w_data, w_strb, 32'hFFFFFFFF);
          LA_REG_CHAN_EN_OFFSET[15:0]:
            chan_en_q <= apply_wr(chan_en_q, w_data, w_strb, LA_REG_CHAN_EN_ALLOWED_MASK);
          LA_REG_TRIG_MASK_OFFSET[15:0]:
            trig_mask_q <= apply_wr(trig_mask_q, w_data, w_strb, LA_REG_TRIG_MASK_ALLOWED_MASK);
          LA_REG_TRIG_VALUE_OFFSET[15:0]:
            trig_value_q <= apply_wr(trig_value_q, w_data, w_strb, LA_REG_TRIG_VALUE_ALLOWED_MASK);
          LA_REG_TRIG_RISE_OFFSET[15:0]:
            trig_rise_q <= apply_wr(trig_rise_q, w_data, w_strb, LA_REG_TRIG_RISE_ALLOWED_MASK);
          LA_REG_TRIG_FALL_OFFSET[15:0]:
            trig_fall_q <= apply_wr(trig_fall_q, w_data, w_strb, LA_REG_TRIG_FALL_ALLOWED_MASK);
          LA_REG_PRE_SAMPLES_OFFSET[15:0]: begin
            nv = apply_wr(pre_samples_q, w_data, w_strb, 32'hFFFFFFFF);
            pre_samples_q <= clamp_unsigned(nv, LA_REG_PRE_SAMPLES_MIN,
                                                 LA_REG_PRE_SAMPLES_MAX);
          end
          LA_REG_POST_SAMPLES_OFFSET[15:0]:
            post_samples_q <= apply_wr(post_samples_q, w_data, w_strb, 32'hFFFFFFFF);
          LA_REG_TIMEOUT_OFFSET[15:0]:
            timeout_q <= apply_wr(timeout_q, w_data, w_strb, 32'hFFFFFFFF);
          LA_REG_TXN_MAX_RUN_OFFSET[15:0]: begin
            nv = apply_wr(txn_max_run_q, w_data, w_strb, 32'hFFFFFFFF);
            txn_max_run_q <= clamp_unsigned(nv, LA_REG_TXN_MAX_RUN_MIN,
                                                 LA_REG_TXN_MAX_RUN_MAX);
          end
          LA_REG_LIVE_FLUSH_CYCLES_OFFSET[15:0]:
            live_flush_q <= apply_wr(live_flush_q, w_data, w_strb, 32'hFFFFFFFF);
          LA_REG_PATGEN_CTRL_OFFSET[15:0]:
            patgen_ctrl_q <= apply_wr(patgen_ctrl_q, w_data, w_strb,
                                      LA_REG_PATGEN_CTRL_ALLOWED_MASK);
          LA_REG_IRQ_EN_OFFSET[15:0]:
            irq_en_q <= apply_wr(irq_en_q, w_data, w_strb, LA_REG_IRQ_EN_ALLOWED_MASK);
          LA_REG_IRQ_STAT_OFFSET[15:0]: begin
            // W1C on the strobe-masked data.
            nv = '0;
            for (int b = 0; b < 4; b++) begin
              if (w_strb[b]) nv[b*8 +: 8] = w_data[b*8 +: 8];
            end
            irq_stat_q <= irq_stat_q & ~(nv & LA_REG_IRQ_STAT_ALLOWED_MASK);
          end
          default: ;   // out of range: ignore
        endcase
        aw_pend <= 1'b0;
        w_pend  <= 1'b0;
        b_pend  <= 1'b1;
      end
      if (!aw_pend && !b_pend && s_axi_awvalid) begin
        aw_pend <= 1'b1;
        aw_addr <= s_axi_awaddr;
      end
      if (!w_pend && s_axi_wvalid) begin
        w_pend  <= 1'b1;
        w_data  <= s_axi_wdata;
        w_strb  <= s_axi_wstrb;
      end
      if (b_pend && s_axi_bready) b_pend <= 1'b0;

      // ---- Read channel ----
      if (!ar_pend && !r_pend && s_axi_arvalid) begin
        ar_pend <= 1'b1;
        ar_addr <= s_axi_araddr;
      end
      if (ar_pend) begin
        r_pend  <= 1'b1;
        ar_pend <= 1'b0;
        rdata_q <= rdata_comb;
      end
      if (r_pend && s_axi_rready) r_pend <= 1'b0;

      // ---- IRQ events (bit indexes follow the generated masks) ----
      if (irq_st_done_i) irq_stat_q[$clog2(LA_REG_IRQ_STAT_DONE_MASK)] <= 1'b1;
      if (irq_st_trig_i) irq_stat_q[$clog2(LA_REG_IRQ_STAT_TRIGGERED_MASK)] <= 1'b1;
      if (irq_st_ovf_i)  irq_stat_q[$clog2(LA_REG_IRQ_STAT_OVERFLOW_MASK)] <= 1'b1;
    end
  end

  // ------------------------------------------------------------------
  // Read mux (combinational over the latched address).
  // ------------------------------------------------------------------
  always_comb begin
    rdata_comb = '0;
    case (ar_addr[15:0])
      LA_REG_ID_OFFSET[15:0]:         rdata_comb = LA_REG_ID_VALUE;
      LA_REG_CTRL_OFFSET[15:0]:       rdata_comb = ctrl_q;
      LA_REG_STATUS_OFFSET[15:0]:     rdata_comb = status_i;
      LA_REG_SAMPLE_DIV_OFFSET[15:0]: rdata_comb = sample_div_q;
      LA_REG_MODE_OFFSET[15:0]:       rdata_comb = mode_q;
      LA_REG_CHAN_EN_OFFSET[15:0]:    rdata_comb = chan_en_q;
      LA_REG_TRIG_MASK_OFFSET[15:0]:  rdata_comb = trig_mask_q;
      LA_REG_TRIG_VALUE_OFFSET[15:0]: rdata_comb = trig_value_q;
      LA_REG_TRIG_RISE_OFFSET[15:0]:  rdata_comb = trig_rise_q;
      LA_REG_TRIG_FALL_OFFSET[15:0]:  rdata_comb = trig_fall_q;
      LA_REG_PRE_SAMPLES_OFFSET[15:0]: rdata_comb = pre_samples_q;
      LA_REG_POST_SAMPLES_OFFSET[15:0]: rdata_comb = post_samples_q;
      LA_REG_TRIG_INDEX_OFFSET[15:0]: rdata_comb = trig_index_i;
      LA_REG_WORD_COUNT_LO_OFFSET[15:0]: rdata_comb = word_lo_i;
      LA_REG_WORD_COUNT_HI_OFFSET[15:0]: rdata_comb = word_hi_i;
      LA_REG_IRQ_EN_OFFSET[15:0]:     rdata_comb = irq_en_q;
      LA_REG_IRQ_STAT_OFFSET[15:0]:   rdata_comb = irq_stat_q;
      LA_REG_PATGEN_CTRL_OFFSET[15:0]: rdata_comb = patgen_ctrl_q;
      LA_REG_TIMEOUT_OFFSET[15:0]:    rdata_comb = timeout_q;
      LA_REG_TXN_MAX_RUN_OFFSET[15:0]: rdata_comb = txn_max_run_q;
      LA_REG_LIVE_FLUSH_CYCLES_OFFSET[15:0]: rdata_comb = live_flush_q;
      default: rdata_comb = '0;
    endcase
  end

endmodule : la_regs
