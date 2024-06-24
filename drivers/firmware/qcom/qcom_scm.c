// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2010,2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015,2024 Linaro Ltd.
 */

#define LOG_DEBUG

#include <cpu_func.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/of_access.h>
#include <dm/uclass.h>
#include <linux/bug.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/arm-smccc.h>
#include <linux/sizes.h>
#include <firmware/qcom_scm.h>
#include <part.h>

/* HACKS */
#include <env.h>

#include "qcom_scm.h"


struct qcom_scm {
	struct udevice *dev;
};

struct qcom_scm_current_perm_info {
	__le32 vmid;
	__le32 perm;
	__le64 ctx;
	__le32 ctx_size;
	__le32 unused;
};

struct qcom_scm_mem_map_info {
	__le64 mem_addr;
	__le64 mem_size;
};

/**
 * struct qcom_scm_qseecom_resp - QSEECOM SCM call response.
 * @result:    Result or status of the SCM call. See &enum qcom_scm_qseecom_result.
 * @resp_type: Type of the response. See &enum qcom_scm_qseecom_resp_type.
 * @data:      Response data. The type of this data is given in @resp_type.
 */
struct qcom_scm_qseecom_resp {
	u64 result;
	u64 resp_type;
	u64 data;
};

struct qcom_scm_qseecom_app_start {
	u64 mdt_len;
	u64 img_len;
	u64 pa;

	char app_name[32];
};

enum qcom_scm_qseecom_result {
	QSEECOM_RESULT_SUCCESS			= 0,
	QSEECOM_RESULT_INCOMPLETE		= 1,
	QSEECOM_RESULT_BLOCKED_ON_LISTENER	= 2,
	QSEECOM_RESULT_FAILURE			= 0xFFFFFFFF,
};

enum qcom_scm_qseecom_resp_type {
	QSEECOM_SCM_RES_APP_ID			= 0xEE01,
	QSEECOM_SCM_RES_QSEOS_LISTENER_ID	= 0xEE02,
};

enum qcom_scm_qseecom_tz_owner {
	QSEECOM_TZ_OWNER_SIP			= 2,
	QSEECOM_TZ_OWNER_TZ_APPS		= 48,
	QSEECOM_TZ_OWNER_QSEE_OS		= 50
};

enum qcom_scm_qseecom_tz_svc {
	QSEECOM_TZ_SVC_APP_ID_PLACEHOLDER	= 0,
	QSEECOM_TZ_SVC_APP_MGR			= 1,
	QSEECOM_TZ_SVC_EXTERNAL			= 3,
	QSEECOM_TZ_SVC_INFO			= 6,
};

enum qcom_scm_qseecom_tz_cmd_app {
	QSEECOM_TZ_CMD_APP_SEND			= 1,
	QSEECOM_TZ_CMD_APP_LOOKUP		= 3,
	QSEECOM_TZ_CMD_REGION_NOTIFY		= 5,
	QSEECOM_TZ_CMD_LOAD_SERVICES_IMAGE	= 7,
	QSEECOM_TZ_CMD_QUERY_CMNLIBS		= 10,
};

enum qcom_scm_qseecom_tz_cmd_info {
	QSEECOM_TZ_CMD_INFO_VERSION		= 3,
};

#define QSEECOM_MAX_APP_NAME_SIZE		64

static const char * const qcom_scm_convention_names[] = {
	[SMC_CONVENTION_UNKNOWN] = "unknown",
	[SMC_CONVENTION_ARM_32] = "smc arm 32",
	[SMC_CONVENTION_ARM_64] = "smc arm 64",
	[SMC_CONVENTION_LEGACY] = "smc legacy",
};

enum qcom_scm_convention qcom_scm_convention = SMC_CONVENTION_UNKNOWN;
static struct qcom_scm *__scm;


static enum qcom_scm_convention __get_convention(void)
{
	unsigned long flags;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.args[0] = SCM_SMC_FNID(QCOM_SCM_SVC_INFO,
					   QCOM_SCM_INFO_IS_CALL_AVAIL) |
			   (ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT),
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;
	enum qcom_scm_convention probed_convention;
	int ret;
	bool forced = false;

	if (likely(qcom_scm_convention != SMC_CONVENTION_UNKNOWN))
		return qcom_scm_convention;

