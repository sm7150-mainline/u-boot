// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI_ABSOLUTE_POINTER_PROTOCOL_GUID
 *
 * Copyright (c) 2024 Caleb Connolly
 */

#include <efi_abs_pointer.h>

const efi_guid_t efi_guid_abs_pointer_protocol = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;

static struct efi_abs_pointer_mode efi_aptr_mode = {
	.abs_max_x = 1080,
	.abs_max_y = 2400,
};

/* Register absolute pointer protocol */
efi_status_t efi_abs_pointer_register(void)
{

	return EFI_SUCCESS;
}

static efi_status_t EFIAPI
efi_abs_pointer_get_state(struct efi_abs_pointer_protocol *this,
			  struct efi_abs_pointer_state *state)
{

	return EFI_SUCCESS;
}

struct efi_abs_pointer_protocol efi_abs_pointer_prot = {
	.mode = &efi_aptr_mode,
	.
};
