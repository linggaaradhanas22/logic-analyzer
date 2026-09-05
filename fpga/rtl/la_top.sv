// P2.11 la_top
//
// Top-level integration of the acquisition datapath:
//
//   ch_in[7:0] -> (SELFTEST mux) -> la_input_sync -> la_sampler ->+
//   la_patgen ------------------------------------------------------+-> ring
//                                                                      -> trigger
//   la_pretrig_ring -> la_txn_encoder | la_raw_packer (mode mux)
//                   -> la_axis_fifo -> m_axis (to AXI DMA later)
//   la_capture_ctrl coordinates; la_regs provides the AXI-Lite map;
//   STATUS composition and IRQ events live here; SOFT_RESET and srst
//   reset everything.
//
// Configuration comes from the register block: SAMPLE_DIV, CHAN_EN,
// TRIG_*, MODE (encoding), CTRL (SELFTEST_EN, STREAM), PRE/POST_SAMPLES,
// TIMEOUT, TXN_MAX_RUN, LIVE_FLUSH_CYCLES and PATGEN_CTRL. In STREAM mode
// the ring's pre_req is forced to zero (pass-through). Registers are
// latched by capture_ctrl at the ARM edge; other blocks sample their
// configuration combinationally.
//
// Status register composition (P1.1):
//   [0] IDLE      no capture active and no sticky result
//   [1] ARMED     waiting for the trigger (burst)
//   [2] TRIGGERED sticky trigger event
//   [3] DONE      sticky completion (burst or overflow drain)
//   [4] FIFO_OVF  sticky overflow stop
//   [5] ABORTED   sticky abort/timeout
//   [6] STREAMING live capture running
// Sticky bits clear at the next ARM (or reset).

