// SPDX-License-Identifier: GPL-2.0

#ifndef __QCOM_PRIV_H__
#define __QCOM_PRIV_H__

void qcom_set_serialno(void);

#if IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT)
void qcom_configure_capsule_updates(void);
#else
void qcom_configure_capsule_updates(void) {}
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

#if CONFIG_IS_ENABLED(OF_LIVE)
/**
 * qcom_of_fixup_nodes() - Fixup Qualcomm DT nodes
 *
 * Adjusts nodes in the live tree to improve compatibility with U-Boot.
 */
void qcom_of_fixup_nodes(void);
#else
static inline void qcom_of_fixup_nodes(void)
{
	log_debug("Unable to dynamically fixup USB nodes, please enable CONFIG_OF_LIVE\n");
}
#endif /* OF_LIVE */

#if CONFIG_IS_ENABLED(MULTI_DTB_FIT)

/**
 * qcom_of_match() - determine the FIT config name for the
 * current board based on the FDT provided by ABL or LK.
 * This assumes that the FDT is a dummy which just contains
 * the match data to use following the lk2nd spec.
 */
const char *qcom_of_match(void *external_fdt);
#else
static inline const char *qcom_of_match(void *external_fdt)
{
	return NULL;
}
#endif /* MULTI_DTB_FIT */

struct pte_smem_detect_state {
	phys_addr_t ram_start;
	phys_addr_t start;
	size_t size;
};

int qcom_smem_detect(struct pte_smem_detect_state *state);

#endif /* __QCOM_PRIV_H__ */
