// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Bhupesh Sharma <bhupesh.sharma@linaro.org>
 *
 * Based on Linux driver
 */

#include <asm/io.h>
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <ufs.h>

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>

#include "ufs.h"
#include "ufs-qcom.h"

#define MSEC_PER_SEC	(1000L)
#define USEC_PER_SEC	(1000000L)
#define NSEC_PER_SEC	(1000000000L)

static int ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(struct ufs_hba *hba,
						       u32 clk_cycles);

static int ufs_qcom_clk_get(struct udevice *dev,
		const char *name, struct clk **clk_out, bool optional)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (!IS_ERR(clk)) {
		*clk_out = clk;
		return 0;
	}

	err = PTR_ERR(clk);

	if (optional && err == -ENOENT) {
		*clk_out = NULL;
		return 0;
	}

	if (err != -EPROBE_DEFER)
		dev_err(dev, "failed to get %s err %d\n", name, err);

	return err;
}

static int ufs_qcom_clk_enable(struct udevice *dev,
		const char *name, struct clk *clk)
{
	int err = 0;

	err = clk_prepare_enable(clk);
	if (err)
		dev_err(dev, "%s: %s enable failed %d\n", __func__, name, err);

	return err;
}

static int ufs_qcom_enable_lane_clks(struct ufs_qcom_priv *priv)
{
	int err;
	struct udevice *dev = priv->hba->dev;

	if (priv->is_lane_clks_enabled)
		return 0;

	err = ufs_qcom_clk_enable(dev, "rx_lane0_sync_clk",
		priv->rx_l0_sync_clk);
	if (err)
		return err;

	err = ufs_qcom_clk_enable(dev, "tx_lane0_sync_clk",
		priv->tx_l0_sync_clk);
	if (err)
		goto disable_rx_l0;

	err = ufs_qcom_clk_enable(dev, "rx_lane1_sync_clk",
		priv->rx_l1_sync_clk);
	if (err)
		goto disable_tx_l0;

	priv->is_lane_clks_enabled = true;

	return 0;

disable_tx_l0:
	clk_disable_unprepare(priv->tx_l0_sync_clk);
disable_rx_l0:
	clk_disable_unprepare(priv->rx_l0_sync_clk);

	return err;
}

static int ufs_qcom_init_lane_clks(struct ufs_qcom_priv *priv)
{
	int err = 0;
	struct udevice *dev = priv->hba->dev;

	err = ufs_qcom_clk_get(dev, "rx_lane0_sync_clk",
					&priv->rx_l0_sync_clk, false);
	if (err)
		return err;

	err = ufs_qcom_clk_get(dev, "tx_lane0_sync_clk",
					&priv->tx_l0_sync_clk, false);
	if (err)
		return err;

	err = ufs_qcom_clk_get(dev, "rx_lane1_sync_clk",
					&priv->rx_l1_sync_clk, false);
	if (err)
		return err;

	return 0;
}

static int ufs_qcom_enable_core_clks(struct ufs_qcom_priv *priv)
{
	int err;
	struct udevice *dev = priv->hba->dev;

	if (priv->is_core_clks_enabled)
		return 0;

	err = ufs_qcom_clk_enable(dev, "core_clk", priv->core_clk);
	if (err)
		return err;

	err = ufs_qcom_clk_enable(dev, "bus_aggr_clk", priv->bus_aggr_clk);
	if (err)
		goto disable_core_clk;

	err = ufs_qcom_clk_enable(dev, "iface_clk", priv->iface_clk);
	if (err)
		goto disable_bus_aggr_clk;

	err = ufs_qcom_clk_enable(dev, "core_clk_unipro", priv->core_clk_unipro);
	if (err)
		goto disable_iface_clk;

	priv->is_core_clks_enabled = true;

	return 0;

disable_iface_clk:
	clk_disable_unprepare(priv->iface_clk);
disable_bus_aggr_clk:
	clk_disable_unprepare(priv->bus_aggr_clk);
disable_core_clk:
	clk_disable_unprepare(priv->core_clk);

	return err;
}

