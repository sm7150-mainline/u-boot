// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Driver for Synaptics TCM Oncell Touchscreens
 *
 *  Copyright (c) 2024 Frieder Hannenheim <frieder.hannenheim@proton.me>
 *  Copyright (c) 2024 Caleb Connolly <caleb.connolly@linaro.org>
 */

#define LOG_DEBUG

#include <console.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <power/regulator.h>
#include <env.h>
#include <errno.h>
#include <asm/gpio.h>
#include <log.h>
#include <stdio_dev.h>
#include <input.h>
#include <linux/input.h>
#include <i2c.h>
#include <time.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <hexdump.h>
#include <keyboard.h>
#include <video_console.h>

#include <dm/device_compat.h>


/* Commands */
#define TCM_NONE 0x00
#define TCM_CONTINUE_WRITE 0x01
#define TCM_IDENTIFY 0x02
#define TCM_RESET 0x04
#define TCM_ENABLE_REPORT 0x05
#define TCM_DISABLE_REPORT 0x06
#define TCM_GET_BOOT_INFO 0x10
#define TCM_ERASE_FLASH 0x11
#define TCM_WRITE_FLASH 0x12
#define TCM_READ_FLASH 0x13
#define TCM_RUN_APPLICATION_FIRMWARE 0x14
#define TCM_SPI_MASTER_WRITE_THEN_READ 0x15
#define TCM_REBOOT_TO_ROM_BOOTLOADER 0x16
#define TCM_RUN_BOOTLOADER_FIRMWARE 0x1f
#define TCM_GET_APPLICATION_INFO 0x20
#define TCM_GET_STATIC_CONFIG 0x21
#define TCM_SET_STATIC_CONFIG 0x22
#define TCM_GET_DYNAMIC_CONFIG 0x23
#define TCM_SET_DYNAMIC_CONFIG 0x24
#define TCM_GET_TOUCH_REPORT_CONFIG 0x25
#define TCM_SET_TOUCH_REPORT_CONFIG 0x26
#define TCM_REZERO 0x27
#define TCM_COMMIT_CONFIG 0x28
#define TCM_DESCRIBE_DYNAMIC_CONFIG 0x29
#define TCM_PRODUCTION_TEST 0x2a
#define TCM_SET_CONFIG_ID 0x2b
#define TCM_ENTER_DEEP_SLEEP 0x2c
#define TCM_EXIT_DEEP_SLEEP 0x2d
#define TCM_GET_TOUCH_INFO 0x2e
#define TCM_GET_DATA_LOCATION 0x2f
#define TCM_DOWNLOAD_CONFIG 0xc0
#define TCM_GET_NSM_INFO 0xc3
#define TCM_EXIT_ESD 0xc4

#define MODE_APPLICATION 0x01
#define MODE_HOST_DOWNLOAD 0x02
#define MODE_BOOTLOADER 0x0b
#define MODE_TDDI_BOOTLOADER 0x0c

#define APP_STATUS_OK 0x00
#define APP_STATUS_BOOTING 0x01
#define APP_STATUS_UPDATING 0x02
#define APP_STATUS_BAD_APP_CONFIG 0xff

/* status codes */
#define REPORT_IDLE 0x00
#define REPORT_OK 0x01
#define REPORT_BUSY 0x02
#define REPORT_CONTINUED_READ 0x03
#define REPORT_RECEIVE_BUFFER_OVERFLOW 0x0c
#define REPORT_PREVIOUS_COMMAND_PENDING 0x0d
#define REPORT_NOT_IMPLEMENTED 0x0e
#define REPORT_ERROR 0x0f

/* report types */
#define REPORT_IDENTIFY 0x10
#define REPORT_TOUCH 0x11
#define REPORT_DELTA 0x12
#define REPORT_RAW 0x13
#define REPORT_DEBUG 0x14
#define REPORT_LOG 0x1d
#define REPORT_TOUCH_HOLD 0x20
#define REPORT_INVALID 0xff

