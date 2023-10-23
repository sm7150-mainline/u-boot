/*
 * Copyright (c) 2023, Marijn Suijten.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debugcc.h"

static struct debug_mux gcc = {
	.base =	0x100000,
	.size = 0x1f0000,

	.enable_reg = 0x62008,
	.enable_mask = BIT(0),

	.mux_reg = 0x62000,
	.mux_mask = 0x3ff,

	.div_reg = 0x62004,
	.div_mask = 0xf,
	.div_val = 2,

	.xo_div4_reg = 0x4300c,
	.debug_ctl_reg = 0x62038,
	.debug_status_reg = 0x6203c,
};

static struct debug_mux cam_cc = {
	.base = 0xad00000,
	.size = 0x10000,
	.block_name = "cam",

	.enable_reg = 0xd008,
	.enable_mask = BIT(0),

	.mux_reg = 0xd000,
	.mux_mask = 0xff,

	.div_reg = 0xd004,
	.div_mask = 0xf,
	.div_val = 4,
};

static struct debug_mux disp_cc = {
	.base = 0xaf00000,
	.size = 0x20000,
	.block_name = "disp",

	.enable_reg = 0x500c,
	.enable_mask = BIT(0),

	.mux_reg = 0x7000,
	.mux_mask = 0xff,

	.div_reg = 0x5008,
	.div_mask = 0x3,
	.div_val = 4,
};

static struct debug_mux gpu_cc = {
	.base = 0x3d90000,
	.size = 0x9000,
	.block_name = "gpu",

	.enable_reg = 0x1100,
	.enable_mask = BIT(0),

	.mux_reg = 0x1568,
	.mux_mask = 0xff,

	.div_reg = 0x10fc,
	.div_mask = 0x3,
	.div_val = 2,
};

static struct debug_mux npu_cc = {
	.base = 0x9980000,
	.size = 0x10000,
	.block_name = "npu",

	.enable_reg = 0x3008,
	.enable_mask = BIT(0),

	.mux_reg = 0x3000,
	.mux_mask = 0xff,

	.div_reg = 0x3004,
	.div_mask = 0x3,
	.div_val = 2,
};

static struct debug_mux video_cc = {
	.base = 0xabf0000,
	.size = 0x10000,
	.block_name = "video",

	.enable_reg = 0xebc,
	.enable_mask = BIT(0),

	.mux_reg = 0xa4c,
	.mux_mask = 0x3f,

	.div_reg = 0x39c,
	.div_mask = 0x7,
	.div_val = 3,
};

static struct debug_mux mc_cc = {
	.base =	0x90ba000,
	.size = /* 0x54 */ 0x1000,
	.block_name = "mc",

	.measure = measure_mccc,
};

static struct debug_mux apss_cc = {
	.base =	0x182a0000,
	.size = /* 0x1c */ 0x1000,
	.block_name = "apss",

	.enable_reg = 0x0,
	.enable_mask = BIT(0),

	.mux_reg = 0x18,
	.mux_mask = 0x7f << 4,
	.mux_shift = 4,

	.div_reg = 0x18,
	.div_mask = 0xf << 11,
	.div_shift = 11,
	.div_val = 1,
};

static const struct debug_mux *sm8250_blocks[] = {
	&gcc,
	//&cam_cc,
	&disp_cc,
	&gpu_cc,
	&npu_cc,
	//&video_cc,
	&mc_cc,
	&apss_cc,
	NULL,
};

