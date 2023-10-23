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
#include <dm/device.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <asm/io.h>

#include "debugcc.h"

static const struct debugcc_platform *platforms[] = {
	&qcs404_debugcc,
	&sdm845_debugcc,
	&sm6115_debugcc,
	&sm8250_debugcc,
	NULL
};

static unsigned int measure_ticks(struct debug_mux *gcc, unsigned int ticks)
{
	uint32_t val;

	writel(ticks, gcc->base + gcc->debug_ctl_reg);
	do {
		val = readl(gcc->base + gcc->debug_status_reg);
	} while (val & BIT(25));

	writel(ticks | BIT(20), gcc->base + gcc->debug_ctl_reg);
	do {
		val = readl(gcc->base + gcc->debug_status_reg);
	} while (!(val & BIT(25)));

	val &= 0x1ffffff;

	writel(ticks, gcc->base + gcc->debug_ctl_reg);

	return val;
}

static void mux_prepare_enable(struct debug_mux *mux, int selector)
{
	uint32_t val;

	if (mux->mux_mask) {
		val = readl(mux->base + mux->mux_reg);
		val &= ~mux->mux_mask;
		val |= selector << mux->mux_shift;
		writel(val, mux->base + mux->mux_reg);
	}

	if (mux->div_mask) {
		val = readl(mux->base + mux->div_reg);
		val &= ~mux->div_mask;
		val |= (mux->div_val - 1) << mux->div_shift;
		writel(val, mux->base + mux->div_reg);
	}

	mux_enable(mux);
}

void mux_enable(struct debug_mux *mux)
{
	uint32_t val;

	if (mux->enable_mask) {
		val = readl(mux->base + mux->enable_reg);
		val |= mux->enable_mask;
		writel(val, mux->base + mux->enable_reg);
	}

	if (mux->premeasure)
		mux->premeasure(mux);
}

void mux_disable(struct debug_mux *mux)
{
	uint32_t val;

	if (mux->postmeasure)
		mux->postmeasure(mux);

	if (mux->enable_mask) {
		val = readl(mux->base + mux->enable_reg);
		val &= ~mux->enable_mask;
		writel(val, mux->base + mux->enable_reg);
	}
}

static bool leaf_enabled(struct debug_mux *mux, struct debug_mux *leaf)
{
	uint32_t val;

	/* If no AHB clock is specified, we assume it's clocked */
	if (!leaf || !leaf->ahb_mask)
		return true;

	val = readl(mux->base + leaf->ahb_reg);
	val &= leaf->ahb_mask;

	/* CLK_OFF will be set if block is not clocked, so inverse */
	return !val;
}

static unsigned long measure_default(const struct measure_clk *clk)
{
	unsigned long raw_count_short;
	unsigned long raw_count_full;
	struct debug_mux *gcc = clk->primary;
	unsigned long xo_div4;

	xo_div4 = readl(gcc->base + gcc->xo_div4_reg);
	writel(xo_div4 | 1, gcc->base + gcc->xo_div4_reg);

	raw_count_short = measure_ticks(gcc, 0x1000);
	raw_count_full = measure_ticks(gcc, 0x10000);

	writel(xo_div4, gcc->base + gcc->xo_div4_reg);

	if (raw_count_full == raw_count_short) {
		return 0;
	}

	raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
	raw_count_full = raw_count_full / ((0x10000 * 10) + 35);

	if (clk->leaf && clk->leaf->div_val)
		raw_count_full *= clk->leaf->div_val;

	if (clk->primary->div_val)
		raw_count_full *= clk->primary->div_val;

	if (clk->fixed_div)
		raw_count_full *= clk->fixed_div;


	return raw_count_full;
}

unsigned long measure_mccc(const struct measure_clk *clk)
{
	/* MCCC is always on, just read the rate and return. */
	return 1000000000000ULL / readl(clk->leaf->base + clk->leaf_mux);
}

static void measure(const struct measure_clk *clk)
{
	unsigned long clk_rate;
	struct debug_mux *gcc = clk->primary;

	if (clk->leaf)
		printf("| %-8s ", clk->leaf->block_name);
	else
		printf("| %-8s ", "core");

	if (!leaf_enabled(gcc, clk->leaf)) {
		printf("| %-40s | %7s | %-13s |\n", clk->name, "SKIP", "");
		return;
	}

	if (clk->leaf)
		mux_prepare_enable(clk->leaf, clk->leaf_mux);

	mux_prepare_enable(clk->primary, clk->mux);

	if (clk->leaf && clk->leaf->measure)
		clk_rate = clk->leaf->measure(clk);
	else
		clk_rate = measure_default(clk);

	mux_disable(clk->primary);

	if (clk->leaf) {
		mux_disable(clk->leaf);
	}

	if (clk_rate == 0) {
		printf("| %-40s | %7s | %-13s |\n", clk->name, "OFF", "");
		return;
	}

	printf("| %-40s | %4luMHz | %11ldHz |\n", clk->name, DIV_ROUND_CLOSEST(clk_rate, 1000000), clk_rate);
}

