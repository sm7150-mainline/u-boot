// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm Snapdragon boards.
 *
 * Copyright (c) 2024 Linaro Ltd.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#define LOG_CATEGORY LOGC_BOARD
#define pr_fmt(fmt) "OF Match: " fmt
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
#include <usb.h>
#include <sort.h>
#include <time.h>

#include "qcom-priv.h"

DECLARE_GLOBAL_DATA_PTR;

struct qcom_match {
	const fdt64_t *memory;
	int memsize;
	const char *bootargs;
};

static int is_match_memory(struct qcom_match *data, ofnode node)
{
	const u64 *memory;
	int memsize;

	memory = ofnode_read_prop(node, "match,memory", &memsize);
	if (!memory)
		return -ENODATA;

	if (memsize != data->memsize)
		return false;

	if (memcmp(memory, data->memory, memsize))
		return false;

	return true;
}

static int is_match_bootargs(struct qcom_match *data, ofnode node)
{
	const char *bootargs;

	bootargs = ofnode_read_string(node, "match,bootargs");
	if (!bootargs)
		return -ENODATA;

	if (!strstr(data->bootargs, bootargs))
		return false;

	return true;
}

static bool is_match(struct qcom_match *data, ofnode node)
{
	int ret;

	ret = is_match_memory(data, node);
	if (!ret) {
		log_debug("%s: memory mismatch\n", ofnode_get_name(node));
		return false;
	}

	ret = is_match_bootargs(data, node);
	if (!ret) {
		log_debug("%s: bootargs mismatch\n", ofnode_get_name(node));
		return false;
	}

	return true;
}

const char *qcom_of_match(void *external_fdt)
{
	struct qcom_match data;
	ofnode node, match_node;

	/*
	 * To keep things simple, we just override gd->fdt_blob.
	 * this function should only be called from board_fdt_blob_setup()
	 * anyway, the return value of which will clobber gd->fdt_blob
	 * regardless.
	 */
	gd->fdt_blob = external_fdt;

	match_node = ofnode_path("/chosen/match");
	if (!ofnode_valid(match_node)) {
		log_debug("No match node found in device tree!\n");
		return NULL;
	}

	node = ofnode_path("/memory");
	if (!ofnode_valid(node)) {
		log_debug("No memory node found in device tree!\n");
		return NULL;
	}
	data.memory = ofnode_read_prop(node, "reg", &data.memsize);
	if (!data.memory) {
		log_err("No memory configuration was provided by the previous bootloader!\n");
		return NULL;
	}

	node = ofnode_path("/chosen");
	if (!ofnode_valid(node)) {
		log_debug("No chosen node found in device tree!\n");
		return NULL;
	}
	data.bootargs = ofnode_read_string(node, "bootargs");

	ofnode_for_each_subnode(node, match_node) {
		if (is_match(&data, node)) {
			log_debug("found %s\n", ofnode_get_name(node));
			return ofnode_get_name(node);
		}
	}

	return NULL;
}