static int ufs_qcom_init_core_clks(struct ufs_qcom_priv *priv)
{
	int err = 0;
	struct udevice *dev = priv->hba->dev;

	err = ufs_qcom_clk_get(dev, "core_clk",
					&priv->core_clk, false);
	if (err)
		return err;

	err = ufs_qcom_clk_get(dev, "bus_aggr_clk",
					&priv->bus_aggr_clk, false);
	if (err)
		return err;

	err = ufs_qcom_clk_get(dev, "iface_clk", &priv->iface_clk, false);
	if (err)
		return err;

	err = ufs_qcom_clk_get(dev, "core_clk_unipro", &priv->core_clk_unipro, false);
	if (err)
		return err;

	/* ref_clk is optional */

	return 0;
}

static void ufs_qcom_select_unipro_mode(struct ufs_qcom_priv *priv)
{
	ufshcd_rmwl(priv->hba, QUNIPRO_SEL,
		   ufs_qcom_cap_qunipro(priv) ? QUNIPRO_SEL : 0,
		   REG_UFS_CFG1);

	if (priv->hw_ver.major == 0x05)
		ufshcd_rmwl(priv->hba, QUNIPRO_G4_SEL, 0, REG_UFS_CFG0);

	/* make sure above configuration is applied before we return */
	mb();
}

/*
 * ufs_qcom_reset - reset host controller and PHY
 */
static int ufs_qcom_reset(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	int ret = 0;

	ret = reset_assert(&priv->core_reset);
	if (ret) {
		dev_err(hba->dev, "%s: core_reset assert failed, err = %d\n",
				 __func__, ret);
		return ret;
	}

	/*
	 * The hardware requirement for delay between assert/deassert
	 * is at least 3-4 sleep clock (32.7KHz) cycles, which comes to
	 * ~125us (4/32768). To be on the safe side add 200us delay.
	 */
	udelay(210);

	ret = reset_deassert(&priv->core_reset);
	if (ret)
		dev_err(hba->dev, "%s: core_reset deassert failed, err = %d\n",
				 __func__, ret);

	udelay(1100);

	return 0;
}

static void ufs_qcom_set_caps(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);

	if (priv->hw_ver.major >= 0x2) {
		priv->caps = UFS_QCOM_CAP_QUNIPRO |
			     UFS_QCOM_CAP_RETAIN_SEC_CFG_AFTER_PWR_COLLAPSE;
	}
}

/**
 * ufs_qcom_advertise_quirks - advertise the known QCOM UFS controller quirks
 * @hba: host controller instance
 *
 * QCOM UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void ufs_qcom_advertise_quirks(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);

	if (priv->hw_ver.major == 0x01) {
		hba->quirks |= UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
			    | UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP
			    | UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE;

		if (priv->hw_ver.minor == 0x0001 && priv->hw_ver.step == 0x0001)
			hba->quirks |= UFSHCD_QUIRK_BROKEN_INTR_AGGR;

		hba->quirks |= UFSHCD_QUIRK_BROKEN_LCC;
	}

	if (priv->hw_ver.major == 0x2) {
		hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION;

		if (!ufs_qcom_cap_qunipro(priv))
			/* Legacy UniPro mode still need following quirks */
			hba->quirks |= (UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
				| UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE
				| UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP);
	}

	if (priv->hw_ver.major > 0x3)
		hba->quirks |= UFSHCD_QUIRK_REINIT_AFTER_MAX_GEAR_SWITCH;
}

static void ufs_qcom_dev_ref_clk_ctrl(struct ufs_hba *hba, bool enable)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);

	if (priv->dev_ref_clk_ctrl_mmio &&
	    (enable ^ priv->is_dev_ref_clk_enabled)) {
		u32 temp = readl_relaxed(priv->dev_ref_clk_ctrl_mmio);

		if (enable)
			temp |= priv->dev_ref_clk_en_mask;
		else
			temp &= ~priv->dev_ref_clk_en_mask;

		/*
		 * If we are here to disable this clock it might be immediately
		 * after entering into hibern8 in which case we need to make
		 * sure that device ref_clk is active for specific time after
		 * hibern8 enter.
		 */
		if (!enable)
			udelay(10);

		writel_relaxed(temp, priv->dev_ref_clk_ctrl_mmio);

		/*
		 * Make sure the write to ref_clk reaches the destination and
		 * not stored in a Write Buffer (WB).
		 */
		readl(priv->dev_ref_clk_ctrl_mmio);

		/*
		 * If we call hibern8 exit after this, we need to make sure that
		 * device ref_clk is stable for at least 1us before the hibern8
		 * exit command.
		 */
		if (enable)
			udelay(1);

		priv->is_dev_ref_clk_enabled = enable;
	}
}