static struct measure_clk sm8250_clocks[] = {
	//{ "cam_cc_bps_ahb_clk", &gcc, 0x55, &cam_cc, 0x18 },
	//{ "cam_cc_bps_areg_clk", &gcc, 0x55, &cam_cc, 0x17 },
	//{ "cam_cc_bps_axi_clk", &gcc, 0x55, &cam_cc, 0x16 },
	//{ "cam_cc_bps_clk", &gcc, 0x55, &cam_cc, 0x14 },
	//{ "cam_cc_camnoc_axi_clk", &gcc, 0x55, &cam_cc, 0x3c },
	//{ "cam_cc_camnoc_dcd_xo_clk", &gcc, 0x55, &cam_cc, 0x3d },
	//{ "cam_cc_cci_0_clk", &gcc, 0x55, &cam_cc, 0x39 },
	//{ "cam_cc_cci_1_clk", &gcc, 0x55, &cam_cc, 0x3a },
	//{ "cam_cc_core_ahb_clk", &gcc, 0x55, &cam_cc, 0x40 },
	//{ "cam_cc_cpas_ahb_clk", &gcc, 0x55, &cam_cc, 0x3b },
	//{ "cam_cc_csi0phytimer_clk", &gcc, 0x55, &cam_cc, 0x8 },
	//{ "cam_cc_csi1phytimer_clk", &gcc, 0x55, &cam_cc, 0xa },
	//{ "cam_cc_csi2phytimer_clk", &gcc, 0x55, &cam_cc, 0xc },
	//{ "cam_cc_csi3phytimer_clk", &gcc, 0x55, &cam_cc, 0xe },
	//{ "cam_cc_csi4phytimer_clk", &gcc, 0x55, &cam_cc, 0x10 },
	//{ "cam_cc_csi5phytimer_clk", &gcc, 0x55, &cam_cc, 0x12 },
	//{ "cam_cc_csiphy0_clk", &gcc, 0x55, &cam_cc, 0x9 },
	//{ "cam_cc_csiphy1_clk", &gcc, 0x55, &cam_cc, 0xb },
	//{ "cam_cc_csiphy2_clk", &gcc, 0x55, &cam_cc, 0xd },
	//{ "cam_cc_csiphy3_clk", &gcc, 0x55, &cam_cc, 0xf },
	//{ "cam_cc_csiphy4_clk", &gcc, 0x55, &cam_cc, 0x11 },
	//{ "cam_cc_csiphy5_clk", &gcc, 0x55, &cam_cc, 0x13 },
	//{ "cam_cc_fd_core_clk", &gcc, 0x55, &cam_cc, 0x37 },
	//{ "cam_cc_fd_core_uar_clk", &gcc, 0x55, &cam_cc, 0x38 },
	//{ "cam_cc_gdsc_clk", &gcc, 0x55, &cam_cc, 0x41 },
	//{ "cam_cc_icp_ahb_clk", &gcc, 0x55, &cam_cc, 0x36 },
	//{ "cam_cc_icp_clk", &gcc, 0x55, &cam_cc, 0x35 },
	//{ "cam_cc_ife_0_ahb_clk", &gcc, 0x55, &cam_cc, 0x26 },
	//{ "cam_cc_ife_0_areg_clk", &gcc, 0x55, &cam_cc, 0x1f },
	//{ "cam_cc_ife_0_axi_clk", &gcc, 0x55, &cam_cc, 0x25 },
	//{ "cam_cc_ife_0_clk", &gcc, 0x55, &cam_cc, 0x1e },
	//{ "cam_cc_ife_0_cphy_rx_clk", &gcc, 0x55, &cam_cc, 0x24 },
	//{ "cam_cc_ife_0_csid_clk", &gcc, 0x55, &cam_cc, 0x22 },
	//{ "cam_cc_ife_0_dsp_clk", &gcc, 0x55, &cam_cc, 0x21 },
	//{ "cam_cc_ife_1_ahb_clk", &gcc, 0x55, &cam_cc, 0x2e },
	//{ "cam_cc_ife_1_areg_clk", &gcc, 0x55, &cam_cc, 0x29 },
	//{ "cam_cc_ife_1_axi_clk", &gcc, 0x55, &cam_cc, 0x2d },
	//{ "cam_cc_ife_1_clk", &gcc, 0x55, &cam_cc, 0x27 },
	//{ "cam_cc_ife_1_cphy_rx_clk", &gcc, 0x55, &cam_cc, 0x2c },
	//{ "cam_cc_ife_1_csid_clk", &gcc, 0x55, &cam_cc, 0x2b },
	//{ "cam_cc_ife_1_dsp_clk", &gcc, 0x55, &cam_cc, 0x2a },
	//{ "cam_cc_ife_lite_ahb_clk", &gcc, 0x55, &cam_cc, 0x32 },
	//{ "cam_cc_ife_lite_axi_clk", &gcc, 0x55, &cam_cc, 0x49 },
	//{ "cam_cc_ife_lite_clk", &gcc, 0x55, &cam_cc, 0x2f },
	//{ "cam_cc_ife_lite_cphy_rx_clk", &gcc, 0x55, &cam_cc, 0x31 },
	//{ "cam_cc_ife_lite_csid_clk", &gcc, 0x55, &cam_cc, 0x30 },
	//{ "cam_cc_ipe_0_ahb_clk", &gcc, 0x55, &cam_cc, 0x1d },
	//{ "cam_cc_ipe_0_areg_clk", &gcc, 0x55, &cam_cc, 0x1c },
	//{ "cam_cc_ipe_0_axi_clk", &gcc, 0x55, &cam_cc, 0x1b },
	//{ "cam_cc_ipe_0_clk", &gcc, 0x55, &cam_cc, 0x19 },
	//{ "cam_cc_jpeg_clk", &gcc, 0x55, &cam_cc, 0x33 },
	//{ "cam_cc_mclk0_clk", &gcc, 0x55, &cam_cc, 0x1 },
	//{ "cam_cc_mclk1_clk", &gcc, 0x55, &cam_cc, 0x2 },
	//{ "cam_cc_mclk2_clk", &gcc, 0x55, &cam_cc, 0x3 },
	//{ "cam_cc_mclk3_clk", &gcc, 0x55, &cam_cc, 0x4 },
	//{ "cam_cc_mclk4_clk", &gcc, 0x55, &cam_cc, 0x5 },
	//{ "cam_cc_mclk5_clk", &gcc, 0x55, &cam_cc, 0x6 },
	//{ "cam_cc_mclk6_clk", &gcc, 0x55, &cam_cc, 0x7 },
	//{ "cam_cc_sbi_ahb_clk", &gcc, 0x55, &cam_cc, 0x4e },
	//{ "cam_cc_sbi_axi_clk", &gcc, 0x55, &cam_cc, 0x4d },
	//{ "cam_cc_sbi_clk", &gcc, 0x55, &cam_cc, 0x4a },
	//{ "cam_cc_sbi_cphy_rx_clk", &gcc, 0x55, &cam_cc, 0x4c },
	//{ "cam_cc_sbi_csid_clk", &gcc, 0x55, &cam_cc, 0x4b },
	//{ "cam_cc_sbi_ife_0_clk", &gcc, 0x55, &cam_cc, 0x4f },
	//{ "cam_cc_sbi_ife_1_clk", &gcc, 0x55, &cam_cc, 0x50 },
	//{ "cam_cc_sleep_clk", &gcc, 0x55, &cam_cc, 0x42 },

