/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015, 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 */
#ifndef __QCOM_SCM_H
#define __QCOM_SCM_H

#include <linux/err.h>
#include <linux/types.h>

#include <dt-bindings/firmware/qcom,scm.h>

#define QCOM_SCM_VERSION(major, minor)	(((major) << 16) | ((minor) & 0xFF))

struct qcom_scm_vmperm {
	int vmid;
	int perm;
};

#define QCOM_SCM_PERM_READ       0x4
#define QCOM_SCM_PERM_WRITE      0x2
#define QCOM_SCM_PERM_EXEC       0x1
#define QCOM_SCM_PERM_RW (QCOM_SCM_PERM_READ | QCOM_SCM_PERM_WRITE)
#define QCOM_SCM_PERM_RWX (QCOM_SCM_PERM_RW | QCOM_SCM_PERM_EXEC)

bool qcom_scm_is_available(void);

int qcom_scm_qseecom_app_get_id(const char *app_name, u32 *app_id);
int qcom_scm_qseecom_app_send(u32 app_id, dma_addr_t req, size_t req_size,
			      dma_addr_t rsp, size_t rsp_size);

#endif