/* Touch report codes */
#define TOUCH_END 0
#define TOUCH_FOREACH_ACTIVE_OBJECT 1
#define TOUCH_FOREACH_OBJECT 2
#define TOUCH_FOREACH_END 3
#define TOUCH_PAD_TO_NEXT_BYTE 4
#define TOUCH_TIMESTAMP 5
#define TOUCH_OBJECT_N_INDEX 6
#define TOUCH_OBJECT_N_CLASSIFICATION 7
#define TOUCH_OBJECT_N_X_POSITION 8
#define TOUCH_OBJECT_N_Y_POSITION 9
#define TOUCH_OBJECT_N_Z 10
#define TOUCH_OBJECT_N_X_WIDTH 11
#define TOUCH_OBJECT_N_Y_WIDTH 12
#define TOUCH_OBJECT_N_TX_POSITION_TIXELS 13
#define TOUCH_OBJECT_N_RX_POSITION_TIXELS 14
#define TOUCH_0D_BUTTONS_STATE 15
#define TOUCH_GESTURE_DOUBLE_TAP 16
#define TOUCH_FRAME_RATE 17 /* Normally 80hz */
#define TOUCH_POWER_IM 18
#define TOUCH_CID_IM 19
#define TOUCH_RAIL_IM 20
#define TOUCH_CID_VARIANCE_IM 21
#define TOUCH_NSM_FREQUENCY 22
#define TOUCH_NSM_STATE 23
#define TOUCH_NUM_OF_ACTIVE_OBJECTS 23
#define TOUCH_NUM_OF_CPU_CYCLES_USED_SINCE_LAST_FRAME 24
#define TOUCH_TUNING_GAUSSIAN_WIDTHS 0x80
#define TOUCH_TUNING_SMALL_OBJECT_PARAMS 0x81
#define TOUCH_TUNING_0D_BUTTONS_VARIANCE 0x82
#define TOUCH_REPORT_GESTURE_SWIPE 193
#define TOUCH_REPORT_GESTURE_CIRCLE 194
#define TOUCH_REPORT_GESTURE_UNICODE 195
#define TOUCH_REPORT_GESTURE_VEE 196
#define TOUCH_REPORT_GESTURE_TRIANGLE 197
#define TOUCH_REPORT_GESTURE_INFO 198
#define TOUCH_REPORT_GESTURE_COORDINATE 199
#define TOUCH_REPORT_CUSTOMER_GRIP_INFO 203

struct tcm_message_header {
	u8 marker;
	u8 code;
} __packed;

/* header + 2 bytes (which are length of data depending on report code) */
#define REPORT_PEAK_LEN (sizeof(struct tcm_message_header) + 2)

struct tcm_cmd {
	u8 cmd;
	u16 length;
	u8 data[];
};

struct tcm_identification {
	struct tcm_message_header header;
	u16 length;
	u8 version;
	u8 mode;
	char part_number[16];
	u8 build_id[4];
	u8 max_write_size[2];
} __packed;

struct tcm_app_info {
	struct tcm_message_header header;
	u16 length;
	u8 version[2];
	u16 status;
	u8 static_config_size[2];
	u8 dynamic_config_size[2];
	u8 app_config_start_write_block[2];
	u8 app_config_size[2];
	u8 max_touch_report_config_size[2];
	u8 max_touch_report_payload_size[2];
	char customer_config_id[16];
	u16 max_x;
	u16 max_y;
	u8 max_objects[2];
	u8 num_of_buttons[2];
	u8 num_of_image_rows[2];
	u8 num_of_image_cols[2];
	u8 has_hybrid_data[2];
} __packed;

struct tcm_report_config_prop {
	u8 id; /* TOUCH_OBJECT_* */
	u8 bits; /* Size of the field in bits */
};

struct tcm_report_config_entry {
	u8 foreach; /* TOUCH_FOREACH_* (and maybe other things?) */
	int n_props;
	const struct tcm_report_config_prop *props;
};

struct tcm_report_config {
	int n_entries;
	const struct tcm_report_config_entry *entries;
};

struct tcm_data {
	struct udevice *dev;
	struct regmap *regmap;
	struct input_dev *input;
	struct gpio_desc *reset_gpio;
	struct udevice *supplies[2];
	struct udevice *vid;

	/* annoying state */
	u16 buf_size;
	char buf[256];
};