	{ "disp_cc_mdss_ahb_clk", &gcc, 0x56, &disp_cc, 0x2b },
	{ "disp_cc_mdss_byte0_clk", &gcc, 0x56, &disp_cc, 0x15 },
	{ "disp_cc_mdss_byte0_intf_clk", &gcc, 0x56, &disp_cc, 0x16 },
	{ "disp_cc_mdss_byte1_clk", &gcc, 0x56, &disp_cc, 0x17 },
	{ "disp_cc_mdss_byte1_intf_clk", &gcc, 0x56, &disp_cc, 0x18 },
	{ "disp_cc_mdss_dp_aux1_clk", &gcc, 0x56, &disp_cc, 0x25 },
	{ "disp_cc_mdss_dp_aux_clk", &gcc, 0x56, &disp_cc, 0x20 },
	{ "disp_cc_mdss_dp_link1_clk", &gcc, 0x56, &disp_cc, 0x22 },
	{ "disp_cc_mdss_dp_link1_intf_clk", &gcc, 0x56, &disp_cc, 0x23 },
	{ "disp_cc_mdss_dp_link_clk", &gcc, 0x56, &disp_cc, 0x1b },
	{ "disp_cc_mdss_dp_link_intf_clk", &gcc, 0x56, &disp_cc, 0x1c },
	{ "disp_cc_mdss_dp_pixel1_clk", &gcc, 0x56, &disp_cc, 0x1f },
	{ "disp_cc_mdss_dp_pixel2_clk", &gcc, 0x56, &disp_cc, 0x21 },
	{ "disp_cc_mdss_dp_pixel_clk", &gcc, 0x56, &disp_cc, 0x1e },
	{ "disp_cc_mdss_edp_aux_clk", &gcc, 0x56, &disp_cc, 0x29 },
	{ "disp_cc_mdss_edp_gtc_clk", &gcc, 0x56, &disp_cc, 0x2a },
	{ "disp_cc_mdss_edp_link_clk", &gcc, 0x56, &disp_cc, 0x27 },
	{ "disp_cc_mdss_edp_link_intf_clk", &gcc, 0x56, &disp_cc, 0x28 },
	{ "disp_cc_mdss_edp_pixel_clk", &gcc, 0x56, &disp_cc, 0x26 },
	{ "disp_cc_mdss_esc0_clk", &gcc, 0x56, &disp_cc, 0x19 },
	{ "disp_cc_mdss_esc1_clk", &gcc, 0x56, &disp_cc, 0x1a },
	{ "disp_cc_mdss_mdp_clk", &gcc, 0x56, &disp_cc, 0x11 },
	{ "disp_cc_mdss_mdp_lut_clk", &gcc, 0x56, &disp_cc, 0x13 },
	{ "disp_cc_mdss_non_gdsc_ahb_clk", &gcc, 0x56, &disp_cc, 0x2c },
	{ "disp_cc_mdss_pclk0_clk", &gcc, 0x56, &disp_cc, 0xf },
	{ "disp_cc_mdss_pclk1_clk", &gcc, 0x56, &disp_cc, 0x10 },
	{ "disp_cc_mdss_rot_clk", &gcc, 0x56, &disp_cc, 0x12 },
	{ "disp_cc_mdss_rscc_ahb_clk", &gcc, 0x56, &disp_cc, 0x2e },
	{ "disp_cc_mdss_rscc_vsync_clk", &gcc, 0x56, &disp_cc, 0x2d },
	{ "disp_cc_mdss_vsync_clk", &gcc, 0x56, &disp_cc, 0x14 },
	{ "disp_cc_sleep_clk", &gcc, 0x56, &disp_cc, 0x37 },
	{ "disp_cc_xo_clk", &gcc, 0x56, &disp_cc, 0x36 },