	/*
	 * Per the "SMC calling convention specification", the 64-bit calling
	 * convention can only be used when the client is 64-bit, otherwise
	 * system will encounter the undefined behaviour.
	 */
#if IS_ENABLED(CONFIG_ARM64)
	/*
	 * Device isn't required as there is only one argument - no device
	 * needed to dma_map_single to secure world
	 */
	probed_convention = SMC_CONVENTION_ARM_64;
	ret = __scm_smc_call(NULL, &desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;

	/*
	 * Some SC7180 firmwares didn't implement the
	 * QCOM_SCM_INFO_IS_CALL_AVAIL call, so we fallback to forcing ARM_64
	 * calling conventions on these firmwares. Luckily we don't make any
	 * early calls into the firmware on these SoCs so the device pointer
	 * will be valid here to check if the compatible matches.
	 */
	if (__scm) {
		if (ofnode_device_is_compatible(dev_ofnode(__scm->dev), "qcom,scm-sc7180")) {
			forced = true;
			goto found;
		}
	}
#endif

	probed_convention = SMC_CONVENTION_ARM_32;
	ret = __scm_smc_call(NULL, &desc, probed_convention, &res, true);
	if (!ret && res.result[0] == 1)
		goto found;

	probed_convention = SMC_CONVENTION_LEGACY;
found:
	spin_lock_irqsave(&scm_query_lock, flags);
	if (probed_convention != qcom_scm_convention) {
		qcom_scm_convention = probed_convention;
		pr_info("qcom_scm: convention: %s%s\n",
			qcom_scm_convention_names[qcom_scm_convention],
			forced ? " (forced)" : "");
	}
	spin_unlock_irqrestore(&scm_query_lock, flags);

	return qcom_scm_convention;
}

/**
 * qcom_scm_call() - Invoke a syscall in the secure world
 * @dev:	device
 * @desc:	Descriptor structure containing arguments and return values
 * @res:        Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 */
static int qcom_scm_call(struct udevice *dev, const struct qcom_scm_desc *desc,
			 struct qcom_scm_res *res)
{
	//might_sleep();
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return scm_smc_call(dev, desc, res, false);
	case SMC_CONVENTION_LEGACY:
		pr_err("Legacy SCM calling convention is not supported.\n");
		return -EINVAL;
//		return scm_legacy_call(dev, desc, res);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}


static int __qcom_scm_assign_mem(struct udevice *dev, phys_addr_t mem_region,
				 size_t mem_sz, phys_addr_t src, size_t src_sz,
				 phys_addr_t dest, size_t dest_sz)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_ASSIGN,
		.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_RO, QCOM_SCM_VAL,
					 QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_RO,
					 QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args[0] = mem_region,
		.args[1] = mem_sz,
		.args[2] = src,
		.args[3] = src_sz,
		.args[4] = dest,
		.args[5] = dest_sz,
		.args[6] = 0,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(dev, &desc, &res);