static const struct debugcc_platform *find_platform(void)
{
	const struct debugcc_platform **p;
	const char **compatibles, *name;

	if (ofnode_read_string_list(ofnode_root(), "compatible", &compatibles) < 0) {
		fprintf(stderr, "Can't get root compatible!\n");
		return NULL;
	}
	/* Get the last compatible in the list */
	while (compatibles[1]) compatibles++;
	name = compatibles[0];
	while (*name++ != ',') {
		if (*name == '\0') {
			fprintf(stderr, "Compatible '%s' doesn't match 'qcom,platform' format!\n", compatibles[0]);
			return NULL;
		}
	}

	for (p = platforms; *p; p++) {
		if (!strcmp((*p)->name, name))
			return *p;
	}

	return NULL;
}

static const struct measure_clk *find_clock(const struct debugcc_platform *platform,
					    const char *name)
{
	const struct measure_clk *clk;

	for (clk = platform->clocks; clk->name; clk++) {
		if (!strcmp(clk->name, name))
			return clk;
	}

	return NULL;
}

static bool clock_from_block(const struct measure_clk *clk, const char *block_name)
{
	return  !block_name ||
		(!clk->leaf && !strcmp(block_name, CORE_CC_BLOCK)) ||
		(clk->leaf && clk->leaf->block_name && !strcmp(block_name, clk->leaf->block_name));
}

static void list_clocks_block(const struct debugcc_platform *platform, const char *block_name)
{
	const struct measure_clk *clk;

	for (clk = platform->clocks; clk->name; clk++) {
		if (!clock_from_block(clk, block_name))
			continue;

		if (clk->leaf && clk->leaf->block_name)
			printf("%-40s %s\n", clk->name, clk->leaf->block_name);
		else
			printf("%s\n", clk->name);
	}
}

static void list_blocks(const struct debugcc_platform *platform)
{
	const struct debug_mux **mux;

	printf("Available blocks:\n");
	for (mux = platform->blocks; *mux; mux++) {
		if (!(*mux)->block_name)
			printf(" - core\n");
		else
			printf(" - %s\n", (*mux)->block_name);
	}
}

static int usage(void)
{

	fprintf(stderr, "clk debug qcom_clk [-b blk] <-a | -l | -s needle | clk>\n");
	fprintf(stderr, "\n");

	return -EINVAL;
}

int qcom_debugcc_run(int argc, char *const argv[])
{
	const struct debugcc_platform *platform = NULL;
	const struct measure_clk *clk = NULL;
	bool do_list_clocks = false;
	bool all_clocks = false;
	const char *block_name = NULL;
	const char *search = NULL;
	struct getopt_state gs;
	int opt;

	platform = find_platform();
	if (!platform) {
		printf("Couldn't find platform for board\n");
		return -ENODEV;
	}

	getopt_init_state(&gs);

	while ((opt = getopt(&gs, argc, argv, "ab:ls:")) != -1) {
		switch (opt) {
		case 'a':
			all_clocks = true;
			break;
		case 'b':
			block_name = gs.arg;
			break;
		case 'l':
			do_list_clocks = true;
			break;
		case 's':
			search = gs.arg;
			break;
		case ':':
			usage();
			if (gs.opt == 'b') {
				list_blocks(platform);
			}
			return 0;
		default:
			return usage();
			/* NOTREACHED */
		}
	}

	if (do_list_clocks) {
		list_clocks_block(platform, block_name);
		return 0;
	}

	if (!all_clocks && !search) {
		if (gs.index >= argc) {
			printf("index %d argc %d\n", gs.index, argc);
			return usage();
		}

		for (; gs.index < argc; gs.index++) {
			clk = find_clock(platform, argv[gs.index]);
			if (!clk) {
				fprintf(stderr, "no clock named \"%s\"\n", argv[gs.index]);
				return -EINVAL;
			}
		}
	}

	printf("| %-8s | %-40s | %-7s | %-13s |\n", "BLOCK", "CLOCK", "MHz", "Hz");
	printf("+----------+------------------------------------------+---------+---------------+\n");
	if (clk) {
		measure(clk);
	} else {
		for (clk = platform->clocks; clk->name; clk++) {
			if (clock_from_block(clk, block_name) && (!search || strstr(clk->name, search))) {
				measure(clk);
			}
		}
	}

	return 0;
}
