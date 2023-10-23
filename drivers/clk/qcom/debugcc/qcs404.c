/*
 * Copyright (c) 2019, Linaro Ltd.
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

#define GCC_BASE	0x01800000
#define GCC_SIZE	0x80000

#define GCC_DEBUG_CLK_CTL		0x74000
#define GCC_CLOCK_FREQ_MEASURE_CTL	0x74004
#define GCC_CLOCK_FREQ_MEASURE_STATUS	0x74008
#define GCC_XO_DIV4_CBCR		0x30034

static struct debug_mux gcc;

static struct debug_mux gcc = {
	.base =	GCC_BASE,
	.size = GCC_SIZE,

	.enable_reg = GCC_DEBUG_CLK_CTL,
	.enable_mask = BIT(16),

	.mux_reg = GCC_DEBUG_CLK_CTL,
	.mux_mask = 0x1ff,

	.div_reg = GCC_DEBUG_CLK_CTL,
	.div_shift = 12,
	.div_mask = 0xf << 12,
	.div_val = 4,

	.xo_div4_reg = GCC_XO_DIV4_CBCR,
	.debug_ctl_reg = GCC_CLOCK_FREQ_MEASURE_CTL,
	.debug_status_reg = GCC_CLOCK_FREQ_MEASURE_STATUS,
};

static struct debug_mux turing = {
	.base = 0x800000,
	.size = 0x30000,

	.enable_reg = 0x22008,
	.enable_mask = BIT(0),

	.mux_reg = 0x22000,
	.mux_mask = 0xffff,

	.div_mask = 0,

	.ahb_reg = 0x5e004,
	.ahb_mask = BIT(31),
};

static const struct debug_mux *qcs404_blocks[] = {
	&gcc,
	&turing,
	NULL,
};

static struct measure_clk qcs404_clocks[] = {
	{ "snoc_clk", &gcc, 0 },
	{ "gcc_sys_noc_usb3_clk", &gcc, 1 },
	{ "pnoc_clk", &gcc, 8 },
	{ "gcc_pcnoc_usb2_clk", &gcc, 9 },
	{ "gcc_pcnoc_usb3_clk", &gcc, 10 },
	{ "gcc_gp1_clk", &gcc, 16 },
	{ "gcc_gp2_clk", &gcc, 17 },
	{ "gcc_gp3_clk", &gcc, 18 },
	{ "gcc_bimc_gfx_clk", &gcc, 45 },
	{ "aon_clk_src", &gcc, 50, &turing, 1},
	{ "turing_wrapper_aon_clk", &gcc, 50, &turing, 2},
	{ "turing_wrapper_cnoc_sway_aon_clk", &gcc, 50, &turing, 3},
	{ "turing_wrapper_qos_ahbs_aon_clk", &gcc, 50, &turing, 4},
	{ "q6ss_ahbm_aon_clk", &gcc, 50, &turing, 5},
	{ "q6ss_ahbs_aon_clk", &gcc, 50, &turing, 6},
	{ "turing_wrapper_bus_timeout_aon_clk", &gcc, 50, &turing, 7},
	{ "turing_wrapper_rscc_aon_clk", &gcc, 50, &turing, 8},
	{ "q6ss_alt_reset_aon_clk", &gcc, 50, &turing, 10},
	{ "qos_fixed_lat_counter_clk_src", &gcc, 50, &turing, 11},
	{ "turing_wrapper_qos_dmonitor_fixed_lat_counter_clk", &gcc, 50, &turing, 12},
	{ "turing_wrapper_qos_danger_fixed_lat_counter_clk", &gcc, 50, &turing, 13},
	{ "q6_xo_clk_src", &gcc, 50, &turing, 14},
	{ "qos_xo_clk_src", &gcc, 50, &turing, 15},
	{ "turing_wrapper_qos_xo_lat_counter_clk", &gcc, 50, &turing, 16},
	{ "bcr_slp_clk_src", &gcc, 50, &turing, 19},
	{ "q6ss_bcr_slp_clk", &gcc, 50, &turing, 20},
	{ "turing_wrapper_cnoc_ahbs_clk", &gcc, 50, &turing, 28},
	{ "q6ss_q6_axim_clk", &gcc, 50, &turing, 29},
	{ "q6ss_sleep_clk_src", &gcc, 50, &turing, 33},
	{ "qdsp6ss_xo_clk", &gcc, 50, &turing, 36},
	{ "qdsp6ss_sleep_clk", &gcc, 50, &turing, 37},
	{ "q6ss_dbg_in_clk", &gcc, 50, &turing, 39},
	{ "gcc_usb_hs_system_clk", &gcc, 96 },
	{ "gcc_usb_hs_inactivity_timers_clk", &gcc, 98 },
	{ "gcc_usb2a_phy_sleep_clk", &gcc, 99 },
	{ "gcc_usb_hs_phy_cfg_ahb_clk", &gcc, 100 },
	{ "gcc_usb20_mock_utmi_clk", &gcc, 101 },
	{ "gcc_sdcc1_apps_clk", &gcc, 104 },
	{ "gcc_sdcc1_ahb_clk", &gcc, 105 },
	{ "gcc_sdcc1_ice_core_clk", &gcc, 106 },
	{ "gcc_sdcc2_apps_clk", &gcc, 112 },
	{ "gcc_sdcc2_ahb_clk", &gcc, 113 },
	{ "gcc_usb30_master_clk", &gcc, 120 },
	{ "gcc_usb30_sleep_clk", &gcc, 121 },
	{ "gcc_usb30_mock_utmi_clk", &gcc, 122 },
	{ "gcc_usb3_phy_pipe_clk", &gcc, 123 },
	{ "gcc_usb3_phy_aux_clk", &gcc, 124 },
	{ "gcc_eth_axi_clk", &gcc, 128 },
	{ "gcc_eth_rgmii_clk", &gcc, 129 },
	{ "gcc_eth_slave_ahb_clk", &gcc, 130 },
	{ "gcc_eth_ptp_clk", &gcc, 131 },
	{ "gcc_blsp1_ahb_clk", &gcc, 136 },
	{ "gcc_blsp1_qup1_spi_apps_clk", &gcc, 138 },
	{ "wcnss_m_clk", &gcc, 138 },
	{ "gcc_blsp1_qup1_i2c_apps_clk", &gcc, 139 },
	{ "gcc_blsp1_uart1_apps_clk", &gcc, 140 },
	{ "gcc_blsp1_qup2_spi_apps_clk", &gcc, 142 },
	{ "gcc_blsp1_qup2_i2c_apps_clk", &gcc, 143 },
	{ "gcc_blsp1_uart2_apps_clk", &gcc, 144 },
	{ "gcc_blsp1_qup3_spi_apps_clk", &gcc, 146 },
	{ "gcc_blsp1_qup3_i2c_apps_clk", &gcc, 147 },
	{ "gcc_blsp1_qup4_spi_apps_clk", &gcc, 148 },
	{ "gcc_blsp1_qup4_i2c_apps_clk", &gcc, 149 },
	{ "gcc_blsp1_uart3_apps_clk", &gcc, 150 },
	{ "gcc_blsp1_qup0_spi_apps_clk", &gcc, 152 },
	{ "gcc_blsp1_qup0_i2c_apps_clk", &gcc, 153 },
	{ "gcc_blsp1_uart0_apps_clk", &gcc, 154 },
	{ "gcc_blsp2_ahb_clk", &gcc, 160 },
	{ "gcc_blsp2_qup0_i2c_apps_clk", &gcc, 162 },
	{ "gcc_blsp2_qup0_spi_apps_clk", &gcc, 163 },
	{ "gcc_blsp2_uart0_apps_clk", &gcc, 164 },
	{ "gcc_pcie_0_slv_axi_clk", &gcc, 168 },
	{ "gcc_pcie_0_mstr_axi_clk", &gcc, 169 },
	{ "gcc_pcie_0_cfg_ahb_clk", &gcc, 170 },
	{ "gcc_pcie_0_aux_clk", &gcc, 171 },
	{ "gcc_pcie_0_pipe_clk", &gcc, 172 },
	//{ "pcie0_pipe_clk", &gcc, 173, 1 },
	{ "qpic_clk", &gcc, 192 },
	{ "gcc_pdm_ahb_clk", &gcc, 208 },
	{ "gcc_pdm2_clk", &gcc, 210 },
	{ "gcc_pwm0_xo512_clk", &gcc, 211 },
	{ "gcc_pwm1_xo512_clk", &gcc, 212 },
	{ "gcc_pwm2_xo512_clk", &gcc, 213 },
	{ "gcc_prng_ahb_clk", &gcc, 216 },
	{ "gcc_geni_ir_s_clk", &gcc, 238 },
	{ "gcc_boot_rom_ahb_clk", &gcc, 248 },
	{ "ce1_clk", &gcc, 312 },
	{ "bimc_clk", &gcc, 346 },
	//{ "bimc_fsm_ddr_clk", &gcc, 350, 1 },
	{ "gcc_apss_ahb_clk", &gcc, 360 },
	{ "gcc_dcc_clk", &gcc, 441 },
	{ "gcc_oxili_gfx3d_clk", &gcc, 490 },
	{ "gcc_oxili_ahb_clk", &gcc, 491 },
	{ "gcc_mdss_hdmi_pclk_clk", &gcc, 497 },
	{ "gcc_mdss_hdmi_app_clk", &gcc, 498 },
	{ "gcc_mdss_ahb_clk", &gcc, 502 },
	{ "gcc_mdss_axi_clk", &gcc, 503 },
	{ "gcc_mdss_pclk0_clk", &gcc, 504 },
	{ "gcc_mdss_mdp_clk", &gcc, 505 },
	{ "gcc_mdss_vsync_clk", &gcc, 507 },
	{ "gcc_mdss_byte0_clk", &gcc, 508 },
	{ "gcc_mdss_esc0_clk", &gcc, 509 },
	{}
};

struct debugcc_platform qcs404_debugcc = {
	"qcs404",
	qcs404_clocks,
	qcs404_blocks,
};
