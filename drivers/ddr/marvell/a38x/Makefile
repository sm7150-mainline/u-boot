# SPDX-License-Identifier: GPL-2.0+

obj-$(CONFIG_XPL_BUILD) += mv_ddr_plat.o
obj-$(CONFIG_XPL_BUILD) += mv_ddr_sys_env_lib.o
obj-$(CONFIG_XPL_BUILD) += ddr3_debug.o
obj-$(CONFIG_XPL_BUILD) += ddr3_init.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_bist.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_centralization.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_db.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_hw_algo.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_ip_engine.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_leveling.o
obj-$(CONFIG_XPL_BUILD) += ddr3_training_pbs.o
obj-$(CONFIG_XPL_BUILD) += mv_ddr_build_message.o
obj-$(CONFIG_XPL_BUILD) += mv_ddr_common.o
obj-$(CONFIG_XPL_BUILD) += mv_ddr_spd.o
obj-$(CONFIG_XPL_BUILD) += mv_ddr_topology.o
obj-$(CONFIG_XPL_BUILD) += xor.o

obj-$(CONFIG_ARMADA_38X_SUPPORT_OLD_DDR3_TRAINING) += old/

ifdef CONFIG_DDR4
	obj-$(CONFIG_XPL_BUILD) += mv_ddr4_mpr_pda_if.o
	obj-$(CONFIG_XPL_BUILD) += mv_ddr4_training.o
	obj-$(CONFIG_XPL_BUILD) += mv_ddr4_training_calibration.o
	obj-$(CONFIG_XPL_BUILD) += mv_ddr4_training_db.o
	obj-$(CONFIG_XPL_BUILD) += mv_ddr4_training_leveling.o
endif