	// { "apss_cc_debug_mux", &gcc, 0xe7 },
	// { "cam_cc_debug_mux", &gcc, 0x55 },
	// { "disp_cc_debug_mux", &gcc, 0x56 },
	{ "gcc_aggre_noc_pcie_tbu_clk", &gcc, 0x36 },
	{ "gcc_aggre_ufs_card_axi_clk", &gcc, 0x142 },
	{ "gcc_aggre_ufs_phy_axi_clk", &gcc, 0x141 },
	{ "gcc_aggre_usb3_prim_axi_clk", &gcc, 0x13f },
	{ "gcc_aggre_usb3_sec_axi_clk", &gcc, 0x140 },
	{ "gcc_boot_rom_ahb_clk", &gcc, 0xa3 },
	{ "gcc_camera_ahb_clk", &gcc, 0x44 },
	{ "gcc_camera_hf_axi_clk", &gcc, 0x4d },
	{ "gcc_camera_sf_axi_clk", &gcc, 0x4e },
	{ "gcc_camera_xo_clk", &gcc, 0x52 },
	{ "gcc_cfg_noc_usb3_prim_axi_clk", &gcc, 0x21 },
	{ "gcc_cfg_noc_usb3_sec_axi_clk", &gcc, 0x22 },
	{ "gcc_cpuss_ahb_clk", &gcc, 0xe0 },
	{ "gcc_cpuss_dvm_bus_clk", &gcc, 0xe4 },
	{ "gcc_cpuss_rbcpr_clk", &gcc, 0xe1 },
	{ "gcc_ddrss_gpu_axi_clk", &gcc, 0xc4 },
	{ "gcc_ddrss_pcie_sf_tbu_clk", &gcc, 0xc5 },
	{ "gcc_disp_ahb_clk", &gcc, 0x45 },
	{ "gcc_disp_hf_axi_clk", &gcc, 0x4f },
	{ "gcc_disp_sf_axi_clk", &gcc, 0x50 },
	{ "gcc_disp_xo_clk", &gcc, 0x53 },
	{ "gcc_gp1_clk", &gcc, 0xef },
	{ "gcc_gp2_clk", &gcc, 0xf0 },
	{ "gcc_gp3_clk", &gcc, 0xf1 },
	{ "gcc_gpu_cfg_ahb_clk", &gcc, 0x161 },
	{ "gcc_gpu_gpll0_clk_src", &gcc, 0x167 },
	{ "gcc_gpu_gpll0_div_clk_src", &gcc, 0x168 },
	{ "gcc_gpu_memnoc_gfx_clk", &gcc, 0x164 },
	{ "gcc_gpu_snoc_dvm_gfx_clk", &gcc, 0x166 },
	{ "gcc_npu_axi_clk", &gcc, 0x17a },
	{ "gcc_npu_bwmon_axi_clk", &gcc, 0x19a },
	{ "gcc_npu_bwmon_cfg_ahb_clk", &gcc, 0x199 },
	{ "gcc_npu_cfg_ahb_clk", &gcc, 0x179 },
	{ "gcc_npu_dma_clk", &gcc, 0x17b },
	{ "gcc_npu_gpll0_clk_src", &gcc, 0x17e },
	{ "gcc_npu_gpll0_div_clk_src", &gcc, 0x17f },
	{ "gcc_pcie0_phy_refgen_clk", &gcc, 0x103 },
	{ "gcc_pcie1_phy_refgen_clk", &gcc, 0x104 },
	{ "gcc_pcie2_phy_refgen_clk", &gcc, 0x105 },
	{ "gcc_pcie_0_aux_clk", &gcc, 0xf6 },
	{ "gcc_pcie_0_cfg_ahb_clk", &gcc, 0xf5 },
	{ "gcc_pcie_0_mstr_axi_clk", &gcc, 0xf4 },
	{ "gcc_pcie_0_pipe_clk", &gcc, 0xf7 },
	{ "gcc_pcie_0_slv_axi_clk", &gcc, 0xf3 },
	{ "gcc_pcie_0_slv_q2a_axi_clk", &gcc, 0xf2 },
	{ "gcc_pcie_1_aux_clk", &gcc, 0xfe },
	{ "gcc_pcie_1_cfg_ahb_clk", &gcc, 0xfd },
	{ "gcc_pcie_1_mstr_axi_clk", &gcc, 0xfc },
	{ "gcc_pcie_1_pipe_clk", &gcc, 0xff },
	{ "gcc_pcie_1_slv_axi_clk", &gcc, 0xfb },
	{ "gcc_pcie_1_slv_q2a_axi_clk", &gcc, 0xfa },
	{ "gcc_pcie_2_aux_clk", &gcc, 0x191 },
	{ "gcc_pcie_2_cfg_ahb_clk", &gcc, 0x190 },
	{ "gcc_pcie_2_mstr_axi_clk", &gcc, 0x18f },
	{ "gcc_pcie_2_pipe_clk", &gcc, 0x192 },
	{ "gcc_pcie_2_slv_axi_clk", &gcc, 0x18e },
	{ "gcc_pcie_2_slv_q2a_axi_clk", &gcc, 0x18d },
	{ "gcc_pcie_phy_aux_clk", &gcc, 0x102 },
	{ "gcc_pdm2_clk", &gcc, 0x9d },
	{ "gcc_pdm_ahb_clk", &gcc, 0x9b },
	{ "gcc_pdm_xo4_clk", &gcc, 0x9c },
	{ "gcc_prng_ahb_clk", &gcc, 0x9e },
	{ "gcc_qmip_camera_nrt_ahb_clk", &gcc, 0x48 },
	{ "gcc_qmip_camera_rt_ahb_clk", &gcc, 0x49 },
	{ "gcc_qmip_disp_ahb_clk", &gcc, 0x4a },
	{ "gcc_qmip_video_cvp_ahb_clk", &gcc, 0x46 },
	{ "gcc_qmip_video_vcodec_ahb_clk", &gcc, 0x47 },
	{ "gcc_qupv3_wrap0_core_2x_clk", &gcc, 0x88 },
	{ "gcc_qupv3_wrap0_core_clk", &gcc, 0x87 },
	{ "gcc_qupv3_wrap0_s0_clk", &gcc, 0x89 },
	{ "gcc_qupv3_wrap0_s1_clk", &gcc, 0x8a },
	{ "gcc_qupv3_wrap0_s2_clk", &gcc, 0x8b },
	{ "gcc_qupv3_wrap0_s3_clk", &gcc, 0x8c },
	{ "gcc_qupv3_wrap0_s4_clk", &gcc, 0x8d },
	{ "gcc_qupv3_wrap0_s5_clk", &gcc, 0x8e },
	{ "gcc_qupv3_wrap0_s6_clk", &gcc, 0x8f },
	{ "gcc_qupv3_wrap0_s7_clk", &gcc, 0x90 },
	{ "gcc_qupv3_wrap1_core_2x_clk", &gcc, 0x94 },
	{ "gcc_qupv3_wrap1_core_clk", &gcc, 0x93 },
	{ "gcc_qupv3_wrap1_s0_clk", &gcc, 0x95 },
	{ "gcc_qupv3_wrap1_s1_clk", &gcc, 0x96 },
	{ "gcc_qupv3_wrap1_s2_clk", &gcc, 0x97 },
	{ "gcc_qupv3_wrap1_s3_clk", &gcc, 0x98 },
	{ "gcc_qupv3_wrap1_s4_clk", &gcc, 0x99 },
	{ "gcc_qupv3_wrap1_s5_clk", &gcc, 0x9a },
	{ "gcc_qupv3_wrap2_core_2x_clk", &gcc, 0x184 },
	{ "gcc_qupv3_wrap2_core_clk", &gcc, 0x183 },
	{ "gcc_qupv3_wrap2_s0_clk", &gcc, 0x185 },
	{ "gcc_qupv3_wrap2_s1_clk", &gcc, 0x186 },
	{ "gcc_qupv3_wrap2_s2_clk", &gcc, 0x187 },
	{ "gcc_qupv3_wrap2_s3_clk", &gcc, 0x188 },
	{ "gcc_qupv3_wrap2_s4_clk", &gcc, 0x189 },
	{ "gcc_qupv3_wrap2_s5_clk", &gcc, 0x18a },
	{ "gcc_qupv3_wrap_0_m_ahb_clk", &gcc, 0x85 },
	{ "gcc_qupv3_wrap_0_s_ahb_clk", &gcc, 0x86 },
	{ "gcc_qupv3_wrap_1_m_ahb_clk", &gcc, 0x91 },
	{ "gcc_qupv3_wrap_1_s_ahb_clk", &gcc, 0x92 },
	{ "gcc_qupv3_wrap_2_m_ahb_clk", &gcc, 0x181 },
	{ "gcc_qupv3_wrap_2_s_ahb_clk", &gcc, 0x182 },
	{ "gcc_sdcc2_ahb_clk", &gcc, 0x82 },
	{ "gcc_sdcc2_apps_clk", &gcc, 0x81 },
	{ "gcc_sdcc4_ahb_clk", &gcc, 0x84 },
	{ "gcc_sdcc4_apps_clk", &gcc, 0x83 },
	{ "gcc_sys_noc_cpuss_ahb_clk", &gcc, 0xc },
	{ "gcc_tsif_ahb_clk", &gcc, 0x9f },
	{ "gcc_tsif_inactivity_timers_clk", &gcc, 0xa1 },
	{ "gcc_tsif_ref_clk", &gcc, 0xa0 },
	{ "gcc_ufs_card_ahb_clk", &gcc, 0x107 },
	{ "gcc_ufs_card_axi_clk", &gcc, 0x106 },
	{ "gcc_ufs_card_ice_core_clk", &gcc, 0x10d },
	{ "gcc_ufs_card_phy_aux_clk", &gcc, 0x10e },
	{ "gcc_ufs_card_rx_symbol_0_clk", &gcc, 0x109 },
	{ "gcc_ufs_card_rx_symbol_1_clk", &gcc, 0x10f },
	{ "gcc_ufs_card_tx_symbol_0_clk", &gcc, 0x108 },
	{ "gcc_ufs_card_unipro_core_clk", &gcc, 0x10c },
	{ "gcc_ufs_phy_ahb_clk", &gcc, 0x113 },
	{ "gcc_ufs_phy_axi_clk", &gcc, 0x112 },
	{ "gcc_ufs_phy_ice_core_clk", &gcc, 0x119 },
	{ "gcc_ufs_phy_phy_aux_clk", &gcc, 0x11a },
	{ "gcc_ufs_phy_rx_symbol_0_clk", &gcc, 0x115 },
	{ "gcc_ufs_phy_rx_symbol_1_clk", &gcc, 0x11b },
	{ "gcc_ufs_phy_tx_symbol_0_clk", &gcc, 0x114 },
	{ "gcc_ufs_phy_unipro_core_clk", &gcc, 0x118 },
	{ "gcc_usb30_prim_master_clk", &gcc, 0x6e },
	{ "gcc_usb30_prim_mock_utmi_clk", &gcc, 0x70 },
	{ "gcc_usb30_prim_sleep_clk", &gcc, 0x6f },
	{ "gcc_usb30_sec_master_clk", &gcc, 0x75 },
	{ "gcc_usb30_sec_mock_utmi_clk", &gcc, 0x77 },
	{ "gcc_usb30_sec_sleep_clk", &gcc, 0x76 },
	{ "gcc_usb3_prim_phy_aux_clk", &gcc, 0x71 },
	{ "gcc_usb3_prim_phy_com_aux_clk", &gcc, 0x72 },
	{ "gcc_usb3_prim_phy_pipe_clk", &gcc, 0x73 },
	{ "gcc_usb3_sec_phy_aux_clk", &gcc, 0x78 },
	{ "gcc_usb3_sec_phy_com_aux_clk", &gcc, 0x79 },
	{ "gcc_usb3_sec_phy_pipe_clk", &gcc, 0x7a },
	{ "gcc_video_ahb_clk", &gcc, 0x43 },
	{ "gcc_video_axi0_clk", &gcc, 0x4b },
	{ "gcc_video_axi1_clk", &gcc, 0x4c },
	{ "gcc_video_xo_clk", &gcc, 0x51 },
	// { "gpu_cc_debug_mux", &gcc, 0x163 },
	// { "mc_cc_debug_mux", &gcc, 0xd1 },
	{ "measure_only_cnoc_clk", &gcc, 0x19 },
	{ "measure_only_ipa_2x_clk", &gcc, 0x147 },
	{ "measure_only_memnoc_clk", &gcc, 0xcc },
	{ "measure_only_snoc_clk", &gcc, 0x7 },
	// { "npu_cc_debug_mux", &gcc, 0x180 },
	// { "video_cc_debug_mux", &gcc, 0x57 },