/**
 * ufs_qcom_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_qcom_setup_clocks(struct ufs_hba *hba, bool on,
				 enum ufs_notify_change_status status)
{
	switch (status) {
	case PRE_CHANGE:
		if (!on) {
			/* disable device ref_clk */
			ufs_qcom_dev_ref_clk_ctrl(hba, false);
		}
		break;
	case POST_CHANGE:
		if (on) {
			/* enable the device ref clock for HS mode*/
			ufs_qcom_dev_ref_clk_ctrl(hba, true);
		}
		break;
	}

	return 0;
}

static int ufs_qcom_power_up_sequence(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	struct phy phy;
	int ret;

	/* Reset UFS Host Controller and PHY */
	ret = ufs_qcom_reset(hba);
	if (ret)
		dev_warn(hba->dev, "%s: host reset returned %d\n",
				  __func__, ret);

	/* get phy */
	ret = generic_phy_get_by_name(hba->dev, "ufsphy", &phy);
	if (ret) {
		dev_warn(hba->dev, "%s: Unable to get QMP ufs phy, ret = %d\n",
			 __func__, ret);
		return ret;
	}

	/* phy initialization */
	ret = generic_phy_init(&phy);
	if (ret) {
		dev_err(hba->dev, "%s: phy init failed, ret = %d\n",
			__func__, ret);
		return ret;
	}

	// ufshcd_rmwl(hba, UFS_PHY_SOFT_RESET, UFS_PHY_SOFT_RESET, REG_UFS_CFG1);
	// udelay(1100);
	ufshcd_rmwl(hba, UFS_PHY_SOFT_RESET, 0, REG_UFS_CFG1);
	udelay(1100);

	/* power on phy */
	ret = generic_phy_power_on(&phy);
	if (ret) {
		dev_err(hba->dev, "%s: phy power on failed, ret = %d\n",
			__func__, ret);
		goto out_disable_phy;
	}

	ufs_qcom_select_unipro_mode(priv);

	return 0;

out_disable_phy:
	generic_phy_exit(&phy);

	return ret;
}

