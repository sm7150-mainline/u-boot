// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm UART driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * UART will work in Data Mover mode.
 * Based on Linux driver.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <serial.h>
#include <watchdog.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <dm/pinctrl.h>

/* Serial registers - this driver works in uartdm mode*/

#define UARTDM_DMRX             0x34 /* Max RX transfer length */
#define UARTDM_DMEN             0x3C /* DMA/data-packing mode */
#define UARTDM_NCF_TX           0x40 /* Number of chars to TX */

#define UARTDM_RXFS             0x50 /* RX channel status register */
#define UARTDM_RXFS_BUF_SHIFT   0x7  /* Number of bytes in the packing buffer */
#define UARTDM_RXFS_BUF_MASK    0x7
#define UARTDM_MR1				 0x00
#define UARTDM_MR2				 0x04
#define UARTDM_CSR				 0xA0

#define UARTDM_SR                0xA4 /* Status register */
#define UARTDM_SR_RX_READY       (1 << 0) /* Word is the receiver FIFO */
#define UARTDM_SR_TX_EMPTY       (1 << 3) /* Transmitter underrun */
#define UARTDM_SR_UART_OVERRUN   (1 << 4) /* Receive overrun */

#define UARTDM_CR                         0xA8 /* Command register */
#define UARTDM_CR_CMD_RESET_ERR           (3 << 4) /* Clear overrun error */
#define UARTDM_CR_CMD_RESET_STALE_INT     (8 << 4) /* Clears stale irq */
#define UARTDM_CR_CMD_RESET_TX_READY      (3 << 8) /* Clears TX Ready irq*/
#define UARTDM_CR_CMD_FORCE_STALE         (4 << 8) /* Causes stale event */
#define UARTDM_CR_CMD_STALE_EVENT_DISABLE (6 << 8) /* Disable stale event */

#define UARTDM_IMR                0xB0 /* Interrupt mask register */
#define UARTDM_ISR                0xB4 /* Interrupt status register */
#define UARTDM_ISR_TX_READY       0x80 /* TX FIFO empty */

#define UARTDM_TF               0x100 /* UART Transmit FIFO register */
#define UARTDM_RF               0x140 /* UART Receive FIFO register */

#define UART_DM_CLK_RX_TX_BIT_RATE 0xCC
#define MSM_BOOT_UART_DM_8_N_1_MODE 0x34
#define MSM_BOOT_UART_DM_CMD_RESET_RX 0x10
#define MSM_BOOT_UART_DM_CMD_RESET_TX 0x20

DECLARE_GLOBAL_DATA_PTR;

struct msm_serial_data {
	phys_addr_t base;
	unsigned chars_cnt; /* number of buffered chars */
	uint32_t chars_buf; /* buffered chars */
	uint32_t clk_bit_rate; /* data mover mode bit rate register value */
};

static int msm_serial_fetch(struct udevice *dev)
{
	struct msm_serial_data *priv = dev_get_priv(dev);
	unsigned sr;

	if (priv->chars_cnt)
		return priv->chars_cnt;

	/* Clear error in case of buffer overrun */
	if (readl(priv->base + UARTDM_SR) & UARTDM_SR_UART_OVERRUN)
		writel(UARTDM_CR_CMD_RESET_ERR, priv->base + UARTDM_CR);

	/* We need to fetch new character */
	sr = readl(priv->base + UARTDM_SR);

	if (sr & UARTDM_SR_RX_READY) {
		/* There are at least 4 bytes in fifo */
		priv->chars_buf = readl(priv->base + UARTDM_RF);
		priv->chars_cnt = 4;
	} else {
		/* Check if there is anything in fifo */
		priv->chars_cnt = readl(priv->base + UARTDM_RXFS);
		/* Extract number of characters in UART packing buffer*/
		priv->chars_cnt = (priv->chars_cnt >>
				   UARTDM_RXFS_BUF_SHIFT) &
				  UARTDM_RXFS_BUF_MASK;
		if (!priv->chars_cnt)
			return 0;

		/* There is at least one charcter, move it to fifo */
		writel(UARTDM_CR_CMD_FORCE_STALE,
		       priv->base + UARTDM_CR);

		priv->chars_buf = readl(priv->base + UARTDM_RF);
		writel(UARTDM_CR_CMD_RESET_STALE_INT,
		       priv->base + UARTDM_CR);
		writel(0x7, priv->base + UARTDM_DMRX);
	}

	return priv->chars_cnt;
}

static int msm_serial_getc(struct udevice *dev)
{
	struct msm_serial_data *priv = dev_get_priv(dev);
	char c;

	if (!msm_serial_fetch(dev))
		return -EAGAIN;

	c = priv->chars_buf & 0xFF;
	priv->chars_buf >>= 8;
	priv->chars_cnt--;

	return c;
}