	{ "gpu_cc_ahb_clk", &gcc, 0x163, &gpu_cc, 0x10 },
	{ "gpu_cc_crc_ahb_clk", &gcc, 0x163, &gpu_cc, 0x11 },
	{ "gpu_cc_cx_apb_clk", &gcc, 0x163, &gpu_cc, 0x14 },
	{ "gpu_cc_cx_gmu_clk", &gcc, 0x163, &gpu_cc, 0x18 },
	{ "gpu_cc_cx_qdss_at_clk", &gcc, 0x163, &gpu_cc, 0x12 },
	{ "gpu_cc_cx_qdss_trig_clk", &gcc, 0x163, &gpu_cc, 0x17 },
	{ "gpu_cc_cx_qdss_tsctr_clk", &gcc, 0x163, &gpu_cc, 0x13 },
	{ "gpu_cc_cx_snoc_dvm_clk", &gcc, 0x163, &gpu_cc, 0x15 },
	{ "gpu_cc_cxo_aon_clk", &gcc, 0x163, &gpu_cc, 0xa },
	{ "gpu_cc_cxo_clk", &gcc, 0x163, &gpu_cc, 0x19 },
	{ "gpu_cc_gx_gmu_clk", &gcc, 0x163, &gpu_cc, 0xf },
	{ "gpu_cc_gx_qdss_tsctr_clk", &gcc, 0x163, &gpu_cc, 0xd },
	{ "gpu_cc_gx_vsense_clk", &gcc, 0x163, &gpu_cc, 0xc },
	{ "gpu_cc_sleep_clk", &gcc, 0x163, &gpu_cc, 0x16 },
	{ "measure_only_gpu_cc_cx_gfx3d_clk", &gcc, 0x163, &gpu_cc, 0x1a },
	{ "measure_only_gpu_cc_cx_gfx3d_slv_clk", &gcc, 0x163, &gpu_cc, 0x1b },
	{ "measure_only_gpu_cc_gx_gfx3d_clk", &gcc, 0x163, &gpu_cc, 0xb },