static int ufs_qcom_check_hibern8(struct ufs_hba *hba)
{
	int err, retry_count = 50;
	u32 tx_fsm_val = 0;

	do {
		err = ufshcd_dme_get(hba,
				UIC_ARG_MIB_SEL(MPHY_TX_FSM_STATE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				&tx_fsm_val);
		if (err || tx_fsm_val == TX_FSM_HIBERN8)
			break;

		/* max. 200us */
		udelay(200);
		retry_count--;
	} while (retry_count != 0);

	/*
	 * check the state again.
	 */
	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(MPHY_TX_FSM_STATE,
				UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				&tx_fsm_val);

	if (err) {
		dev_err(hba->dev, "%s: unable to get TX_FSM_STATE, err %d\n",
				__func__, err);
	} else if (tx_fsm_val != TX_FSM_HIBERN8) {
		err = tx_fsm_val;
		dev_err(hba->dev, "%s: invalid TX_FSM_STATE = %d\n",
				__func__, err);
	}

	return err;
}

/*
 * The UTP controller has a number of internal clock gating cells (CGCs).
 * Internal hardware sub-modules within the UTP controller control the CGCs.
 * Hardware CGCs disable the clock to inactivate UTP sub-modules not involved
 * in a specific operation, UTP controller CGCs are by default disabled and
 * this function enables them (after every UFS link startup) to save some power
 * leakage.
 */
static void ufs_qcom_enable_hw_clk_gating(struct ufs_hba *hba)
{
	ufshcd_writel(hba,
		ufshcd_readl(hba, REG_UFS_CFG2) | REG_UFS_CFG2_CGC_EN_ALL,
		REG_UFS_CFG2);

	/* Ensure that HW clock gating is enabled before next operations */
	mb();
}

static int ufs_qcom_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		ufs_qcom_power_up_sequence(hba);
		/*
		 * The PHY PLL output is the source of tx/rx lane symbol
		 * clocks, hence, enable the lane clocks only after PHY
		 * is initialized.
		 */
		err = ufs_qcom_enable_core_clks(priv);
		if (err < 0)
			return err;

		err = ufs_qcom_enable_lane_clks(priv);
		if (err < 0)
			return err;
		break;
	case POST_CHANGE:
		/* check if UFS PHY moved from DISABLED to HIBERN8 */
		err = ufs_qcom_check_hibern8(hba);
		ufs_qcom_enable_hw_clk_gating(hba);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

/*
 * Returns zero for success and non-zero in case of a failure
 */
static int ufs_qcom_cfg_timers(struct ufs_hba *hba, u32 gear,
			       u32 hs, u32 rate, bool update_link_startup_timer)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	u32 core_clk_period_in_ns;
	u32 tx_clk_cycles_per_us = 0;
	unsigned long core_clk_rate = 0;
	u32 core_clk_cycles_per_us = 0;

	static u32 pwm_fr_table[][2] = {
		{UFS_PWM_G1, 0x1},
		{UFS_PWM_G2, 0x1},
		{UFS_PWM_G3, 0x1},
		{UFS_PWM_G4, 0x1},
	};

	static u32 hs_fr_table_rA[][2] = {
		{UFS_HS_G1, 0x1F},
		{UFS_HS_G2, 0x3e},
		{UFS_HS_G3, 0x7D},
	};

	static u32 hs_fr_table_rB[][2] = {
		{UFS_HS_G1, 0x24},
		{UFS_HS_G2, 0x49},
		{UFS_HS_G3, 0x92},
	};

	if (gear == 0) {
		dev_err(hba->dev, "%s: invalid gear = %d\n", __func__, gear);
		return -EINVAL;
	}

	core_clk_rate = clk_get_rate(priv->core_clk);

	/* If frequency is smaller than 1MHz, set to 1MHz */
	if (core_clk_rate < DEFAULT_CLK_RATE_HZ || IS_ERR_VALUE(core_clk_rate))
		core_clk_rate = DEFAULT_CLK_RATE_HZ;

	debug("%s: core_clk_rate: %lu\n", __func__, core_clk_rate);

	core_clk_cycles_per_us = core_clk_rate / USEC_PER_SEC;
	if (ufshcd_readl(hba, REG_UFS_SYS1CLK_1US) != core_clk_cycles_per_us) {
		ufshcd_writel(hba, core_clk_cycles_per_us, REG_UFS_SYS1CLK_1US);
		ufshcd_readl(hba, REG_UFS_SYS1CLK_1US);
		/*
		 * make sure above write gets applied before we return from
		 * this function.
		 */
		mb();
	}

	core_clk_period_in_ns = NSEC_PER_SEC / core_clk_rate;
	core_clk_period_in_ns <<= OFFSET_CLK_NS_REG;
	core_clk_period_in_ns &= MASK_CLK_NS_REG;

	switch (hs) {
	case FASTAUTO_MODE:
	case FAST_MODE:
		if (rate == PA_HS_MODE_A) {
			if (gear > ARRAY_SIZE(hs_fr_table_rA)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rA));
				return -EINVAL;
			}
			tx_clk_cycles_per_us = hs_fr_table_rA[gear-1][1];
		} else if (rate == PA_HS_MODE_B) {
			if (gear > ARRAY_SIZE(hs_fr_table_rB)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rB));
				return -EINVAL;
			}
			tx_clk_cycles_per_us = hs_fr_table_rB[gear-1][1];
		} else {
			dev_err(hba->dev, "%s: invalid rate = %d\n",
				__func__, rate);
			return -EINVAL;
		}
		break;
	case SLOWAUTO_MODE:
	case SLOW_MODE:
		if (gear > ARRAY_SIZE(pwm_fr_table)) {
			dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(pwm_fr_table));
			return -EINVAL;
		}
		tx_clk_cycles_per_us = pwm_fr_table[gear-1][1];
		break;
	case UNCHANGED:
	default:
		dev_err(hba->dev, "%s: invalid mode = %d\n", __func__, hs);
		return -EINVAL;
	}

	if (ufshcd_readl(hba, REG_UFS_TX_SYMBOL_CLK_NS_US) !=
	    (core_clk_period_in_ns | tx_clk_cycles_per_us)) {
		/* this register 2 fields shall be written at once */
		ufshcd_writel(hba, core_clk_period_in_ns | tx_clk_cycles_per_us,
			      REG_UFS_TX_SYMBOL_CLK_NS_US);
		/*
		 * make sure above write gets applied before we return from
		 * this function.
		 */
		mb();
	}

	if (update_link_startup_timer && priv->hw_ver.major != 0x5) {
		ufshcd_writel(hba, ((core_clk_rate / MSEC_PER_SEC) * 100),
			      REG_UFS_CFG0);
		/*
		 * make sure that this configuration is applied before
		 * we return
		 */
		mb();
	}

	return 0;
}

