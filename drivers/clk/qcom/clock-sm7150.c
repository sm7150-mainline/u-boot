// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for Qualcomm SM7150
 *
 * (C) Copyright 2024 Danila Tikhonov <danila@jiaxyga.com>
 *
 * Based on Linux Kernel driver
 */

#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dt-bindings/clock/qcom,sm7150-gcc.h>

#include "clock-qcom.h"

#define USB30_PRIM_MASTER_CLK_CMD_RCGR		0xf01c
#define USB30_PRIM_MOCK_UTMI_CLK_CMD_RCGR	0xf034
#define USB3_PRIM_PHY_AUX_CMD_RCGR		0xf060

#define SE8_UART_APPS_CMD_RCGR			0x18278
#define GCC_SDCC2_APPS_CLK_SRC_REG		0x1400c

#define APCS_GPLL7_STATUS			0x27000
#define APCS_GPLLX_ENA_REG			0x52000

static const struct freq_tbl ftbl_gcc_qupv3_wrap0_s0_clk_src[] = {
	F(7372800, CFG_CLK_SRC_GPLL0_EVEN, 1, 384, 15625),
	F(14745600, CFG_CLK_SRC_GPLL0_EVEN, 1, 768, 15625),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(29491200, CFG_CLK_SRC_GPLL0_EVEN, 1, 1536, 15625),
	F(32000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 8, 75),
	F(48000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 4, 25),
	F(64000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 16, 75),
	F(80000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 4, 15),
	F(96000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 8, 25),
	F(100000000, CFG_CLK_SRC_GPLL0_EVEN, 3, 0, 0),
	F(102400000, CFG_CLK_SRC_GPLL0_EVEN, 1, 128, 375),
	F(112000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 28, 75),
	F(117964800, CFG_CLK_SRC_GPLL0_EVEN, 1, 6144, 15625),
	F(120000000, CFG_CLK_SRC_GPLL0_EVEN, 2.5, 0, 0),
	F(128000000, CFG_CLK_SRC_GPLL0, 1, 16, 75),
	{ }
};

static const struct freq_tbl ftbl_gcc_sdcc2_apps_clk_src[] = {
	F(400000, CFG_CLK_SRC_CXO, 12, 1, 4),
	F(9600000, CFG_CLK_SRC_CXO, 2, 0, 0),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(25000000, CFG_CLK_SRC_GPLL0_EVEN, 12, 0, 0),
	F(50000000, CFG_CLK_SRC_GPLL0_EVEN, 6, 0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0, 6, 0, 0),
	F(208000000, CFG_CLK_SRC_GPLL7, 4, 0, 0),
	{ }
};

static const struct pll_vote_clk gpll7_vote_clk = {
	.status = APCS_GPLL7_STATUS,
	.status_bit = BIT(31),
	.ena_vote = APCS_GPLLX_ENA_REG,
	.vote_bit = BIT(7),
};

static ulong sm7150_clk_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	const struct freq_tbl *freq;

	if (clk->id < priv->data->num_clks)
		debug("%s: %s, requested rate=%ld\n", __func__,
			priv->data->clks[clk->id].name, rate);

	switch (clk->id) {
	case GCC_QUPV3_WRAP1_S2_CLK: /* UART8 */
		freq = qcom_find_freq(ftbl_gcc_qupv3_wrap0_s0_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, SE8_UART_APPS_CMD_RCGR,
						freq->pre_div, freq->m, freq->n, freq->src, 16);
		return freq->freq;
	case GCC_SDCC2_APPS_CLK:
		/* Enable GPLL7 so that we can point SDCC2_APPS_CLK_SRC at it */
		clk_enable_gpll0(priv->base, &gpll7_vote_clk);
		freq = qcom_find_freq(ftbl_gcc_sdcc2_apps_clk_src, rate);
		printf("%s: got freq %u\n", __func__, freq->freq);
		WARN(freq->src != CFG_CLK_SRC_GPLL7, "SDCC2_APPS_CLK_SRC not set to GPLL7, requested rate %lu\n", rate);
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC2_APPS_CLK_SRC_REG,
						freq->pre_div, freq->m, freq->n, CFG_CLK_SRC_GPLL7, 8);
		return rate;
	default:
		return 0;
	}
}

