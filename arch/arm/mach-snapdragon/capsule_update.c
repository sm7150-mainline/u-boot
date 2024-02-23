// SPDX-License-Identifier: GPL-2.0+
/*
 * Common initialisation for Qualcomm Snapdragon boards.
 *
 * Copyright (c) 2023 Linaro Ltd.
 * Author: Caleb Connolly <caleb.connolly@linaro.org>
 */

#include <dm/device.h>
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

static int find_boot_partition(const char *partname, struct blk_desc *blk_dev, struct disk_partition *info)
{
	int ret;
	int partnum;

	for (partnum = 1;; partnum++) {
		ret = part_get_info(blk_dev, partnum, info);
		if (ret) {
			return ret;
		}
		if (!strncmp(info->name, partname, 6)) {
			return partnum;
		}
	}

	return 0;
}

void qcom_set_serialno(void)
{
	const char *cmdline = get_cmdline();
	char serial[32];

	if (!cmdline) {
		debug("Failed to get bootargs\n");
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
 * The slot suffix is most easily accessed via the bootargs which are populated by
 * the previous stage bootloader (ABL). However in the future we will likely want to
 * read them directly from the GPT vendor attribute bits.
 *
 * For now only SCSI is supported.
 */
void qcom_configure_capsule_updates(void)
{
	struct blk_desc *desc;
	struct disk_partition info;
	int ret, partnum = -1, devnum;
	char *dfu_string;
	const char *cmdline;
	char partname[7] = "boot";

#ifdef CONFIG_DM_SCSI
	/* Scan for SCSI devices */
	ret = scsi_scan(false);
	if (ret) {
		debug("Failed to scan SCSI devices: %d\n", ret);
		return;
	}
#else
	debug("Qualcomm UEFI CapsuleUpdates requires SCSI support\n");
	return;
#endif

	cmdline = get_cmdline();
	if (!cmdline) {
		debug("Failed to get bootargs\n");
		return;
	}

	/* Some boards might only have one boot partition, so this is optional */
	ret = get_cmdline_option(cmdline, "androidboot.slot_suffix=", &partname[4], 3);
	if (ret < 0)
		debug("Failed to get slot suffix from bootargs (board might be A-only?)\n");

	for(devnum = 0;; devnum++) {
		ret = blk_get_desc(UCLASS_SCSI, devnum, &desc);
		if (ret == -ENODEV)
			break;
		else if (ret)
			continue;
		if (desc->part_type != PART_TYPE_UNKNOWN) {
			partnum = find_boot_partition(partname, desc, &info);
			if (partnum >= 0)
				break;
		}
	}
	if (partnum < 0) {
		debug("Failed to find boot partition\n");
		return;
	}

	dfu_string = malloc(32);

	snprintf(dfu_string, 32, "scsi %d=u-boot-bin part %d", devnum, partnum);
	printf("DFU string: %s\n", dfu_string);

	update_info.dfu_string = dfu_string;
}
