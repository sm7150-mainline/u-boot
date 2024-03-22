// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Linaro Ltd.
 *
 * Stub clock driver for non-vital clocks.
 */

#include <clk.h>
#include <clk-uclass.h>
#include <dm.h>

static ulong stub_clk_set_rate(struct clk *clk, ulong rate)
{
	return (clk->rate = rate);
}

static ulong stub_clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int stub_clk_nop(struct clk *clk)
{
	return 0;
};

static struct clk_ops stub_clk_ops = {
	.set_rate = stub_clk_set_rate,
	.get_rate = stub_clk_get_rate,
	.enable = stub_clk_nop,
	.disable = stub_clk_nop,
};

U_BOOT_DRIVER(stub_clk) = {
	.name		= "stub_clk",
	.id		= UCLASS_CLK,
	.ops		= &stub_clk_ops,
	.flags		= DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