static int tcm_send_cmd(struct tcm_data *tcm, struct tcm_cmd *cmd)
{
	struct dm_i2c_chip *chip = dev_get_parent_plat(tcm->dev);
	struct i2c_msg msg;
	int ret;

	dev_dbg(tcm->dev, "sending command %#x (%d bytes)\n", cmd->cmd, 1 + cmd->length);

	msg.addr = chip->chip_addr;
	msg.flags = 0;
	msg.len = 1 + cmd->length;
	msg.buf = (u8 *)cmd;

	ret = dm_i2c_xfer(tcm->dev, &msg, 1);
	if (!ret)
		return 0;
	else
		return ret;
}

static int tcm_send_cmd_noargs(struct tcm_data *tcm, u8 cmd)
{
	struct tcm_cmd c = {
		.cmd = cmd,
		.length = 0,
	};

	return tcm_send_cmd(tcm, &c);
}

static int tcm_read_buf(struct tcm_data *tcm,
			void *buf, size_t length)
{
	struct dm_i2c_chip *chip = dev_get_parent_plat(tcm->dev);
	struct i2c_msg msg;
	int ret;

	msg.addr = chip->chip_addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = buf;

	ret = dm_i2c_xfer(tcm->dev, &msg, 1);
	if (!ret)
		return 0;
	else
		return ret;
}

/* Poll for a particular report code */
static int tcm_poll_ready(struct tcm_data *tcm, int code)
{
	struct tcm_message_header header;
	int ret;
	ulong start = get_timer(0);

	while (get_timer(start) < 500) {
		ret = tcm_read_buf(tcm, &header, sizeof(header));
		if (ret && ret != -ETIMEDOUT)
			return ret;

		dev_dbg(tcm->dev, "%s: %#x\n", __func__, header.code);

		if (header.code == code)
			return 0;

		/* Errors */
		if (header.code == REPORT_RECEIVE_BUFFER_OVERFLOW && header.code <= REPORT_ERROR)
			return header.code;

		/* Discard any LOG or DEBUG reports */
		if (header.code == REPORT_LOG || header.code == REPORT_DEBUG)
			tcm_read_buf(tcm, tcm->buf, sizeof(tcm->buf));

		udelay(100);
	}
	
	return -ETIMEDOUT;
}

static int tcm_recv_report(struct tcm_data *tcm,
			   void *buf, size_t length)
{
	int ret;

	// ret = tcm_poll_ready(tcm);
	// if (ret) {
	// 	dev_err(tcm->dev, "failed to poll ready: %d\n", ret);
	// 	return -ETIMEDOUT;
	// }

	return tcm_read_buf(tcm, buf, length);
}

