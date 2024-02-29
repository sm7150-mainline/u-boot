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

/* Load addressed are calculated during board_late_init(). See arm/mach-snapdragon/board.c */
#define CFG_EXTRA_ENV_SETTINGS \
	"stdin=serial,button-kbd\0"	\
	"stdout=serial,vidconsole\0"	\
	"stderr=serial,vidconsole\0" \
	"preboot=usb start\0" \
	"fastboot=fastboot -l $fastboot_addr_r usb 0\0" \
	"bootmenu_0=Boot first available device=bootflow scan -b\0" \
	"bootmenu_1=Enable USB mass storage=ums 0 scsi 0,1,2,3,4,5\0" \
	"bootmenu_2=Enable fastboot mode=run fastboot\0" \
	"bootmenu_3=Reset device=reset\0" \
	"menucmd=bootmenu\0" \
	"bootcmd=bootflow scan -b\0" /* first entry is default */

#endif
