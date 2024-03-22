// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm Snapdragon boards.
 *
 * Copyright (c) 2023 Linaro Ltd.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#define pr_fmt(fmt) "CapsuleUpdate: " fmt

#include <dm/device.h>
#include <dm/uclass.h>
#include <efi.h>
#include <efi_loader.h>
#include <malloc.h>
#include <scsi.h>
#include <part.h>

#include "qcom-priv.h"

struct efi_fw_image fw_images[] = {
	{
		.image_type_id = QUALCOMM_UBOOT_BOOT_IMAGE_GUID,
		.fw_name = u"QUALCOMM-UBOOT",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	/* Filled in by configure_dfu_string() */
	.dfu_string = NULL,
	.num_images = ARRAY_SIZE(fw_images),
	.images = fw_images,
};

/**
 * out_len includes the trailing null space
 */
static int get_cmdline_option(const char *cmdline, const char *key, char *out, int out_len)
{
	const char *p, *p_end;
	int len;

	p = strstr(cmdline, key);
	if (!p)
		return -ENOENT;

	p += strlen(key);
	p_end = strstr(p, " ");
	if (!p_end)
		return -ENOENT;

	len = p_end - p;
	if (len > out_len)
		len = out_len;

	strncpy(out, p, len);
	out[len] = '\0';

	return 0;
}

/* The bootargs are populated by the previous stage bootloader */
static const char *get_cmdline(void)
{
	ofnode node;
	static const char *cmdline = NULL;

	if (cmdline)
		return cmdline;
	
	node = ofnode_path("/chosen");
	if (!ofnode_valid(node))
		return NULL;

	cmdline = ofnode_read_string(node, "bootargs");

	return cmdline;
}

struct part_slot_status {
	u16 : 2;
	u16 active : 1;
	u16 : 3;
	u16 successful : 1;
	u16 unbootable : 1;
	u16 tries_remaining : 4;
};

#define SLOT_STATUS(info) ((struct part_slot_status*)&info->type_flags)

static int find_boot_partition(const char *partname, struct blk_desc *blk_dev, struct disk_partition *info)
{
	int ret;
	int partnum;

	for (partnum = 1;; partnum++) {
		ret = part_get_info(blk_dev, partnum, info);
		if (ret) {
			return ret;
		}
		if (!strncmp(info->name, partname, strlen(partname)) && SLOT_STATUS(info)->active) {
			log_debug("Found active %s partition!\n", partname);
			return partnum;
		}
	}

	return -1;
}

void qcom_set_serialno(void)
{
	const char *cmdline = get_cmdline();
	char serial[32];

	if (!cmdline) {
		log_debug("Failed to get bootargs\n");
		return;
	}

	get_cmdline_option(cmdline, "androidboot.serialno=", serial, sizeof(serial));
	if (serial[0] != '\0') {
		env_set("serial#", serial);
	}
}

/**
 * qcom_configure_capsule_updates() - Configure the DFU string for capsule updates
 *
 * U-Boot is flashed to the boot partition on Qualcomm boards. In most cases there
 * are two boot partitions, boot_a and boot_b. As we don't currently support doing
 * full A/B updates, we only support updating the currently active boot partition.
 *
 * So we need to find the current slot suffix and the associated boot partition.
 * We do this by looking for the boot partition that has the 'active' flag set
 * in the GPT partition vendor attribute bits.
 */
void qcom_configure_capsule_updates(void)
{
	struct blk_desc *desc;
	struct disk_partition info;
	int ret = 0, partnum = -1, devnum;
	static char dfu_string[32] = { 0 };
	char *partname = "boot";
	struct udevice *dev = NULL;

#ifdef CONFIG_SCSI
	/* Scan for SCSI devices */
	ret = scsi_scan(false);
	if (ret) {
		debug("Failed to scan SCSI devices: %d\n", ret);
		return;
	}
#else
	debug("Qualcomm capsule update running without SCSI support!\n");
#endif

	uclass_foreach_dev_probe(UCLASS_BLK, dev) {
		if (device_get_uclass_id(dev) != UCLASS_BLK)
			continue;

		desc = dev_get_uclass_plat(dev);
		if (!desc || desc->part_type == PART_TYPE_UNKNOWN)
			continue;
		devnum = desc->devnum;
		partnum = find_boot_partition(partname, desc,
					      &info);
		if (partnum >= 0)
			break;
	}

	if (partnum < 0) {
		log_err("Failed to find boot partition\n");
		return;
	}

	switch(desc->uclass_id) {
	case UCLASS_SCSI:
		snprintf(dfu_string, 32, "scsi %d=u-boot-bin part %d", devnum, partnum);
		break;
	case UCLASS_MMC:
		snprintf(dfu_string, 32, "mmc part %d:%d=u-boot-bin", devnum, partnum);
		break;
	default:
		debug("Unsupported storage uclass: %d\n", desc->uclass_id);
		return;
	}
	log_debug("U-Boot boot partition is %s\n", info.name);
	log_debug("DFU string: %s\n", dfu_string);

	update_info.dfu_string = dfu_string;
}
