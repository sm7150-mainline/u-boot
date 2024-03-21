// SPDX-License-Identifier: GPL-2.0+
/*
 * DM-free SMEM framework for automagic DTB selection
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#define LOG_DEBUG
#include <asm/armv8/mmu.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/psci.h>
#include <asm/system.h>
#include <dm/device.h>
#include <dm/pinctrl.h>
#include <dm/uclass-internal.h>
#include <dm/read.h>
#include <env.h>
#include <fdt_support.h>
#include <init.h>
#include <linux/arm-smccc.h>
#include <linux/bug.h>
#include <linux/psci.h>
#include <linux/sizes.h>
#include <lmb.h>
#include <malloc.h>
#include <fdt_support.h>
#include <sort.h>
#include <time.h>

#include "qcom-priv.h"

/* No qualcomm platform maps RAM below this address */
#define MIN_RAM_BASE 0x10000000ULL

/* Find the SMEM region by hunting for a matching entry in the pagetable
 * set up by the previous bootloader.
 */
bool pte_cb_detect_smem(u64 start_attrs, u64 end, int va_bits, void *priv)
{
	struct pte_smem_detect_state *state = priv;
	u64 memtype = start_attrs & PMD_ATTRINDX_MASK;
	phys_addr_t addr = start_attrs & GENMASK_ULL(va_bits, PAGE_SHIFT);
	size_t size = end - addr;

	/* Guess the RAM base address */
	if (addr > MIN_RAM_BASE && !state->ram_start) {
		state->ram_start = addr & ~(MIN_RAM_BASE - 1);
		printf("Guessing RAM base at 0x%llx\n", state->ram_start);
	}

	/* Check the address is in roughly the right place - within the first 256M of RAM */
	if (!state->ram_start || addr < state->ram_start || addr > state->ram_start + 0x20000000)
		return false;

	printf("Checking 0x%llx - 0x%llx: %llx (%x)\n", addr, addr + size, memtype, PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE));

	/* SMEM is always mapped nGnRE (I think...) */
	if (memtype != PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE))
		return false;

	/* SMEM is always one of these sizes.
	 * Sometimes if it's size 0x200000 it gets split into
	 * two entries.
	 * NOTE: SDX75 has a non-standard size...
	 */
	if (size != 0x100000 && size != 0x200000)
		return false;

	/* Ok, we found a matching entry. There could be a second entry for the
	 * region so we should check it */
	if (!state->start) {
		state->start = addr;
		state->size = size;
		if (size == 0x100000)
			return false;
	} else if (state->start && size == 0x100000) {
		/* We found the second entry! */
		state->size = 0x200000;
	}

	/* We probably found it? */
	debug("Found SMEM at 0x%llx\n", state->start);

	return true;
}

int qcom_smem_detect(struct pte_smem_detect_state *state)
{
	u64 ttbr, tcr, mair;
	int el;

	memset(state, 0, sizeof(*state));

	el = current_el();
	get_ttbr_tcr_mair(el, &ttbr, &tcr, &mair);

	walk_pagetable(ttbr, tcr, pte_cb_detect_smem, state);
	debug("SMEM: 0x%llx - 0x%llx\n", state->start, state->start + state->size);

	if (!state->start)
		return -ENOENT;

	return 0;
}