static int ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(struct ufs_hba *hba,
						       u32 clk_cycles)
{
	int err;
	u32 core_clk_ctrl_reg;

	if (clk_cycles > DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK)
		return -EINVAL;

	err = ufshcd_dme_get(hba,
			    UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
			    &core_clk_ctrl_reg);
	if (err)
		return err;

	core_clk_ctrl_reg &= ~DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK;
	core_clk_ctrl_reg |= clk_cycles;

	/* Clear CORE_CLK_DIV_EN */
	core_clk_ctrl_reg &= ~DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT;

	return ufshcd_dme_set(hba,
			    UIC_ARG_MIB(DME_VS_CORE_CLK_CTRL),
			    core_clk_ctrl_reg);
}

/* TBD: Move this to common framework layer */
u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba)
{
	/* HCI version 1.0 and 1.1 supports UniPro 1.41 */
	switch (hba->version) {
	case UFSHCI_VERSION_10:
	case UFSHCI_VERSION_11:
		return UFS_UNIPRO_VER_1_41;

	case UFSHCI_VERSION_20:
	case UFSHCI_VERSION_21:
	default:
		return UFS_UNIPRO_VER_1_6;
	}
}

static int ufs_qcom_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		if (ufs_qcom_cfg_timers(hba, UFS_HS_G3, FAST_MODE,
					PA_HS_MODE_B, true)) {
			dev_err(hba->dev, "%s: ufs_qcom_cfg_timers() failed\n",
				__func__);
			return -EINVAL;
		}

		if (ufs_qcom_cap_qunipro(priv))
			/*
			 * set unipro core clock cycles to 150 & clear clock
			 * divider
			 */
			err = ufs_qcom_set_dme_vs_core_clk_ctrl_clear_div(hba,
									  300);

		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE), 0);

		break;
	default:
		break;
	}

	return err;
}

/**
 * ufs_qcom_init - bind phy with controller
 * @hba: host controller instance
 *
 * Powers up PHY enabling clocks and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_qcom_init(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);
	int err;

	priv->hba = hba;

	/* setup clocks */
	ufs_qcom_setup_clocks(hba, true, PRE_CHANGE);
	ufs_qcom_setup_clocks(hba, true, POST_CHANGE);

	ufs_qcom_get_controller_revision(hba, &priv->hw_ver.major,
		&priv->hw_ver.minor, &priv->hw_ver.step);
	dev_info(hba->dev, "Qcom UFS HC version: %d.%d.%d\n", priv->hw_ver.major,
		priv->hw_ver.minor, priv->hw_ver.step);

	/*
	 * for newer controllers, device reference clock control bit has
	 * moved inside UFS controller register address space itself.
	 */
	if (priv->hw_ver.major >= 0x02) {
		priv->dev_ref_clk_ctrl_mmio = hba->mmio_base + REG_UFS_CFG1;
		priv->dev_ref_clk_en_mask = BIT(26);
	}

	err = ufs_qcom_init_core_clks(priv);
	if (err) {
		dev_err(hba->dev, "failed to initialize core clocks, err:%d\n", err);
		return err;
	}

	err = ufs_qcom_init_lane_clks(priv);
	if (err) {
		dev_err(hba->dev, "failed to initialize lane clocks, err:%d\n", err);
		return err;
	}

	ufs_qcom_set_caps(hba);
	ufs_qcom_advertise_quirks(hba);
	ufs_qcom_setup_clocks(hba, true, POST_CHANGE);

	/* Power up the PHY using UFS_HS_G3. */
	priv->hs_gear = UFS_HS_G3;

	return 0;
}

static void ufshcd_print_clk_freqs(struct ufs_hba *hba)
{
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);

	dev_info(hba->dev, "clk: %s, rate: %lu\n", "bus_aggr_clk",
		       clk_get_rate(priv->bus_aggr_clk));
	dev_info(hba->dev, "clk: %s, rate: %lu\n", "iface_clk",
		       clk_get_rate(priv->iface_clk));
	dev_info(hba->dev, "clk: %s, rate: %lu\n", "core_clk_unipro",
		       clk_get_rate(priv->core_clk_unipro));
}

