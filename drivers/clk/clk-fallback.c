// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Linaro Ltd.
 *
 * Stub clock driver for non-vital clocks.
 */

#include <clk.h>
#include <clk-uclass.h>
#include <dm.h>

static ulong fallback_clk_set_rate(struct clk *clk, ulong rate)
{
	return (clk->rate = rate);
}

static ulong fallback_clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int fallback_clk_nop(struct clk *clk)
{
	return 0;
};

static struct clk_ops fallback_clk_ops = {
	.set_rate = fallback_clk_set_rate,
	.get_rate = fallback_clk_get_rate,
	.enable = fallback_clk_nop,
	.disable = fallback_clk_nop,
};

U_BOOT_DRIVER(clk_fallback) = {
	.name		= "clk_fallback",
	.id		= UCLASS_CLK,
	.ops		= &fallback_clk_ops,
	.flags		= DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