	return ret ? : res.result[0];
}

/**
 * qcom_scm_assign_mem() - Make a secure call to reassign memory ownership
 * @mem_addr: mem region whose ownership need to be reassigned
 * @mem_sz:   size of the region.
 * @srcvm:    vmid for current set of owners, each set bit in
 *            flag indicate a unique owner
 * @newvm:    array having new owners and corresponding permission
 *            flags
 * @dest_cnt: number of owners in next set.
 *
 * Return negative errno on failure or 0 on success with @srcvm updated.
 */
int qcom_scm_assign_mem(phys_addr_t mem_addr, size_t mem_sz,
			u64 *srcvm,
			const struct qcom_scm_vmperm *newvm,
			unsigned int dest_cnt)
{
	struct qcom_scm_current_perm_info *destvm;
	struct qcom_scm_mem_map_info *mem_to_map;
	phys_addr_t mem_to_map_phys;
	phys_addr_t dest_phys;
	dma_addr_t ptr_phys;
	size_t mem_to_map_sz;
	size_t dest_sz;
	size_t src_sz;
	size_t ptr_sz;
	int next_vm;
	__le32 *src;
	void *ptr;
	int ret, i, b;
	u64 srcvm_bits = *srcvm;

	src_sz = generic_hweight64(srcvm_bits) * sizeof(*src);
	mem_to_map_sz = sizeof(*mem_to_map);
	dest_sz = dest_cnt * sizeof(*destvm);
	ptr_sz = ALIGN(src_sz, SZ_64) + ALIGN(mem_to_map_sz, SZ_64) +
			ALIGN(dest_sz, SZ_64);

	ptr = kzalloc(ptr_sz, GFP_KERNEL);//dma_alloc_coherent(__scm->dev, ptr_sz, &ptr_phys, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ptr_phys = (phys_addr_t)ptr;

	/* Fill source vmid detail */
	src = ptr;
	i = 0;
	for (b = 0; b < 64; b++) { // BITS_PER_TYPE(u64)
		if (srcvm_bits & BIT(b))
			src[i++] = cpu_to_le32(b);
	}

	/* Fill details of mem buff to map */
	mem_to_map = ptr + ALIGN(src_sz, SZ_64);
	mem_to_map_phys = ptr_phys + ALIGN(src_sz, SZ_64);
	mem_to_map->mem_addr = cpu_to_le64(mem_addr);
	mem_to_map->mem_size = cpu_to_le64(mem_sz);

	next_vm = 0;
	/* Fill details of next vmid detail */
	destvm = ptr + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	dest_phys = ptr_phys + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	for (i = 0; i < dest_cnt; i++, destvm++, newvm++) {
		destvm->vmid = cpu_to_le32(newvm->vmid);
		destvm->perm = cpu_to_le32(newvm->perm);
		destvm->ctx = 0;
		destvm->ctx_size = 0;
		next_vm |= BIT(newvm->vmid);
	}

	flush_dcache_range(ptr_phys, (unsigned long)ptr + ptr_sz);

	ret = __qcom_scm_assign_mem(__scm->dev, mem_to_map_phys, mem_to_map_sz,
				    ptr_phys, src_sz, dest_phys, dest_sz);
	//dma_free_coherent(__scm->dev, ptr_sz, ptr, ptr_phys);
	if (ret) {
		dev_err(__scm->dev,
			"Assign memory protection call failed %d\n", ret);
		return -EINVAL;
	}

	*srcvm = next_vm;
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_assign_mem);

/* Lock for QSEECOM SCM call executions */
//static DEFINE_MUTEX(qcom_scm_qseecom_call_lock);

static int __qcom_scm_qseecom_call(const struct qcom_scm_desc *desc,
				   struct qcom_scm_qseecom_resp *res)
{
	struct qcom_scm_res scm_res = {};
	int status;

	/*
	 * QSEECOM SCM calls should not be executed concurrently. Therefore, we
	 * require the respective call lock to be held.
	 */
	//lockdep_assert_held(&qcom_scm_qseecom_call_lock);

	status = qcom_scm_call(__scm->dev, desc, &scm_res);

	res->result = scm_res.result[0];
	res->resp_type = scm_res.result[1];
	res->data = scm_res.result[2];

	if (status)
		return status;

	return 0;
}

/**
 * qcom_scm_qseecom_call() - Perform a QSEECOM SCM call.
 * @desc: SCM call descriptor.
 * @res:  SCM call response (output).
 *
 * Performs the QSEECOM SCM call described by @desc, returning the response in
 * @rsp.
 *
 * Return: Zero on success, nonzero on failure.
 */
static int qcom_scm_qseecom_call(const struct qcom_scm_desc *desc,
				 struct qcom_scm_qseecom_resp *res)
{
	int status;

	/*
	 * Note: Multiple QSEECOM SCM calls should not be executed same time,
	 * so lock things here. This needs to be extended to callback/listener
	 * handling when support for that is implemented.
	 */

	dev_dbg(__scm->dev, "%s: owner=%x, svc=%x, cmd=%x\n",
		__func__, desc->owner, desc->svc, desc->cmd);

	mutex_lock(&qcom_scm_qseecom_call_lock);
	status = __qcom_scm_qseecom_call(desc, res);
	mutex_unlock(&qcom_scm_qseecom_call_lock);

	dev_dbg(__scm->dev, "%s: owner=%x, svc=%x, cmd=%x, result=%lld, type=%llx, data=%llx\n",
		__func__, desc->owner, desc->svc, desc->cmd, res->result,
		res->resp_type, res->data);

	if (status) {
		dev_err(__scm->dev, "qseecom: scm call failed with error %d\n", status);
		return status;
	}

	/*
	 * TODO: Handle incomplete and blocked calls:
	 *
	 * Incomplete and blocked calls are not supported yet. Some devices
	 * and/or commands require those, some don't. Let's warn about them
	 * prominently in case someone attempts to try these commands with a
	 * device/command combination that isn't supported yet.
	 */
	WARN_ON(res->result == QSEECOM_RESULT_INCOMPLETE);
	WARN_ON(res->result == QSEECOM_RESULT_BLOCKED_ON_LISTENER);