static const struct gate_clk sm7150_clks[] = {
	GATE_CLK(GCC_QUPV3_WRAP0_S0_CLK,	0x5200c, BIT(10)),
	GATE_CLK(GCC_QUPV3_WRAP0_S1_CLK,	0x5200c, BIT(11)),
	GATE_CLK(GCC_QUPV3_WRAP0_S2_CLK,	0x5200c, BIT(12)),
	GATE_CLK(GCC_QUPV3_WRAP0_S3_CLK,	0x5200c, BIT(13)),
	GATE_CLK(GCC_QUPV3_WRAP0_S4_CLK,	0x5200c, BIT(14)),
	GATE_CLK(GCC_QUPV3_WRAP0_S5_CLK,	0x5200c, BIT(15)),
	GATE_CLK(GCC_QUPV3_WRAP0_S6_CLK,	0x5200c, BIT(16)),
	GATE_CLK(GCC_QUPV3_WRAP0_S7_CLK,	0x5200c, BIT(17)),
	GATE_CLK(GCC_QUPV3_WRAP1_S0_CLK,	0x5200c, BIT(22)),
	GATE_CLK(GCC_QUPV3_WRAP1_S1_CLK,	0x5200c, BIT(23)),
	GATE_CLK(GCC_QUPV3_WRAP1_S2_CLK,	0x5200c, BIT(24)),
	GATE_CLK(GCC_QUPV3_WRAP1_S3_CLK,	0x5200c, BIT(25)),
	GATE_CLK(GCC_QUPV3_WRAP1_S4_CLK,	0x5200c, BIT(26)),
	GATE_CLK(GCC_QUPV3_WRAP1_S5_CLK,	0x5200c, BIT(27)),
	GATE_CLK(GCC_QUPV3_WRAP1_S6_CLK,	0x5200c, BIT(28)),
	GATE_CLK(GCC_QUPV3_WRAP1_S7_CLK,	0x5200c, BIT(29)),
	GATE_CLK(GCC_QUPV3_WRAP_0_M_AHB_CLK,	0x5200c, BIT(6)),
	GATE_CLK(GCC_QUPV3_WRAP_0_S_AHB_CLK,	0x5200c, BIT(7)),
	GATE_CLK(GCC_QUPV3_WRAP_1_M_AHB_CLK,	0x5200c, BIT(20)),
	GATE_CLK(GCC_QUPV3_WRAP_1_S_AHB_CLK,	0x5200c, BIT(21)),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x12008, BIT(0)),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x1200c, BIT(0)),
	GATE_CLK(GCC_SDCC1_ICE_CORE_CLK,	0x12040, BIT(0)),
	GATE_CLK(GCC_SDCC2_AHB_CLK,		0x14008, BIT(0)),
	GATE_CLK(GCC_SDCC2_APPS_CLK,		0x14004, BIT(0)),
	GATE_CLK(GCC_SDCC4_AHB_CLK,		0x16008, BIT(0)),
	GATE_CLK(GCC_SDCC4_APPS_CLK,		0x16004, BIT(0)),
	GATE_CLK(GCC_UFS_MEM_CLKREF_CLK,	0x8c000, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_AHB_CLK,		0x77014, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_AXI_CLK,		0x77038, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_ICE_CORE_CLK,	0x77090, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_PHY_AUX_CLK,	0x77094, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_RX_SYMBOL_0_CLK,	0x7701c, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_TX_SYMBOL_0_CLK,	0x77018, BIT(0)),
	GATE_CLK(GCC_UFS_PHY_UNIPRO_CORE_CLK,	0x7708c, BIT(0)),
	GATE_CLK(GCC_USB30_PRIM_MASTER_CLK,	0x0f010, BIT(0)),
	GATE_CLK(GCC_USB30_PRIM_MOCK_UTMI_CLK,	0x0f018, BIT(0)),
	GATE_CLK(GCC_USB30_PRIM_SLEEP_CLK,	0x0f014, BIT(0)),
	GATE_CLK(GCC_USB3_PRIM_CLKREF_CLK,	0x8c010, BIT(0)),
	GATE_CLK(GCC_USB3_PRIM_PHY_AUX_CLK,	0x0f050, BIT(0)),
	GATE_CLK(GCC_USB3_PRIM_PHY_COM_AUX_CLK,	0x0f054, BIT(0)),
	GATE_CLK(GCC_USB3_PRIM_PHY_PIPE_CLK,	0x0f058, BIT(0)),
	GATE_CLK(GCC_USB_PHY_CFG_AHB2PHY_CLK,	0x6a004, BIT(0)),
};