module la_top (
  input  logic        clk,
  input  logic        srst,
  input  logic [7:0]  ch_in,
  // AXIS word stream out (to the AXI DMA S2MM path)
  output logic        m_axis_tvalid,
  input  logic        m_axis_tready,
  output logic [31:0] m_axis_tdata,
  output logic [3:0]  m_axis_tkeep,
  output logic        m_axis_tlast,
  // AXI-Lite register interface
  input  logic        s_axi_awvalid,
  output logic        s_axi_awready,
  input  logic [31:0] s_axi_awaddr,
  input  logic [2:0]  s_axi_awprot,
  input  logic        s_axi_wvalid,
  output logic        s_axi_wready,
  input  logic [31:0] s_axi_wdata,
  input  logic [3:0]  s_axi_wstrb,
  output logic        s_axi_bvalid,
  input  logic        s_axi_bready,
  output logic [1:0]  s_axi_bresp,
  input  logic        s_axi_arvalid,
  output logic        s_axi_arready,
  input  logic [31:0] s_axi_araddr,
  input  logic [2:0]  s_axi_arprot,
  output logic        s_axi_rvalid,
  input  logic        s_axi_rready,
  output logic [31:0] s_axi_rdata,
  output logic [1:0]  s_axi_rresp,
  output logic        irq_o
);

  import la_regs_pkg::*;

  // Internal nets (declared together so connection order is irrelevant).
  logic int_rst;
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
  logic arm_pulse;
  logic abort_pulse;
  logic soft_reset_pulse;
  logic selftest_en;
  logic stream_en;
  logic txn_mode;
  logic [7:0] ch_src;
  logic [7:0] ch_sync;
  logic [7:0] sample;
  logic       sample_valid;
  logic       trig;
  logic done_l;
  logic trig_l;
  logic aborted_l;
  logic ovf_l;
  logic [31:0] status_word;
  logic [7:0] patgen_ch;
  logic ring_run;
  logic ring_out_valid;
  logic ring_out_ready;
  logic [7:0] ring_out_sample;
  logic [15:0] ring_pre_len;
  logic [31:0] ring_trig_index;
  logic ring_overflow;
  logic enc_s_valid;
  logic enc_s_ready;
  logic enc_m_valid;
  logic enc_m_ready;
  logic [31:0] enc_m_data;
  logic enc_m_last;
  logic enc_flush;
  logic pck_s_valid;
  logic pck_s_ready;
  logic pck_m_valid;
  logic pck_m_ready;
  logic [31:0] pck_m_data;
  logic [3:0]  pck_m_keep;
  logic pck_m_last;
  logic pck_flush;
  logic use_enc;
  logic fifo_s_valid;
  logic fifo_s_ready;
  logic [31:0] fifo_s_data;
  logic [3:0]  fifo_s_keep;
  logic fifo_s_last;
  logic fifo_overflow;
  logic flush_req;
  logic ev_triggered;
  logic ev_done;
  logic ev_aborted;
  logic ev_overflow;
  logic armed;
  logic streaming;
  logic [31:0] word_count_lo;
  logic [31:0] word_count_hi;

  assign int_rst = srst || soft_reset_pulse;
  assign selftest_en = ctrl_q[$clog2(LA_REG_CTRL_SELFTEST_EN_MASK)];
  assign stream_en   = ctrl_q[$clog2(LA_REG_CTRL_STREAM_MASK)];
  assign txn_mode    = mode_q[$clog2(LA_REG_MODE_ENCODING_MASK)];
  assign ch_src = selftest_en ? patgen_ch : ch_in;

  la_input_sync #(
    .CHANNELS(8),
    .MAJORITY(1'b0)
  ) u_sync (
    .clk(clk), .srst(int_rst), .ch_in(ch_src), .ch_sync(ch_sync)
  );

  la_sampler #(
    .CHANNELS(8)
  ) u_sampler (
    .clk(clk), .srst(int_rst),
    .ch_sync(ch_sync),
    .sample_div(sample_div_q),
    .chan_en(chan_en_q[7:0]),
    .sample(sample),
    .sample_valid(sample_valid)
  );

  la_trigger #(
    .CHANNELS(8)
  ) u_trigger (
    .clk(clk), .srst(int_rst),
    .sample_valid(sample_valid),
    .sample(sample),
    .trig_mask(trig_mask_q[7:0]),
    .trig_value(trig_value_q[7:0]),
    .trig_rise(trig_rise_q[7:0]),
    .trig_fall(trig_fall_q[7:0]),
    .trig(trig)
  );

  // ------------------------------------------------------------------
  // Pattern generator (JE outputs; wired to the selftest mux).
  // ------------------------------------------------------------------
  la_patgen u_patgen (
    .clk(clk), .srst(int_rst),
    .en_i(patgen_ctrl_q[$clog2(LA_REG_PATGEN_CTRL_EN_MASK)]),
    .mode_i(patgen_ctrl_q[7:4]),
    .div_i(patgen_ctrl_q[31:16]),
    .ch_o(patgen_ch)
  );

  // ------------------------------------------------------------------
  // Ring (history + window replay).
  // ------------------------------------------------------------------

  la_pretrig_ring u_ring (
    .clk(clk), .srst(int_rst),
    .run(ring_run),
    .in_valid(sample_valid),
    .in_sample(sample),
    .trig(trig),
    .pre_req(stream_en ? 16'd0 : pre_samples_q[15:0]),
    .out_ready(ring_out_ready),
    .out_valid(ring_out_valid),
    .out_sample(ring_out_sample),
    .pre_len(ring_pre_len),
    .trig_index(ring_trig_index),
    .overflow(ring_overflow)
  );

  // ------------------------------------------------------------------
  // Encoder / packer (mode mux).
  // ------------------------------------------------------------------

  la_txn_encoder u_encoder (
    .clk(clk), .srst(int_rst),
    .max_run(txn_max_run_q[23:0]),
    .s_valid(enc_s_valid),
    .s_ready(enc_s_ready),
    .s_data(ring_out_sample),
    .flush_req(enc_flush),
    .m_valid(enc_m_valid),
    .m_ready(enc_m_ready),
    .m_data(enc_m_data),
    .m_last(enc_m_last)
  );


  la_raw_packer u_packer (
    .clk(clk), .srst(int_rst),
    .s_valid(pck_s_valid),
    .s_ready(pck_s_ready),
    .s_data(ring_out_sample),
    .flush_req(pck_flush),
    .m_valid(pck_m_valid),
    .m_ready(pck_m_ready),
    .m_data(pck_m_data),
    .m_tkeep(pck_m_keep),
    .m_tlast(pck_m_last)
  );

  // Active stage selection.
  assign use_enc = txn_mode;

  assign enc_s_valid = ring_out_valid && use_enc;
  assign pck_s_valid = ring_out_valid && !use_enc;
  assign ring_out_ready = use_enc ? enc_s_ready : pck_s_ready;
  assign enc_flush = flush_req && use_enc;
  assign pck_flush = flush_req && !use_enc;

  // ------------------------------------------------------------------
  // FIFO (to the AXIS output).
  // ------------------------------------------------------------------

  assign fifo_s_valid = use_enc ? enc_m_valid : pck_m_valid;
  assign enc_m_ready  = use_enc ? fifo_s_ready : 1'b0;
  assign pck_m_ready  = !use_enc ? fifo_s_ready : 1'b0;
  assign fifo_s_data  = use_enc ? enc_m_data : pck_m_data;
  assign fifo_s_keep  = use_enc ? 4'hF : pck_m_keep;
  assign fifo_s_last  = use_enc ? enc_m_last : pck_m_last;

  la_axis_fifo u_fifo (
    .clk(clk), .srst(int_rst),
    .s_valid(fifo_s_valid),
    .s_ready(fifo_s_ready),
    .s_data(fifo_s_data),
    .s_tkeep(fifo_s_keep),
    .s_tlast(fifo_s_last),
    .m_valid(m_axis_tvalid),
    .m_ready(m_axis_tready),
    .m_data(m_axis_tdata),
    .m_tkeep(m_axis_tkeep),
    .m_tlast(m_axis_tlast),
    .overflow(fifo_overflow),
    .overflow_clr(arm_pulse),
    .full(),
    .count()
  );

  // ------------------------------------------------------------------
  // Capture controller.
  // ------------------------------------------------------------------

  la_capture_ctrl u_ctrl (
    .clk(clk), .srst(int_rst),
    .arm_i(arm_pulse),
    .abort_i(abort_pulse),
    .mode_enc_i(txn_mode),
    .stream_en_i(stream_en),
    .pre_req_i(stream_en ? 16'd0 : pre_samples_q[15:0]),
    .post_cnt_i(post_samples_q),
    .timeout_i(timeout_q),
    .live_flush_cycles_i(live_flush_q),
    .txn_max_run_i(txn_max_run_q[23:0]),
    .sample_valid_i(sample_valid),
    .trig_i(trig),
    .ring_run_o(ring_run),
    .ring_overflow_i(ring_overflow),
    .sample_emit_i(ring_out_valid && ring_out_ready),
    .word_valid_i(fifo_s_valid),
    .word_ready_i(fifo_s_ready),
    .word_last_i(fifo_s_last),
    .flush_o(flush_req),
    .fifo_ovf_i(fifo_overflow),
    .ev_triggered_o(ev_triggered),
    .ev_done_o(ev_done),
    .ev_aborted_o(ev_aborted),
    .ev_overflow_o(ev_overflow),
    .armed_o(armed),
    .streaming_o(streaming),
    .word_count_lo_o(word_count_lo),
    .word_count_hi_o(word_count_hi),
    .txn_mode_o(),
    .txn_max_run_o()
  );

  // ------------------------------------------------------------------
  // Status composition and IRQ events.
  // ------------------------------------------------------------------
  assign status_word[0] = !armed && !streaming && !done_l && !trig_l
                        && !aborted_l && !ovf_l;
  assign status_word[1] = armed;
  assign status_word[2] = trig_l;
  assign status_word[3] = done_l;
  assign status_word[4] = ovf_l;
  assign status_word[5] = aborted_l;
  assign status_word[6] = streaming;
  assign status_word[31:7] = '0;

  always_ff @(posedge clk) begin
    if (int_rst) begin
      done_l    <= 1'b0;
      trig_l    <= 1'b0;
      aborted_l <= 1'b0;
      ovf_l     <= 1'b0;
    end else if (arm_pulse) begin
      done_l    <= 1'b0;
      trig_l    <= 1'b0;
      aborted_l <= 1'b0;
      ovf_l     <= 1'b0;
    end else begin
      if (ev_done)     done_l    <= 1'b1;
      if (ev_triggered) trig_l   <= 1'b1;
      if (ev_aborted)  aborted_l <= 1'b1;
      if (ev_overflow) ovf_l     <= 1'b1;
    end
  end

  // ------------------------------------------------------------------
  // Register block.
  // ------------------------------------------------------------------
  la_regs u_regs (
    .clk(clk), .srst(int_rst),
    .s_axi_awvalid(s_axi_awvalid), .s_axi_awready(s_axi_awready),
    .s_axi_awaddr(s_axi_awaddr), .s_axi_awprot(s_axi_awprot),
    .s_axi_wvalid(s_axi_wvalid), .s_axi_wready(s_axi_wready),
    .s_axi_wdata(s_axi_wdata), .s_axi_wstrb(s_axi_wstrb),
    .s_axi_bvalid(s_axi_bvalid), .s_axi_bready(s_axi_bready),
    .s_axi_bresp(s_axi_bresp),
    .s_axi_arvalid(s_axi_arvalid), .s_axi_arready(s_axi_arready),
    .s_axi_araddr(s_axi_araddr), .s_axi_arprot(s_axi_arprot),
    .s_axi_rvalid(s_axi_rvalid), .s_axi_rready(s_axi_rready),
    .s_axi_rdata(s_axi_rdata), .s_axi_rresp(s_axi_rresp),
    .ctrl_o(ctrl_q), .mode_o(mode_q), .sample_div_o(sample_div_q),
    .chan_en_o(chan_en_q), .trig_mask_o(trig_mask_q),
    .trig_value_o(trig_value_q), .trig_rise_o(trig_rise_q),
    .trig_fall_o(trig_fall_q), .pre_samples_o(pre_samples_q),
    .post_samples_o(post_samples_q), .timeout_o(timeout_q),
    .txn_max_run_o(txn_max_run_q), .live_flush_o(live_flush_q),
    .patgen_ctrl_o(patgen_ctrl_q),
    .arm_pulse_o(arm_pulse), .abort_pulse_o(abort_pulse),
    .soft_reset_pulse_o(soft_reset_pulse), .irq_o(irq_o),
    .status_i(status_word),
    .trig_index_i(ring_trig_index),
    .word_lo_i(word_count_lo),
    .word_hi_i(word_count_hi),
    .irq_st_done_i(ev_done),
    .irq_st_trig_i(ev_triggered),
    .irq_st_ovf_i(ev_overflow)
  );

endmodule : la_top