	{ "npu_cc_atb_clk", &gcc, 0x180, &npu_cc, 0x17 },
	{ "npu_cc_bto_core_clk", &gcc, 0x180, &npu_cc, 0x19 },
	{ "npu_cc_bwmon_clk", &gcc, 0x180, &npu_cc, 0x18 },
	{ "npu_cc_cal_hm0_cdc_clk", &gcc, 0x180, &npu_cc, 0xb },
	{ "npu_cc_cal_hm0_clk", &gcc, 0x180, &npu_cc, 0x2 },
	{ "npu_cc_cal_hm0_dpm_ip_clk", &gcc, 0x180, &npu_cc, 0xc },
	{ "npu_cc_cal_hm0_perf_cnt_clk", &gcc, 0x180, &npu_cc, 0xd },
	{ "npu_cc_cal_hm1_cdc_clk", &gcc, 0x180, &npu_cc, 0xe },
	{ "npu_cc_cal_hm1_clk", &gcc, 0x180, &npu_cc, 0x3 },
	{ "npu_cc_cal_hm1_dpm_ip_clk", &gcc, 0x180, &npu_cc, 0xf },
	{ "npu_cc_cal_hm1_perf_cnt_clk", &gcc, 0x180, &npu_cc, 0x10 },
	{ "npu_cc_core_clk", &gcc, 0x180, &npu_cc, 0x4 },
	{ "npu_cc_dl_dpm_clk", &gcc, 0x180, &npu_cc, 0x23 },
	{ "npu_cc_dl_llm_clk", &gcc, 0x180, &npu_cc, 0x22 },
	{ "npu_cc_dpm_clk", &gcc, 0x180, &npu_cc, 0x8 },
	{ "npu_cc_dpm_temp_clk", &gcc, 0x180, &npu_cc, 0x14 },
	{ "npu_cc_dpm_xo_clk", &gcc, 0x180, &npu_cc, 0xa },
	{ "npu_cc_dsp_ahbm_clk", &gcc, 0x180, &npu_cc, 0x1c },
	{ "npu_cc_dsp_ahbs_clk", &gcc, 0x180, &npu_cc, 0x1b },
	{ "npu_cc_dsp_axi_clk", &gcc, 0x180, &npu_cc, 0x1e },
	{ "npu_cc_dsp_bwmon_ahb_clk", &gcc, 0x180, &npu_cc, 0x1d },
	{ "npu_cc_dsp_bwmon_clk", &gcc, 0x180, &npu_cc, 0x1f },
	{ "npu_cc_isense_clk", &gcc, 0x180, &npu_cc, 0x7 },
	{ "npu_cc_llm_clk", &gcc, 0x180, &npu_cc, 0x6 },
	{ "npu_cc_llm_curr_clk", &gcc, 0x180, &npu_cc, 0x21 },
	{ "npu_cc_llm_temp_clk", &gcc, 0x180, &npu_cc, 0x15 },
	{ "npu_cc_llm_xo_clk", &gcc, 0x180, &npu_cc, 0x9 },
	{ "npu_cc_noc_ahb_clk", &gcc, 0x180, &npu_cc, 0x13 },
	{ "npu_cc_noc_axi_clk", &gcc, 0x180, &npu_cc, 0x12 },
	{ "npu_cc_noc_dma_clk", &gcc, 0x180, &npu_cc, 0x11 },
	{ "npu_cc_rsc_xo_clk", &gcc, 0x180, &npu_cc, 0x1a },
	{ "npu_cc_s2p_clk", &gcc, 0x180, &npu_cc, 0x16 },
	{ "npu_cc_xo_clk", &gcc, 0x180, &npu_cc, 0x1 },