	return 0;
}

/**
 * qcom_scm_qseecom_get_version() - Query the QSEECOM version.
 * @version: Pointer where the QSEECOM version will be stored.
 *
 * Performs the QSEECOM SCM querying the QSEECOM version currently running in
 * the TrustZone.
 *
 * Return: Zero on success, nonzero on failure.
 */
static int qcom_scm_qseecom_get_version(u32 *version)
{
	struct qcom_scm_desc desc = {};
	struct qcom_scm_qseecom_resp res = {};
	u32 feature = 10;
	int ret;

	desc.owner = QSEECOM_TZ_OWNER_SIP;
	desc.svc = QSEECOM_TZ_SVC_INFO;
	desc.cmd = QSEECOM_TZ_CMD_INFO_VERSION;
	desc.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_VAL);
	desc.args[0] = feature;

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;

	*version = res.result;
	return 0;
}

/**
 * qcom_scm_qseecom_app_get_id() - Query the app ID for a given QSEE app name.
 * @app_name: The name of the app.
 * @app_id:   The returned app ID.
 *
 * Query and return the application ID of the SEE app identified by the given
 * name. This returned ID is the unique identifier of the app required for
 * subsequent communication.
 *
 * Return: Zero on success, nonzero on failure, -ENOENT if the app has not been
 * loaded or could not be found.
 */
int qcom_scm_qseecom_app_get_id(const char *app_name, u32 *app_id)
{
	unsigned long name_buf_size = QSEECOM_MAX_APP_NAME_SIZE;
	unsigned long app_name_len = strlen(app_name);
	struct qcom_scm_desc desc = {};
	struct qcom_scm_qseecom_resp res = {};
	dma_addr_t name_buf_phys;
	char *name_buf;
	int status;

	if (app_name_len >= name_buf_size)
		return -EINVAL;

	name_buf = kzalloc(name_buf_size, GFP_KERNEL);
	if (!name_buf)
		return -ENOMEM;

	memcpy(name_buf, app_name, app_name_len);

	name_buf_phys = (phys_addr_t)name_buf; //dma_map_single(__scm->dev, name_buf, name_buf_size, DMA_TO_DEVICE);
	// status = dma_mapping_error(__scm->dev, name_buf_phys);
	// if (status) {
	// 	kfree(name_buf);
	// 	dev_err(__scm->dev, "qseecom: failed to map dma address\n");
	// 	return status;
	// }

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_APP_LOOKUP;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = name_buf_phys;
	desc.args[1] = app_name_len;

	status = qcom_scm_qseecom_call(&desc, &res);
	//dma_unmap_single(__scm->dev, name_buf_phys, name_buf_size, DMA_TO_DEVICE);
	kfree(name_buf);

	if (status)
		return status;

	if (res.result == QSEECOM_RESULT_FAILURE)
		return -ENOENT;

	if (res.result != QSEECOM_RESULT_SUCCESS)
		return -EINVAL;

	if (res.resp_type != QSEECOM_SCM_RES_APP_ID)
		return -EINVAL;

	*app_id = res.data;
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_qseecom_app_get_id);

/**
 * qcom_scm_qseecom_app_send() - Send to and receive data from a given QSEE app.
 * @app_id:   The ID of the target app.
 * @req:      DMA address of the request buffer sent to the app.
 * @req_size: Size of the request buffer.
 * @rsp:      DMA address of the response buffer, written to by the app.
 * @rsp_size: Size of the response buffer.
 *
 * Sends a request to the QSEE app associated with the given ID and read back
 * its response. The caller must provide two DMA memory regions, one for the
 * request and one for the response, and fill out the @req region with the
 * respective (app-specific) request data. The QSEE app reads this and returns
 * its response in the @rsp region.
 *
 * Return: Zero on success, nonzero on failure.
 */
int qcom_scm_qseecom_app_send(u32 app_id, dma_addr_t req, size_t req_size,
			      dma_addr_t rsp, size_t rsp_size)
{
	struct qcom_scm_qseecom_resp res = {};
	struct qcom_scm_desc desc = {};
	int status;

	desc.owner = QSEECOM_TZ_OWNER_TZ_APPS;
	desc.svc = QSEECOM_TZ_SVC_APP_ID_PLACEHOLDER;
	desc.cmd = QSEECOM_TZ_CMD_APP_SEND;
	desc.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_VAL,
				     QCOM_SCM_RW, QCOM_SCM_VAL,
				     QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = app_id;
	desc.args[1] = req;
	desc.args[2] = req_size;
	desc.args[3] = rsp;
	desc.args[4] = rsp_size;

	status = qcom_scm_qseecom_call(&desc, &res);

	if (status)
		return status;

	if (res.result != QSEECOM_RESULT_SUCCESS)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_qseecom_app_send);

