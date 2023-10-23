// SPDX-License-Identifier: BSD-3-Clause
/* Copyright (c) 2022, Linaro Limited */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debugcc.h"

static struct debug_mux cpu_cc = {
	.base = 0xf111000,
	.size = 0x1000,
	.block_name = "cpu",

	.enable_reg = 0x1c,
	.enable_mask = BIT(0),

	.mux_reg = 0x1c,
	.mux_mask = 0x3ff << 8,
	.mux_shift = 8,

	.div_reg = 0x1c,
	.div_mask = 0xf << 28,
	.div_shift = 28,
};

static struct debug_mux disp_cc = {
	.base = 0x5f00000,
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

static struct debug_mux gcc = {
	.base = 0x1400000,
	.size = 0x1f0000,

	.enable_reg = 0x30004,
	.enable_mask = BIT(0),

	.mux_reg = 0x62000,
	.mux_mask = 0x3ff,

	.div_reg = 0x30000,
	.div_mask = 0xf,
	.div_val = 4,

	.xo_div4_reg = 0x28008,
	.debug_ctl_reg = 0x62038,
	.debug_status_reg = 0x6203c,
};

static struct debug_mux gpu_cc = {
	.base = 0x5990000,
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

static const struct debug_mux *sm6115_blocks[] = {
	&gcc,
	&cpu_cc,
	&disp_cc,
	&gpu_cc,
	NULL,
};

static struct measure_clk sm6115_clocks[] = {
	{ "perfcl_clk", &gcc, 0xaf, &cpu_cc, 0x1 },
	{ "pwrcl_clk", &gcc, 0xaf, &cpu_cc, 0x0 },
	//{ "cpu_cc_debug_mux", &gcc, 0xaf },
	//{ "disp_cc_debug_mux", &gcc, 0x42 },
	{ "gcc_ahb2phy_csi_clk", &gcc, 0x63 },
	{ "gcc_ahb2phy_usb_clk", &gcc, 0x64 },
	{ "gcc_bimc_gpu_axi_clk", &gcc, 0x90 },
	{ "gcc_boot_rom_ahb_clk", &gcc, 0x76 },
	{ "gcc_cam_throttle_nrt_clk", &gcc, 0x4c },
	{ "gcc_cam_throttle_rt_clk", &gcc, 0x4b },
	{ "gcc_camera_ahb_clk", &gcc, 0x37 },
	{ "gcc_camera_xo_clk", &gcc, 0x3f },
	{ "gcc_camss_axi_clk", &gcc, 0x136 },
	{ "gcc_camss_camnoc_atb_clk", &gcc, 0x138 },
	{ "gcc_camss_camnoc_nts_xo_clk", &gcc, 0x139 },
	{ "gcc_camss_cci_0_clk", &gcc, 0x134 },
	{ "gcc_camss_cphy_0_clk", &gcc, 0x128 },
	{ "gcc_camss_cphy_1_clk", &gcc, 0x129 },
	{ "gcc_camss_cphy_2_clk", &gcc, 0x12a },
	{ "gcc_camss_csi0phytimer_clk", &gcc, 0x11a },
	{ "gcc_camss_csi1phytimer_clk", &gcc, 0x11b },
	{ "gcc_camss_csi2phytimer_clk", &gcc, 0x11c },
	{ "gcc_camss_mclk0_clk", &gcc, 0x11d },
	{ "gcc_camss_mclk1_clk", &gcc, 0x11e },
	{ "gcc_camss_mclk2_clk", &gcc, 0x11f },
	{ "gcc_camss_mclk3_clk", &gcc, 0x120 },
	{ "gcc_camss_nrt_axi_clk", &gcc, 0x13a },
	{ "gcc_camss_ope_ahb_clk", &gcc, 0x133 },
	{ "gcc_camss_ope_clk", &gcc, 0x131 },
	{ "gcc_camss_rt_axi_clk", &gcc, 0x13c },
	{ "gcc_camss_tfe_0_clk", &gcc, 0x121 },
	{ "gcc_camss_tfe_0_cphy_rx_clk", &gcc, 0x125 },
	{ "gcc_camss_tfe_0_csid_clk", &gcc, 0x12b },
	{ "gcc_camss_tfe_1_clk", &gcc, 0x122 },
	{ "gcc_camss_tfe_1_cphy_rx_clk", &gcc, 0x126 },
	{ "gcc_camss_tfe_1_csid_clk", &gcc, 0x12d },
	{ "gcc_camss_tfe_2_clk", &gcc, 0x123 },
	{ "gcc_camss_tfe_2_cphy_rx_clk", &gcc, 0x127 },
	{ "gcc_camss_tfe_2_csid_clk", &gcc, 0x12f },
	{ "gcc_camss_top_ahb_clk", &gcc, 0x135 },
	{ "gcc_cfg_noc_usb3_prim_axi_clk", &gcc, 0x1d },
	{ "gcc_cpuss_gnoc_clk", &gcc, 0xaa },
	{ "gcc_disp_ahb_clk", &gcc, 0x38 },
	{ "gcc_disp_gpll0_div_clk_src", &gcc, 0x47 },
	{ "gcc_disp_hf_axi_clk", &gcc, 0x3d },
	{ "gcc_disp_throttle_core_clk", &gcc, 0x49 },
	{ "gcc_disp_xo_clk", &gcc, 0x40 },
	{ "gcc_gp1_clk", &gcc, 0xba },
	{ "gcc_gp2_clk", &gcc, 0xbb },
	{ "gcc_gp3_clk", &gcc, 0xbc },
	{ "gcc_gpu_cfg_ahb_clk", &gcc, 0xe5 },
	{ "gcc_gpu_gpll0_clk_src", &gcc, 0xeb },
	{ "gcc_gpu_gpll0_div_clk_src", &gcc, 0xec },
	{ "gcc_gpu_memnoc_gfx_clk", &gcc, 0xe8 },
	{ "gcc_gpu_snoc_dvm_gfx_clk", &gcc, 0xea },
	{ "gcc_gpu_throttle_core_clk", &gcc, 0xef },
	{ "gcc_pdm2_clk", &gcc, 0x73 },
	{ "gcc_pdm_ahb_clk", &gcc, 0x71 },
	{ "gcc_pdm_xo4_clk", &gcc, 0x72 },
	{ "gcc_prng_ahb_clk", &gcc, 0x74 },
	{ "gcc_qmip_camera_nrt_ahb_clk", &gcc, 0x3a },
	{ "gcc_qmip_camera_rt_ahb_clk", &gcc, 0x48 },
	{ "gcc_qmip_disp_ahb_clk", &gcc, 0x3b },
	{ "gcc_qmip_gpu_cfg_ahb_clk", &gcc, 0xed },
	{ "gcc_qmip_video_vcodec_ahb_clk", &gcc, 0x39 },
	{ "gcc_qupv3_wrap0_core_2x_clk", &gcc, 0x6a },
	{ "gcc_qupv3_wrap0_core_clk", &gcc, 0x69 },
	{ "gcc_qupv3_wrap_0_m_ahb_clk", &gcc, 0x67 },
	{ "gcc_qupv3_wrap_0_s_ahb_clk", &gcc, 0x68 },
	{ "gcc_sdcc1_ahb_clk", &gcc, 0xf3 },
	{ "gcc_sdcc1_apps_clk", &gcc, 0xf2 },
	{ "gcc_sdcc1_ice_core_clk", &gcc, 0xf4 },
	{ "gcc_sdcc2_ahb_clk", &gcc, 0x66 },
	{ "gcc_sdcc2_apps_clk", &gcc, 0x65 },
	{ "gcc_sys_noc_cpuss_ahb_clk", &gcc, 0x9 },
	{ "gcc_sys_noc_ufs_phy_axi_clk", &gcc, 0x19 },
	{ "gcc_sys_noc_usb3_prim_axi_clk", &gcc, 0x18 },
	{ "gcc_ufs_phy_ahb_clk", &gcc, 0x111 },
	{ "gcc_ufs_phy_axi_clk", &gcc, 0x110 },
	{ "gcc_ufs_phy_ice_core_clk", &gcc, 0x117 },
	{ "gcc_ufs_phy_phy_aux_clk", &gcc, 0x118 },
	{ "gcc_ufs_phy_rx_symbol_0_clk", &gcc, 0x113 },
	{ "gcc_ufs_phy_tx_symbol_0_clk", &gcc, 0x112 },
	{ "gcc_ufs_phy_unipro_core_clk", &gcc, 0x116 },
	{ "gcc_usb30_prim_master_clk", &gcc, 0x5c },
	{ "gcc_usb30_prim_mock_utmi_clk", &gcc, 0x5e },
	{ "gcc_usb30_prim_sleep_clk", &gcc, 0x5d },
	{ "gcc_usb3_prim_phy_com_aux_clk", &gcc, 0x5f },
	{ "gcc_usb3_prim_phy_pipe_clk", &gcc, 0x60 },
	{ "gcc_vcodec0_axi_clk", &gcc, 0x142 },
	{ "gcc_venus_ahb_clk", &gcc, 0x143 },
	{ "gcc_venus_ctl_axi_clk", &gcc, 0x141 },
	{ "gcc_video_ahb_clk", &gcc, 0x36 },
	{ "gcc_video_axi0_clk", &gcc, 0x3c },
	{ "gcc_video_throttle_core_clk", &gcc, 0x4a },
	{ "gcc_video_vcodec0_sys_clk", &gcc, 0x13f },
	{ "gcc_video_venus_ctl_clk", &gcc, 0x13d },
	{ "gcc_video_xo_clk", &gcc, 0x3e },
	//{ "gpu_cc_debug_mux", &gcc, 0xe7 },
	//{ "mc_cc_debug_mux", &gcc, 0x9e },
	{ "measure_only_cnoc_clk", &gcc, 0x1a },
	{ "measure_only_ipa_2x_clk", &gcc, 0xc6 },
	{ "measure_only_snoc_clk", &gcc, 0x7 },
	{ "disp_cc_mdss_ahb_clk", &gcc, 0x42, &disp_cc, 0x1a },
	{ "disp_cc_mdss_byte0_clk", &gcc, 0x42, &disp_cc, 0x12 },
	{ "disp_cc_mdss_byte0_intf_clk", &gcc, 0x42, &disp_cc, 0x13 },
	{ "disp_cc_mdss_esc0_clk", &gcc, 0x42, &disp_cc, 0x14 },
	{ "disp_cc_mdss_mdp_clk", &gcc, 0x42, &disp_cc, 0xe },
	{ "disp_cc_mdss_mdp_lut_clk", &gcc, 0x42, &disp_cc, 0x10 },
	{ "disp_cc_mdss_non_gdsc_ahb_clk", &gcc, 0x42, &disp_cc, 0x1b },
	{ "disp_cc_mdss_pclk0_clk", &gcc, 0x42, &disp_cc, 0xd },
	{ "disp_cc_mdss_rot_clk", &gcc, 0x42, &disp_cc, 0xf },
	{ "disp_cc_mdss_vsync_clk", &gcc, 0x42, &disp_cc, 0x11 },
	{ "disp_cc_sleep_clk", &gcc, 0x42, &disp_cc, 0x24 },
	{ "disp_cc_xo_clk", &gcc, 0x42, &disp_cc, 0x23 },
	{ "gpu_cc_ahb_clk", &gcc, 0xe7, &gpu_cc, 0x10 },
	{ "gpu_cc_crc_ahb_clk", &gcc, 0xe7, &gpu_cc, 0x11 },
	{ "gpu_cc_cx_gfx3d_clk", &gcc, 0xe7, &gpu_cc, 0x1a },
	{ "gpu_cc_cx_gmu_clk", &gcc, 0xe7, &gpu_cc, 0x18 },
	{ "gpu_cc_cx_snoc_dvm_clk", &gcc, 0xe7, &gpu_cc, 0x15 },
	{ "gpu_cc_cxo_aon_clk", &gcc, 0xe7, &gpu_cc, 0xa },
	{ "gpu_cc_cxo_clk", &gcc, 0xe7, &gpu_cc, 0x19 },
	{ "gpu_cc_gx_cxo_clk", &gcc, 0xe7, &gpu_cc, 0xe },
	{ "gpu_cc_gx_gfx3d_clk", &gcc, 0xe7, &gpu_cc, 0xb },
	{ "gpu_cc_sleep_clk", &gcc, 0xe7, &gpu_cc, 0x16 },
	{}
};

struct debugcc_platform sm6115_debugcc = {
	"sm6115",
	sm6115_clocks,
	sm6115_blocks,
};