static void ufs_qcom_dump_dbg_regs(struct ufs_hba *hba)
{
	u32 reg;
	struct ufs_qcom_priv *priv = dev_get_priv(hba->dev);

	ufshcd_dump_regs(hba, REG_UFS_SYS1CLK_1US, 16 * 4,
			 "HCI Vendor Specific Registers ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_UFS_DBG_RD_REG_OCSC);
	ufshcd_dump_regs(hba, reg, 44 * 4, "UFS_UFS_DBG_RD_REG_OCSC ");

	reg = ufshcd_readl(hba, REG_UFS_CFG1);
	reg |= UTP_DBG_RAMS_EN;
	ufshcd_writel(hba, reg, REG_UFS_CFG1);

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_UFS_DBG_RD_EDTL_RAM);
	ufshcd_dump_regs(hba, reg, 32 * 4, "UFS_UFS_DBG_RD_EDTL_RAM ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_UFS_DBG_RD_DESC_RAM);
	ufshcd_dump_regs(hba, reg, 128 * 4, "UFS_UFS_DBG_RD_DESC_RAM ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_UFS_DBG_RD_PRDT_RAM);
	ufshcd_dump_regs(hba, reg, 64 * 4, "UFS_UFS_DBG_RD_PRDT_RAM ");

	/* clear bit 17 - UTP_DBG_RAMS_EN */
	ufshcd_rmwl(hba, UTP_DBG_RAMS_EN, 0, REG_UFS_CFG1);

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_UAWM);
	ufshcd_dump_regs(hba, reg, 4 * 4, "UFS_DBG_RD_REG_UAWM ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_UARM);
	ufshcd_dump_regs(hba, reg, 4 * 4, "UFS_DBG_RD_REG_UARM ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_TXUC);
	ufshcd_dump_regs(hba, reg, 48 * 4, "UFS_DBG_RD_REG_TXUC ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_RXUC);
	ufshcd_dump_regs(hba, reg, 27 * 4, "UFS_DBG_RD_REG_RXUC ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_DFC);
	ufshcd_dump_regs(hba, reg, 19 * 4, "UFS_DBG_RD_REG_DFC ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_TRLUT);
	ufshcd_dump_regs(hba, reg, 34 * 4, "UFS_DBG_RD_REG_TRLUT ");

	reg = ufs_qcom_get_debug_reg_offset(priv, UFS_DBG_RD_REG_TMRLUT);
	ufshcd_dump_regs(hba, reg, 9 * 4, "UFS_DBG_RD_REG_TMRLUT ");

	ufshcd_print_clk_freqs(hba);
}

static struct ufs_hba_ops ufs_qcom_hba_ops = {
	.init			= ufs_qcom_init,
	.dbg_register_dump	= ufs_qcom_dump_dbg_regs,
	.hce_enable_notify	= ufs_qcom_hce_enable_notify,
	.link_startup_notify	= ufs_qcom_link_startup_notify,
};

static int ufs_qcom_probe(struct udevice *dev)
{
	struct ufs_qcom_priv *priv = dev_get_priv(dev);
	int ret;

	/* get resets */
	ret = reset_get_by_name(dev, "rst", &priv->core_reset);
	if (ret) {
		dev_err(dev, "failed to get reset, ret:%d\n", ret);
		return ret;
	}

	ret = ufshcd_probe(dev, &ufs_qcom_hba_ops);
	if (ret) {
		dev_err(dev, "ufshcd_probe() failed, ret:%d\n", ret);
		return ret;
	}

	return 0;
}

static int ufs_qcom_bind(struct udevice *dev)
{
	struct udevice *scsi_dev;

	return ufs_scsi_bind(dev, &scsi_dev);
}

static const struct udevice_id ufs_qcom_ids[] = {
	{ .compatible = "qcom,ufshc" },
	{},
};

U_BOOT_DRIVER(qcom_ufshcd) = {
	.name		= "qcom-ufshcd",
	.id		= UCLASS_UFS,
	.of_match	= ufs_qcom_ids,
	.probe		= ufs_qcom_probe,
	.bind		= ufs_qcom_bind,
	.priv_auto	= sizeof(struct ufs_qcom_priv),
};
