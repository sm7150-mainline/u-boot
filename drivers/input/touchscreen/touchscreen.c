

#include <errno.h>
#include <log.h>
#include <dm/uclass.h>
#include <touchscreen.h>

struct touchscreen_priv {
	u32 x;
	u32 y;
	u8 state;
};

static int touch_post_probe(struct udevice *dev)
{
	return 0;
}

UCLASS_DRIVER(touch) = {
	.id = UCLASS_TOUCH,
	.name = "touch",
	.post_probe = touch_post_probe,
	.per_device_plat_auto = sizeof(struct touchscreen_plat),
	.per_device_auto = sizeof(struct touchscreen_priv),
};