static int msm_serial_putc(struct udevice *dev, const char ch)
{
	struct msm_serial_data *priv = dev_get_priv(dev);

	if (!(readl(priv->base + UARTDM_SR) & UARTDM_SR_TX_EMPTY) &&
	    !(readl(priv->base + UARTDM_ISR) & UARTDM_ISR_TX_READY))
		return -EAGAIN;

	writel(UARTDM_CR_CMD_RESET_TX_READY, priv->base + UARTDM_CR);

	writel(1, priv->base + UARTDM_NCF_TX);
	writel(ch, priv->base + UARTDM_TF);

	return 0;
}

static int msm_serial_pending(struct udevice *dev, bool input)
{
	if (input) {
		if (msm_serial_fetch(dev))
			return 1;
	}

	return 0;
}

static const struct dm_serial_ops msm_serial_ops = {
	.putc = msm_serial_putc,
	.pending = msm_serial_pending,
	.getc = msm_serial_getc,
};

static int msm_uart_clk_init(struct udevice *dev)
{
	uint clk_rate = fdtdec_get_uint(gd->fdt_blob, dev_of_offset(dev),
					"clock-frequency", 115200);
	struct clk clk;
	int ret;

	ret = clk_get_by_name(dev, "core", &clk);
	if (ret < 0) {
		pr_warn("%s: Failed to get clock: %d\n", __func__, ret);
		return ret;
	}

	ret = clk_set_rate(&clk, clk_rate);
	clk_free(&clk);
	if (ret < 0)
		return ret;

	return 0;
}

static void uart_dm_init(struct msm_serial_data *priv)
{
	/* Delay initialization for a bit to let pins stabilize if necessary */
	mdelay(5);

	writel(priv->clk_bit_rate, priv->base + UARTDM_CSR);
	writel(0x0, priv->base + UARTDM_MR1);
	writel(MSM_BOOT_UART_DM_8_N_1_MODE, priv->base + UARTDM_MR2);
	writel(MSM_BOOT_UART_DM_CMD_RESET_RX, priv->base + UARTDM_CR);
	writel(MSM_BOOT_UART_DM_CMD_RESET_TX, priv->base + UARTDM_CR);

	/* Make sure BAM/single character mode is disabled */
	writel(0x0, priv->base + UARTDM_DMEN);
}
static int msm_serial_probe(struct udevice *dev)
{
	int ret;
	struct msm_serial_data *priv = dev_get_priv(dev);

	/* No need to reinitialize the UART after relocation */
	if (gd->flags & GD_FLG_RELOC)
		return 0;

	ret = msm_uart_clk_init(dev);
	if (ret)
		return ret;

	pinctrl_select_state(dev, "uart");
	uart_dm_init(priv);

	return 0;
}

static int msm_serial_of_to_plat(struct udevice *dev)
{
	struct msm_serial_data *priv = dev_get_priv(dev);

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->clk_bit_rate = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
							"bit-rate", UART_DM_CLK_RX_TX_BIT_RATE);

	return 0;
}

static const struct udevice_id msm_serial_ids[] = {
	{ .compatible = "qcom,msm-uartdm-v1.4" },
	{ }
};

U_BOOT_DRIVER(serial_msm) = {
	.name	= "serial_msm",
	.id	= UCLASS_SERIAL,
	.of_match = msm_serial_ids,
	.of_to_plat = msm_serial_of_to_plat,
	.priv_auto	= sizeof(struct msm_serial_data),
	.probe = msm_serial_probe,
	.ops	= &msm_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

#ifdef CONFIG_DEBUG_UART_MSM

static struct msm_serial_data init_serial_data = {
	.base = CONFIG_VAL(DEBUG_UART_BASE),
	.clk_bit_rate = UART_DM_CLK_RX_TX_BIT_RATE,
};

/* Serial dumb device, to reuse driver code */
static struct udevice init_dev = {
	.priv_ = &init_serial_data,
};

#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
	uart_dm_init(&init_serial_data);
}

static inline void _debug_uart_putc(int ch)
{
	struct msm_serial_data *priv = &init_serial_data;

	while (!(readl(priv->base + UARTDM_SR) & UARTDM_SR_TX_EMPTY) &&
	       !(readl(priv->base + UARTDM_ISR) & UARTDM_ISR_TX_READY))
		;

	writel(UARTDM_CR_CMD_RESET_TX_READY, priv->base + UARTDM_CR);

	writel(1, priv->base + UARTDM_NCF_TX);
	writel(ch, priv->base + UARTDM_TF);
}

DEBUG_UART_FUNCS

#endif