static int sm7150_clk_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks < clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, sm7150_clks[clk->id].name);

	switch (clk->id) {
	case GCC_USB30_PRIM_MASTER_CLK:
		qcom_gate_clk_en(priv, GCC_USB_PHY_CFG_AHB2PHY_CLK);
		/* These numbers are just pulled from the frequency tables in the Linux driver */
		clk_rcg_set_rate_mnd(priv->base, USB30_PRIM_MASTER_CLK_CMD_RCGR,
				     (4.5 * 2) - 1, 0, 0, 1 << 8, 8);
		clk_rcg_set_rate_mnd(priv->base, USB30_PRIM_MOCK_UTMI_CLK_CMD_RCGR,
				     1, 0, 0, 0, 8);
		clk_rcg_set_rate_mnd(priv->base, USB3_PRIM_PHY_AUX_CMD_RCGR,
				     1, 0, 0, 0, 8);
		break;
	}

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map sm7150_gcc_resets[] = {
	[GCC_UFS_PHY_BCR] = { 0x77000 },
	[GCC_USB30_PRIM_BCR] = { 0xf000 },
	[GCC_USB3_DP_PHY_PRIM_BCR] = { 0x50008 },
	[GCC_USB3_DP_PHY_SEC_BCR] = { 0x50014 },
	[GCC_USB3_PHY_PRIM_BCR] = { 0x50000 },
	[GCC_USB3_PHY_SEC_BCR] = { 0x5000c },
	[GCC_QUSB2PHY_PRIM_BCR] = { 0x26000 },
};

static const struct qcom_power_map sm7150_gdscs[] = {
	[PCIE_0_GDSC] = { 0x6b004 },
	[UFS_PHY_GDSC] = { 0x77004 },
	[USB30_PRIM_GDSC] = { 0xf004 },
	[HLOS1_VOTE_AGGRE_NOC_MMU_AUDIO_TBU_GDSC] = { 0x7d030 },
	[HLOS1_VOTE_AGGRE_NOC_MMU_PCIE_TBU_GDSC] = { 0x7d03c },
	[HLOS1_VOTE_AGGRE_NOC_MMU_TBU1_GDSC] = { 0x7d034 },
	[HLOS1_VOTE_AGGRE_NOC_MMU_TBU2_GDSC] = { 0x7d038 },
	[HLOS1_VOTE_MMNOC_MMU_TBU_HF0_GDSC] = { 0x7d040 },
	[HLOS1_VOTE_MMNOC_MMU_TBU_HF1_GDSC] = { 0x7d048 },
	[HLOS1_VOTE_MMNOC_MMU_TBU_SF_GDSC] = { 0x7d044 },
};

static const phys_addr_t sm7150_gpll_addrs[] = {
	0x00100000, // GCC_GPLL0_MODE
	0x00101000, // GCC_GPLL1_MODE /* unused */
	0x00102000, // GCC_GPLL2_MODE /* unused */
	0x00103000, // GCC_GPLL3_MODE /* unused */
	0x00176000, // GCC_GPLL4_MODE /* unused */
	0x00174000, // GCC_GPLL5_MODE /* unused */
	0x00113000, // GCC_GPLL6_MODE /* unused */
	0x00127000, // GCC_GPLL7_MODE
};

