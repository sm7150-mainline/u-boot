#ifndef __TOUCHSCREEN_H__
#define __TOUCHSCREEN_H__

#include <input.h>

/* The pointer stopped touching the screen (since last report) */
#define TOUCH_STATE_UP 0
/* The pointer has touched the screen (since last report) */
#define TOUCH_STATE_ACTIVE 1
/* The pointer touched and released since last report */
#define TOUCH_STATE_TAPPED 2

enum touch_report_prop {
	TOUCH_REPORT_X,
	TOUCH_REPORT_Y,
	TOUCH_REPORT_STATE,
};

/**
 * struct touchscreen_priv - information about a keyboard, for the uclass
 *
 * @is_primary:		This touchscreen is a primary input device
 *			(and is associated with the display).
 */
struct touchscreen_plat {
	bool is_primary;
	u32 max_x;
	u32 max_y;
};

/**
 * struct touchscreen_ops - keyboard device operations
 */
struct touchscreen_ops {
	int (*report_events)(struct udevice *dev);
};

void touch_report(enum touch_report_prop prop, u32 val);

#endif /* __TOUCHSCREEN_H__ */