static int load_image_from_disk(const char *guid, void **buf, size_t *size)
{
	struct udevice *partdev;
	struct disk_partition *info;

	partdev = part_get_by_guid(guid, &info);
	if (IS_ERR_OR_NULL(partdev))
		return -ENODEV;

	/* FIXME: ... malloc() isn't good for these big allocations. we only need for a short
	 * time anyway and can free afterwards...
	 */
	*buf = (void *)env_get_hex("kernel_addr_r", 0);
	if (!*buf)
		return -EINVAL;

	memset(*buf, 0, SZ_8M);

	*size = ALIGN(info->size * info->blksz, SZ_64);

	blk_read(partdev, info->start, info->size, *buf);

	log_info("Loaded %s from disk to %p (%lu bytes)\n", info->name, *buf, *size);

	return 0;
}

#define UEFISECAPP_PART_TYPE "be8a7e08-1b7a-4cae-993a-d5b7fb55b3c2"
#define KEYMASTER_PART_TYPE "a11d2a7c-d82a-4c2f-8a01-1805240e6626"
#define CMNLIB_PART_TYPE "73471795-ab54-43f9-a847-4f72ea5cbef5"
#define CMNLIB64_PART_TYPE "8ea64893-1267-4a1b-947c-7c362acaad2c"

static int qcom_scm_qseecom_start_uefisecapp(void)
{
	struct qcom_scm_desc desc = {};
	struct qcom_scm_qseecom_resp res = {};
	struct qcom_scm_qseecom_app_start *params;
	struct qcom_scm_vmperm perms[2];
	u64 src_perms;
	u32 vmid[2];
	int num_vmids;
	int ret, i;

	/* Configure TZ apps region */
	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_REGION_NOTIFY;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);
	desc.args[0] = 0x61800000;
	desc.args[1] = 0x02100000;

	dev_info(__scm->dev, "qseecom: notifying TZ apps region\n");

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;

	dev_info(__scm->dev, "qseecom: notified TZ apps region\n");

	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_QUERY_CMNLIBS;
	desc.arginfo = QCOM_SCM_ARGS(0);

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (!ret) {
		dev_info(__scm->dev, "qseecom: cmnlibs already loaded\n");
		//goto cmnlibs_loaded;
	}

	/* First load the common libs */
	dev_info(__scm->dev, "qseecom: loading cmnlib\n");
	desc.owner = QSEECOM_TZ_OWNER_QSEE_OS;
	desc.svc = QSEECOM_TZ_SVC_APP_MGR;
	desc.cmd = QSEECOM_TZ_CMD_LOAD_SERVICES_IMAGE;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL);
	params = (void *)&desc.args[0];
	params->mdt_len = 0;

	ret = load_image_from_disk(CMNLIB_PART_TYPE, (void **)&params->pa, (size_t *)&params->img_len);
	if (ret) {
		dev_err(__scm->dev, "qseecom: failed to load CMNLIB_PART_TYPE,\n");
		return ret;
	}

	flush_dcache_range((unsigned long)params, (unsigned long)params + sizeof(*params));
	flush_dcache_range((unsigned long)params->pa, (unsigned long)params->pa + params->img_len);

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;

	dev_info(__scm->dev, "qseecom: loading cmnlib64\n");
	ret = load_image_from_disk(CMNLIB64_PART_TYPE, (void **)&params->pa, (size_t *)&params->img_len);
	if (ret) {
		dev_err(__scm->dev, "qseecom: failed to load CMNLIB64_PART_TYPE\n");
		return ret;
	}

	flush_dcache_range((unsigned long)params, (unsigned long)params + sizeof(*params));
	flush_dcache_range((unsigned long)params->pa, (unsigned long)params->pa + params->img_len);

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;