static int tcm_read_message(struct tcm_data *tcm, u8 cmd, void *buf, size_t length)
{
	struct tcm_message_header *header;
	int ret;
	u16 len;

	memset(tcm->buf, 0, sizeof(tcm->buf));
	header = (struct tcm_message_header *)tcm->buf;

	ret = tcm_send_cmd_noargs(tcm, cmd);
	if (ret)
		return ret;

	/* The firmware will pad if we try to read too many bytes sooo let's just be lazy */
	do {
		ret = tcm_recv_report(tcm, tcm->buf, sizeof(tcm->buf));
		if (ret) {
			dev_err(tcm->dev, "failed to read response: %d\n", ret);
			return ret;
		}
		if (header->code != REPORT_IDLE)
			break;
		udelay(5000);
	} while (1);

	len = get_unaligned_le16(tcm->buf + sizeof(*header));

	//dev_dbg(tcm->dev, "report %#x len %u\n", header->code, len);
	print_hex_dump_bytes("report: ", DUMP_PREFIX_OFFSET, tcm->buf, min(sizeof(tcm->buf), len + sizeof(*header)));

	tcm->buf_size = len + sizeof(*header);

	if (buf) {
		if (length > tcm->buf_size) {
			// dev_warn(tcm->dev, "expected %zu bytes, got %u\n",
			// 	 length, tcm->buf_size);
		}
		length = min((ulong)tcm->buf_size, length);
		memcpy(buf, tcm->buf, length);
	}

	/* Wait for idle*/
	ret = tcm_poll_ready(tcm, 0);
	if (ret) {
		dev_err(tcm->dev, "failed to poll ready: %d\n", ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

// static int tcm_input_open(struct input_dev *dev)
// {
// 	struct tcm_data *tcm = input_get_drvdata(dev);

// 	return i2c_smbus_write_byte(tcm->client, TCM_ENABLE_REPORT);
// }

// static void tcm_input_close(struct input_dev *dev)
// {
// 	struct tcm_data *tcm = input_get_drvdata(dev);
// 	int ret;

// 	ret = i2c_smbus_write_byte(tcm->client, TCM_DISABLE_REPORT);
// 	if (ret)
// 		dev_err(tcm->dev, "failed to turn off sensing\n");
// }

/*
The default report config looks like this:

a5 01 80 00 11 08 1e 08 0f 01 04 01 06 04 07 04
08 0c 09 0c 0a 08 0b 08 0c 08 0d 10 0e 10 03 00
00 00

a5 01 80 00 - HEADER + length

11 08 - TOUCH_FRAME_RATE (8 bits)
30 08 - UNKNOWN (8 bits)
0f 01 - TOUCH_0D_BUTTONS_STATE (1 bit)
04 01 - TOUCH_PAD_TO_NEXT_BYTE (7 bits - padding)
06 04 - TOUCH_OBJECT_N_INDEX (4 bits)
07 04 - TOUCH_OBJECT_N_CLASSIFICATION (4 bits)
08 0c - TOUCH_OBJECT_N_X_POSITION (12 bits)
09 0c - TOUCH_OBJECT_N_Y_POSITION (12 bits)
0a 08 - TOUCH_OBJECT_N_Z (8 bits)
0b 08 - TOUCH_OBJECT_N_X_WIDTH (8 bits)
0c 08 - TOUCH_OBJECT_N_Y_WIDTH (8 bits)
0d 10 - TOUCH_OBJECT_N_TX_POSITION_TIXELS (16 bits) ??
0e 10 - TOUCH_OBJECT_N_RX_POSITION_TIXELS (16 bits) ??
03 00 - TOUCH_FOREACH_END (0 bits)
00 00 - TOUCH_END (0 bits)

Parsing this dynamically gets complicated, and we kinda don't need to.

*/

struct tcm_default_report_data {
	u8 fps;
	struct {
		u8 unknown;
		u8 buttons;
		u8 idx : 4;
		u8 classification : 4;
		u16 x : 12;
		u16 y : 12;
		u8 z;
		u8 width_x;
		u8 width_y;
		u8 tx;
		u8 rx;
	} __packed points[];
} __packed;

#define WIDTH 1080UL
#define HEIGHT 2400UL
#define CHARWIDTH 67UL
#define CHARHEIGHT 74UL

static int tcm_handle_touch_report(struct tcm_data *tcm, char *buf, size_t len)
{
	struct tcm_default_report_data *data;
	buf += REPORT_PEAK_LEN;
	len -= REPORT_PEAK_LEN;

	dev_dbg(tcm->dev, "touch report len %zu\n", len);
	if ((len - 1) % 11)
		dev_err(tcm->dev, "invalid touch report length\n");

	data = (struct tcm_default_report_data *)buf;

	/* We don't need to report releases because we have INPUT_MT_DROP_UNUSED */
	for (int i = 0; i < (len - 1) / 11; i++) {
		u8 major_width, minor_width;

		minor_width = data->points[i].width_x;
		major_width = data->points[i].width_y;

		if (minor_width > major_width)
			swap(major_width, minor_width);
		
		ulong row, col;
		/* Set the row and column */
		row = (data->points[i].y * CHARHEIGHT * 1000) / HEIGHT / 1000;
		col = (data->points[i].x * CHARWIDTH * 1000) / WIDTH / 1000;
		dev_dbg(tcm->dev, "touch report: idx %u x %u y %u (char %lux%lu)\n",
			data->points[i].idx, data->points[i].x, data->points[i].y, col, row);

		vidconsole_clear_and_reset(tcm->vid);
		vidconsole_position_cursor(tcm->vid, col, row);
		/* print a 2x2 square of bright green on white '#' characters */
		vidconsole_put_string(tcm->vid, "\033[48;5;15m\033[38;5;2m####\033[0m\n\033[48;5;15m\033[38;5;2m####\033[0m\n");

		break;
		// input_mt_slot(tcm->input, data->points[i].idx);
		// input_mt_report_slot_state(tcm->input, MT_TOOL_FINGER, true);

		// input_report_abs(tcm->input, ABS_MT_POSITION_X, data->points[i].x);
		// input_report_abs(tcm->input, ABS_MT_POSITION_Y, data->points[i].y);
		// input_report_abs(tcm->input, ABS_MT_TOUCH_MAJOR, major_width);
		// input_report_abs(tcm->input, ABS_MT_TOUCH_MINOR, minor_width);
		// input_report_abs(tcm->input, ABS_MT_PRESSURE, data->points[i].z);
	}

	// input_mt_sync_frame(tcm->input);
	// input_sync(tcm->input);

	return 0;
}

static irqreturn_t tcm_report_irq(int irq, void *data)
{
	struct tcm_data *tcm = data;
	struct tcm_message_header *header;
	char buf[256];
	u16 len;
	int ret;

	header = (struct tcm_message_header *)buf;
	ret = tcm_recv_report(tcm, buf, sizeof(buf));
	if (ret) {
		dev_err(tcm->dev, "failed to read report: %d\n", ret);
		return IRQ_HANDLED;
	}

	switch (header->code) {
	case REPORT_OK:
	//case REPORT_CONTINUED_READ:
	case REPORT_IDENTIFY:
	case REPORT_TOUCH:
	case REPORT_DELTA:
	case REPORT_RAW:
	case REPORT_DEBUG:
	case REPORT_TOUCH_HOLD:
		break;
	default:
		//dev_dbg(tcm->dev, "Ignoring report %#x\n", header->code);
		return IRQ_HANDLED;
	}

	/* Not present for REPORT_CONTINUED_READ */
	len = get_unaligned_le16(buf + sizeof(*header));

	dev_dbg(tcm->dev, "report %#x len %u\n", header->code, len);
	print_hex_dump_bytes("report: ", DUMP_PREFIX_OFFSET, buf, min(sizeof(buf), len + sizeof(*header)));

	if (len > sizeof(buf) - sizeof(*header)) {
		dev_err(tcm->dev, "report too long\n");
		return IRQ_HANDLED;
	}

	/* Check if this is a read response or an indication. For indications
	 * (user touched the screen) we just parse the report directly.
	 */
	if (header->code == REPORT_TOUCH) {
		tcm_handle_touch_report(tcm, buf, len + sizeof(*header));
		return IRQ_HANDLED;
	}

	tcm->buf_size = len + sizeof(*header);
	memcpy(tcm->buf, buf, len + sizeof(*header));

	return IRQ_HANDLED;
}

static int tcm_tstc(struct udevice *dev)
{
	irqreturn_t ret;

	ret = tcm_report_irq(0, dev_get_priv(dev));
	// if (ret == IRQ_HANDLED)
	// 	return 1;

	return 0;
}

static int tcm_hw_init(struct tcm_data *tcm, u16 *max_x, u16 *max_y)
{
	int ret;
	struct tcm_identification id = { 0 };
	struct tcm_app_info app_info = { 0 };

	/* The firmware sends an IDENTIFY report immediately which we treat like a response */
	ret = tcm_read_message(tcm, TCM_RUN_APPLICATION_FIRMWARE, &id, sizeof(id));
	if (ret) {
		dev_err(tcm->dev, "failed to identify device: %d\n", ret);
		return ret;
	}

	/* Disable interrupts, we don't support them anyway... */
	// ret = tcm_send_cmd_noargs(tcm, TCM_DISABLE_REPORT);
	// if (ret)
	// 	return ret;

	dev_dbg(tcm->dev, "Synaptics TCM %s v%d mode %d\n",
		id.part_number, id.version, id.mode);
	if (id.mode != MODE_APPLICATION) {
		/* We don't support firmware updates or anything else */
		dev_err(tcm->dev, "Device is not in application mode\n");
		//return -ENODEV;
	}

	do {
		udelay(20 * 1000);
		ret = tcm_read_message(tcm, TCM_GET_APPLICATION_INFO, &app_info, sizeof(app_info));
		if (ret) {
			dev_err(tcm->dev, "failed to get application info: %d\n", ret);
			return ret;
		}
	} while (app_info.status == APP_STATUS_BOOTING || app_info.status == APP_STATUS_UPDATING);

	dev_dbg(tcm->dev, "Application firmware v%d.%d (customer '%s') status %d\n",
		 app_info.version[0], app_info.version[1], app_info.customer_config_id, app_info.status);

	*max_x = app_info.max_x;
	*max_y = app_info.max_y;

	return 0;
}

static const char *tcm_supply_names[] = { "vdd-supply", "vcc-supply" };

static int tcm_power_on(struct tcm_data *tcm)
{
	int ret;

	for (int i = 0; i < ARRAY_SIZE(tcm_supply_names); i++) {
		ret = regulator_set_enable(tcm->supplies[i], true);
		if (ret) {
			dev_err(tcm->dev, "failed to enable supply %s: %d\n", tcm_supply_names[i], ret);
			return ret;
		
		}
	}

	dm_gpio_set_value(tcm->reset_gpio, false);
	udelay(10 * 1000);
	dm_gpio_set_value(tcm->reset_gpio, true);
	udelay(80 * 1000);

	return 0;
}

static const struct tcm_report_config_entry __maybe_unused report_config_default_entry = {
	.foreach = TOUCH_FOREACH_ACTIVE_OBJECT,
	.n_props = 4,
	.props = (struct tcm_report_config_prop[]){
		{ .id = TOUCH_OBJECT_N_INDEX, .bits = 4 },
		{ .id = TOUCH_OBJECT_N_CLASSIFICATION, .bits = 4},
		{ .id = TOUCH_OBJECT_N_X_POSITION, .bits = 16 },
		{ .id = TOUCH_OBJECT_N_Y_POSITION, .bits = 16 },
		// { .id = TOUCH_OBJECT_N_X_WIDTH, .bits = 12 },
		// { .id = TOUCH_OBJECT_N_Y_WIDTH, .bits = 12 },
	},
};

static int tcm_probe(struct udevice *dev)
{
	struct tcm_data *tcm = dev_get_priv(dev);
	struct tcm_report_config __maybe_unused report_config;
	u16 max_x, max_y;
	int ret;
	ulong start;

	if (device_get_uclass_id(dev->parent) != UCLASS_I2C) {
		dev_err(dev, "parent is not an I2C device!\n");
		return -EPROTONOSUPPORT;
	}

	tcm->dev = dev;

	for (int i = 0; i < ARRAY_SIZE(tcm_supply_names); i++) {
		ret = device_get_supply_regulator(dev, tcm_supply_names[i], &tcm->supplies[i]);
		if (ret) {
			dev_err(dev, "failed to get supply %s: %d\n", tcm_supply_names[i], ret);
			return ret;
		}
	}

	tcm->reset_gpio = devm_gpiod_get_index(tcm->dev, "reset", 0, GPIOD_IS_OUT_ACTIVE);

	ret = tcm_power_on(tcm);
	if (ret) {
		dev_err(tcm->dev, "failed to power on: %d\n", ret);
		return ret;
	}

	ret = tcm_hw_init(tcm, &max_x, &max_y);
	if (ret) {
		dev_err(tcm->dev, "failed to initialize hardware: %d\n", ret);
		return ret;
	}

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &tcm->vid)) {
		dev_err(tcm->dev, "failed to find video device\n");
		return -ENODEV;
	}

	/* display size in chars: 67x74 */

	/* So it turns out this touchscreen will queue exactly ONE input event
	 * which is just perfect.
	 * we can poll it to know the last place it was touched!
	 */
	// printf("queueing inputs\n");
	// udelay(2500 * 1000);
	// printf("Parsing inputs soon\n");
	// udelay(1000 * 1000);
	// printf("Parsing inputs\n");

	start = get_timer(0);
	while (1 || get_timer(start) < 5000) {
		tcm_report_irq(0, tcm);
		//udelay(12500);
	}

	return 0;
}

static const struct udevice_id syna_device_ids[] = {
	{ .compatible = "syna,s3908" },
	{ /* sentinel */ }
};

struct keyboard_ops syna_tcm_s3908_ops = {
	.tstc = tcm_tstc,
};

U_BOOT_DRIVER(syna_tcm_s3908) = {
	.name = "syna_tcm_s3908",
	.id = UCLASS_KEYBOARD,
	.probe = tcm_probe,
	.priv_auto = sizeof(struct tcm_data),
	//.of_match = syna_device_ids,
	.ops = &syna_tcm_s3908_ops,
};
