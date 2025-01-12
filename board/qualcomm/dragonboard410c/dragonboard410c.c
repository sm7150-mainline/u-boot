// SPDX-License-Identifier: GPL-2.0+
/*
 * Board init file for Dragonboard 410C
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 */

#include <button.h>
#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <env.h>
#include <init.h>
#include <net.h>
#include <usb.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <fdt_support.h>
#include <linux/delay.h>

#include "misc.h"

DECLARE_GLOBAL_DATA_PTR;

int board_usb_init(int index, enum usb_init_type init)
{
	static struct udevice *pmic_gpio;
	static struct gpio_desc hub_reset, usb_sel;
	int ret = 0, node;

	if (!pmic_gpio) {
		ret = uclass_get_device_by_name(UCLASS_GPIO,
						"gpio@c000",
						&pmic_gpio);
		if (ret < 0) {
			printf("Failed to find gpio@c000 node.\n");
			return ret;
		}
	}

	/* Try to request gpios needed to start usb host on dragonboard */
	if (!dm_gpio_is_valid(&hub_reset)) {
		node = fdt_subnode_offset(gd->fdt_blob,
					  dev_of_offset(pmic_gpio),
					  "usb_hub_reset_pm");
		if (node < 0) {
			printf("Failed to find usb_hub_reset_pm dt node.\n");
			return node;
		}
		ret = gpio_request_by_name_nodev(offset_to_ofnode(node),
						 "gpios", 0, &hub_reset, 0);
		if (ret < 0) {
			printf("Failed to request usb_hub_reset_pm gpio.\n");
			return ret;
		}
	}

	if (!dm_gpio_is_valid(&usb_sel)) {
		node = fdt_subnode_offset(gd->fdt_blob,
					  dev_of_offset(pmic_gpio),
					  "usb_sw_sel_pm");
		if (node < 0) {
			printf("Failed to find usb_sw_sel_pm dt node.\n");
			return 0;
		}
		ret = gpio_request_by_name_nodev(offset_to_ofnode(node),
						 "gpios", 0, &usb_sel, 0);
		if (ret < 0) {
			printf("Failed to request usb_sw_sel_pm gpio.\n");
			return ret;
		}
	}

	if (init == USB_INIT_HOST) {
		/* Start USB Hub */
		dm_gpio_set_dir_flags(&hub_reset,
				      GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		mdelay(100);
		/* Switch usb to host connectors */
		dm_gpio_set_dir_flags(&usb_sel,
				      GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
		mdelay(100);
	} else { /* Device */
		/* Disable hub */
		dm_gpio_set_dir_flags(&hub_reset, GPIOD_IS_OUT);
		/* Switch back to device connector */
		dm_gpio_set_dir_flags(&usb_sel, GPIOD_IS_OUT);
	}

	return 0;
}

/* Check for vol- button - if pressed - stop autoboot */
int misc_init_r(void)
{
	struct udevice *btn;
	int ret;
	enum button_state_t state;

	ret = button_get_by_label("vol_down", &btn);
	if (ret < 0) {
		printf("Couldn't find power button!\n");
		return ret;
	}

	state = button_get_state(btn);
	if (state == BUTTON_ON) {
		env_set("preboot", "setenv preboot; fastboot 0");
		printf("vol_down pressed - Starting fastboot.\n");
	}

	return 0;
}

int qcom_late_init(void)
{
	char serial[16];

	memset(serial, 0, 16);
	snprintf(serial, 13, "%x", msm_board_serial());
	env_set("serial#", serial);
	return 0;
}

/* Fixup of DTB for Linux Kernel
 * 1. Fixup installed DRAM.
 * 2. Fixup WLAN/BT Mac address:
 *	First, check if MAC addresses for WLAN/BT exists as environemnt
 *	variables wlanaddr,btaddr. if not, generate a unique address.
 */

int ft_board_setup(void *blob, struct bd_info *bd)
{
	u8 mac[ARP_HLEN];

	msm_fixup_memory(blob);

	if (!eth_env_get_enetaddr("wlanaddr", mac)) {
		msm_generate_mac_addr(mac);
	};

	do_fixup_by_compat(blob, "qcom,wcnss-wlan",
			   "local-mac-address", mac, ARP_HLEN, 1);


	if (!eth_env_get_enetaddr("btaddr", mac)) {
		msm_generate_mac_addr(mac);

/* The BD address is same as WLAN MAC address but with
 * least significant bit flipped.
 */
		mac[0] ^= 0x01;
	};

	do_fixup_by_compat(blob, "qcom,wcnss-bt",
			   "local-bd-address", mac, ARP_HLEN, 1);
	return 0;
}