cmnlibs_loaded:
	desc.cmd = QSEECOM_TZ_CMD_APP_SEND;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL);
	params = (void *)&desc.args[0];
	params->mdt_len = 0;

	dev_info(__scm->dev, "qseecom: starting keymaster!\n");
	ret = load_image_from_disk(KEYMASTER_PART_TYPE, (void **)&params->pa, (size_t *)&params->img_len);
	if (ret) {
		dev_err(__scm->dev, "qseecom: failed to load KEYMASTER_PART_TYPE\n");
		return ret;
	}

	flush_dcache_range((unsigned long)params, (unsigned long)params + sizeof(*params));
	flush_dcache_range((unsigned long)params->pa, (unsigned long)params->pa + params->img_len);

	dev_info(__scm->dev, "qseecom: params: mdt_len %llx, img_len %llx, pa %llx\n",
		params->mdt_len, params->img_len, params->pa);

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;

	dev_info(__scm->dev, "qseecom: started keymaster, res %#llx, type %#llx, data %#llx\n",
		res.result, res.resp_type, res.data);

	dev_info(__scm->dev, "qseecom: starting uefisecapp!\n");
	ret = load_image_from_disk(UEFISECAPP_PART_TYPE, (void **)&params->pa, (size_t *)&params->img_len);
	if (ret) {
		dev_err(__scm->dev, "qseecom: failed to load UEFISECAPP_PART_TYPE\n");
		return ret;
	}

	flush_dcache_range((unsigned long)params, (unsigned long)params + sizeof(*params));
	flush_dcache_range((unsigned long)params->pa, (unsigned long)params->pa + params->img_len);

	dev_info(__scm->dev, "qseecom: params: mdt_len %llx, img_len %llx, pa %llx\n",
		params->mdt_len, params->img_len, params->pa);

	ret = qcom_scm_qseecom_call(&desc, &res);
	if (ret)
		return ret;

	dev_info(__scm->dev, "qseecom: started uefisecapp, res %#llx, type %#llx, data %#llx\n",
		res.result, res.resp_type, res.data);

	return 0;
}

static int qcom_scm_qseecom_init(struct qcom_scm *scm)
{
	struct udevice *blk_dev;
	u32 version;
	int ret;

	/*
	 * Note: We do two steps of validation here: First, we try to query the
	 * QSEECOM version as a check to see if the interface exists on this
	 * device. Second, we check against known good devices due to current
	 * driver limitations (see comment in qcom_scm_qseecom_allowlist).
	 *
	 * Note that we deliberately do the machine check after the version
	 * check so that we can log potentially supported devices. This should
	 * be safe as downstream sources indicate that the version query is
	 * neither blocking nor reentrant.
	 */
	ret = qcom_scm_qseecom_get_version(&version);
	if (ret)
		return 0;

	dev_info(scm->dev, "qseecom: found qseecom with version 0x%x\n", version);

	// if (!qcom_scm_qseecom_machine_is_allowed()) {
	// 	dev_info(scm->dev, "qseecom: untested machine, skipping\n");
	// 	return 0;
	// }

	/*
	 * Set up QSEECOM interface device. All application clients will be
	 * set up and managed by the corresponding driver for it.
	 */
	// qseecom_dev = platform_device_alloc("qcom_qseecom", -1);
	// if (!qseecom_dev)
	// 	return -ENOMEM;

	// qseecom_dev->dev.parent = scm->dev;

	// ret = platform_device_add(qseecom_dev);
	// if (ret) {
	// 	platform_device_put(qseecom_dev);
	// 	return ret;
	// }

	// uclass_foreach_dev_probe(UCLASS_PARTITION, blk_dev) {
	// 	struct disk_partition *plat = dev_get_plat(blk_dev);
	// }

	return 0;
}

int qcom_scm_probe(struct udevice *dev)
{
	__scm = dev_get_priv(dev);
	__scm->dev = dev;

	dev_info(dev, "SCM calling convention: %s\n",
		 qcom_scm_convention_names[__get_convention()]);

	qcom_scm_qseecom_init(__scm);

	qcom_scm_qseecom_start_uefisecapp();

	return 0;
}

static const struct udevice_id qcom_scm_of_match[] = {
	{ .compatible = "qcom,scm" },
	{}
};

U_BOOT_DRIVER(qcom_scm) = {
	.name	= "qcom_scm",
	.id	= UCLASS_FIRMWARE,
	.flags = DM_FLAG_PROBE_AFTER_BIND,
	.of_match = qcom_scm_of_match,
	.probe	= qcom_scm_probe,
	.priv_auto = sizeof(struct qcom_scm),
};
