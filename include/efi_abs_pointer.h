// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI_ABSOLUTE_POINTER_PROTOCOL_GUID
 *
 * Copyright (c) 2024 Caleb Connolly
 */

#include <linux/types.h>
#include <efi_api.h>

struct efi_abs_pointer_mode {
  u64    abs_min_x; /* The Absolute Minimum of the device on the x-axis */
  u64    abs_min_y; /* The Absolute Minimum of the device on the y axis. */
  u64    abs_min_z; /* The Absolute Minimum of the device on the z-axis */
  u64    abs_max_x; /* The Absolute Maximum of the device on the x-axis. If 0, and the
                        * abs_min_x is 0, then the pointer device does not support a xaxis */
  u64    abs_max_y; /* The Absolute Maximum of the device on the y -axis. If 0, and the
                        * abs_min_x is 0, then the pointer device does not support a yaxis.
			*/
  u64    abs_max_z; /* The Absolute Maximum of the device on the z-axis. If 0 , and the
                        * abs_min_x is 0, then the pointer device does not support a zaxis */
  u32    attributes;   /* The following bits are set as needed (or'd together) to indicate the
                          * capabilities of the device supported. The remaining bits are undefined
                          * and should be 0 */
};

struct efi_abs_pointer_state {
  /*
  * The unsigned position of the activation on the x axis. If the AboluteMinX
  * and the AboluteMaxX fields of the EFI_ABSOLUTE_POINTER_MODE structure are
  * both 0, then this pointer device does not support an x-axis, and this field
  * must be ignored.
  */
  u64    current_x;

  /*
  * The unsigned position of the activation on the y axis. If the AboluteMinY
  * and the AboluteMaxY fields of the EFI_ABSOLUTE_POINTER_MODE structure are
  * both 0, then this pointer device does not support an y-axis, and this field
  * must be ignored.
  */
  u64    current_y;

  /*
  * The unsigned position of the activation on the z axis, or the pressure
  * measurement. If the AboluteMinZ and the AboluteMaxZ fields of the
  * EFI_ABSOLUTE_POINTER_MODE structure are both 0, then this pointer device
  * does not support an z-axis, and this field must be ignored.
  */
  u64    current_z;

  /*
  * Bits are set to 1 in this structure item to indicate that device buttons are
  * active.
  */
  u32    active_buttons;
};

struct efi_abs_pointer_protocol {
	efi_status_t (EFIAPI *reset) (struct efi_abs_pointer_protocol *this,
				      bool extended_verification);
	efi_status_t (EFIAPI *get_state) (struct efi_abs_pointer_protocol *this,
					  struct efi_abs_pointer_state *state);
	struct efi_event *wait_for_input;
	struct efi_abs_pointer_mode *mode;
};

extern struct efi_abs_pointer_protocol efi_abs_pointer_prot;
extern const efi_guid_t efi_guid_abs_pointer_protocol;