	//{ "video_cc_ahb_clk", &gcc, 0x57, &video_cc, 0x7 },
	//{ "video_cc_mvs0_clk", &gcc, 0x57, &video_cc, 0x3 },
	//{ "video_cc_mvs0c_clk", &gcc, 0x57, &video_cc, 0x1 },
	//{ "video_cc_mvs1_clk", &gcc, 0x57, &video_cc, 0x5 },
	//{ "video_cc_mvs1_div2_clk", &gcc, 0x57, &video_cc, 0x8 },
	//{ "video_cc_mvs1c_clk", &gcc, 0x57, &video_cc, 0x9 },
	//{ "video_cc_sleep_clk", &gcc, 0x57, &video_cc, 0xc },
	//{ "video_cc_xo_clk", &gcc, 0x57, &video_cc, 0xb },

	{ "measure_only_mccc_clk", &gcc, 0xd1, &mc_cc, 0x50 },

	{ "measure_only_apcs_gold_post_acd_clk", &gcc, 0xe7, &apss_cc, 0x25, /* TODO: Are these pre_div_vals? */ 8 },
	{ "measure_only_apcs_goldplus_post_acd_clk", &gcc, 0xe7, &apss_cc, 0x61, /* TODO: Are these pre_div_vals? */ 8 },
	{ "measure_only_apcs_l3_post_acd_clk", &gcc, 0xe7, &apss_cc, 0x41, /* TODO: Are these pre_div_vals? */ 4 },
	{ "measure_only_apcs_silver_post_acd_clk", &gcc, 0xe7, &apss_cc, 0x21, /* TODO: Are these pre_div_vals? */ 4 },

	{}
};

struct debugcc_platform sm8250_debugcc = {
	"sm8250",
	sm8250_clocks,
	sm8250_blocks,
};