static const phys_addr_t sm7150_rcg_addrs[] = {
	0x0010f010, // GCC_USB30_PRIM_MASTER
	0x0010f018, // GCC_USB30_PRIM_MOCK_UTMI
	0x0010f054, // GCC_USB3_PRIM_PHY_AUX
	0x0011200c, // GCC_SDCC1_APPS
	0x00112040, // GCC_SDCC1_ICE_CORE
	0x00114004, // GCC_SDCC2_APPS
	0x00116004, // GCC_SDCC4_APPS
	0x00117014, // GCC_QUPV3_WRAP0_CORE_2X
	0x00117030, // GCC_QUPV3_WRAP0_S0
	0x00117160, // GCC_QUPV3_WRAP0_S1
	0x00117290, // GCC_QUPV3_WRAP0_S2
	0x001173c0, // GCC_QUPV3_WRAP0_S3
	0x001174f0, // GCC_QUPV3_WRAP0_S4
	0x00117620, // GCC_QUPV3_WRAP0_S5
	0x00117750, // GCC_QUPV3_WRAP0_S6
	0x00117880, // GCC_QUPV3_WRAP0_S7
	0x00118014, // GCC_QUPV3_WRAP1_S0
	0x00118144, // GCC_QUPV3_WRAP1_S1
	0x00118274, // GCC_QUPV3_WRAP1_S2
	0x001183a4, // GCC_QUPV3_WRAP1_S3
	0x001184d4, // GCC_QUPV3_WRAP1_S4
	0x00118604, // GCC_QUPV3_WRAP1_S5
	0x00118734, // GCC_QUPV3_WRAP1_S6
	0x00118864, // GCC_QUPV3_WRAP1_S7
	0x0016f004, // GCC_PCIE_0_AUX
	0x0016f02c, // GCC_PCIE_PHY_REFGEN
	0x00177038, // GCC_UFS_PHY_AXI
	0x00177090, // GCC_UFS_PHY_ICE_CORE
	0x0017708c, // GCC_UFS_PHY_UNIPRO_CORE
	0x00177094, // GCC_UFS_PHY_PHY_AUX
};

static const char *const sm7150_rcg_names[] = {
	"GCC_USB30_PRIM_MASTER",
	"GCC_USB30_PRIM_MOCK_UTMI",
	"GCC_USB3_PRIM_PHY_AUX",
	"GCC_SDCC1_APPS",
	"GCC_SDCC1_ICE_CORE",
	"GCC_SDCC2_APPS",
	"GCC_SDCC4_APPS",
	"GCC_QUPV3_WRAP0_CORE_2X",
	"GCC_QUPV3_WRAP0_S0",
	"GCC_QUPV3_WRAP0_S1",
	"GCC_QUPV3_WRAP0_S2",
	"GCC_QUPV3_WRAP0_S3",
	"GCC_QUPV3_WRAP0_S4",
	"GCC_QUPV3_WRAP0_S5",
	"GCC_QUPV3_WRAP0_S6",
	"GCC_QUPV3_WRAP0_S7",
	"GCC_QUPV3_WRAP1_S0",
	"GCC_QUPV3_WRAP1_S1",
	"GCC_QUPV3_WRAP1_S2",
	"GCC_QUPV3_WRAP1_S3",
	"GCC_QUPV3_WRAP1_S4",
	"GCC_QUPV3_WRAP1_S5",
	"GCC_QUPV3_WRAP1_S6",
	"GCC_QUPV3_WRAP1_S7",
	"GCC_PCIE_0_AUX",
	"GCC_PCIE_PHY_REFGEN",
	"GCC_UFS_PHY_AXI",
	"GCC_UFS_PHY_ICE_CORE",
	"GCC_UFS_PHY_UNIPRO_CORE",
	"GCC_UFS_PHY_PHY_AUX",
};

static struct msm_clk_data sm7150_gcc_data = {
	.resets = sm7150_gcc_resets,
	.num_resets = ARRAY_SIZE(sm7150_gcc_resets),
	.clks = sm7150_clks,
	.num_clks = ARRAY_SIZE(sm7150_clks),
	.power_domains = sm7150_gdscs,
	.num_power_domains = ARRAY_SIZE(sm7150_gdscs),

	.enable = sm7150_clk_enable,
	.set_rate = sm7150_clk_set_rate,

	.dbg_pll_addrs = sm7150_gpll_addrs,
	.num_plls = ARRAY_SIZE(sm7150_gpll_addrs),
	.dbg_rcg_addrs = sm7150_rcg_addrs,
	.num_rcgs = ARRAY_SIZE(sm7150_rcg_addrs),
	.dbg_rcg_names = sm7150_rcg_names,
};

static const struct udevice_id gcc_sm7150_of_match[] = {
	{ .compatible = "qcom,sm7150-gcc", .data = (ulong)&sm7150_gcc_data, },
	{ /* Sentinal */ }
};

U_BOOT_DRIVER(gcc_sm7150) = {
	.name		= "gcc_sm7150",
	.id		= UCLASS_NOP,
	.of_match	= gcc_sm7150_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC,
};