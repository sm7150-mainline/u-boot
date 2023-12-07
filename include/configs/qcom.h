/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration file for Qualcomm Snapdragon boards
 *
 * (C) Copyright 2021 Dzmitry Sankouski <dsankouski@gmail.com>
 * (C) Copyright 2023 Linaro Ltd.
 */

#ifndef __CONFIGS_SNAPDRAGON_H
#define __CONFIGS_SNAPDRAGON_H

#define CFG_SYS_BAUDRATE_TABLE	{ 115200, 230400, 460800, 921600 }

// 2a5aa852-b856-4d97-baa9-5c5f4421551f
#define QUALCOMM_UBOOT_BOOT_IMAGE_GUID \
	EFI_GUID(0x2a5aa852, 0xb856, 0x4d97, 0xba, 0xa9, \
		0x5c, 0x5f, 0x44, 0x21, 0x55, 0x1f)

/* Load addressed are calculated during board_late_init(). See arm/mach-snapdragon/board.c */
#define CFG_EXTRA_ENV_SETTINGS \
	"stdin=serial,button-kbd\0"	\
	"stdout=serial,vidconsole\0"	\
	"stderr=serial,vidconsole\0" \
	"bootcmd=bootm $prevbl_initrd_start_addr\0"

#endif
