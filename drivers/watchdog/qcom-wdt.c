// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) Linaro Ltd. 2024
 *
 * Authors:
 *   Caleb Connolly <caleb.connolly@linaro.org>
 *
 * Derived from linux/drivers/watchdog/qcom-wdt.c
 */

#include <common.h>
#include <dm.h>
#include <wdt.h>

#include <asm/io.h>

enum wdt_reg {
	WDT_RST,
	WDT_EN,
	WDT_STS,
	WDT_BARK_TIME,
	WDT_BITE_TIME,
};

struct qcom_wdt_match_data {
	const u32 *offset;
};

struct qcom_wdt {
	void __iomem *base;
	const u32 *layout;
};

static const u32 reg_offset_data_kpss[] = {
	[WDT_RST] = 0x4,
	[WDT_EN] = 0x8,
	[WDT_STS] = 0xC,
	[WDT_BARK_TIME] = 0x10,
	[WDT_BITE_TIME] = 0x14,
};

static const struct qcom_wdt_match_data match_data_kpss = {
	.offset = reg_offset_data_kpss,
};

static void __iomem *wdt_addr(struct qcom_wdt *wdt, enum wdt_reg reg)
{
	return wdt->base + wdt->layout[reg];
}

int qcom_wdt_start(struct udevice *dev, u64 timeout_ms, ulong flags)
{
	/* unimplemented */
	return 0;
}

int qcom_wdt_stop(struct udevice *dev)
{
	struct qcom_wdt *wdt = dev_get_priv(dev);

	writel(0, wdt_addr(wdt, WDT_EN));
	if (readl(wdt_addr(wdt, WDT_EN))) {
		printf("Failed to disable Qualcomm watchdog!\n");
		return -EIO;
	}

	return 0;
}

static int qcom_wdt_probe(struct udevice *dev)
{
	struct qcom_wdt *wdt = dev_get_priv(dev);
	struct qcom_wdt_match_data *data = (void *)dev_get_driver_data(dev);
	int ret;
	
	wdt->base = dev_read_addr_ptr(dev);
	wdt->layout = data->offset;

	ret = qcom_wdt_stop(dev);

	return ret;
}

static const struct wdt_ops qcom_wdt_ops = {
	.stop = qcom_wdt_stop,
};

static const struct udevice_id qcom_wdt_ids[] = {
	{ .compatible = "qcom,kpss-wdt", .data = (ulong)&match_data_kpss },
	{}
};

U_BOOT_DRIVER(qcom_wdt) = {
	.name = "qcom_wdt",
	.id = UCLASS_WDT,
	.of_match = qcom_wdt_ids,
	.ops = &qcom_wdt_ops,
	.probe = qcom_wdt_probe,
	.priv_auto = sizeof(struct qcom_wdt),
};