#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/mutex.h>
#include <linux/err.h>

// per-device state
struct mcp9808_data {
	struct i2c_client *client;
	struct mutex lock; // serialize i2c in case multiple readers hit sysfs
};

static int mcp9808_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *val);

static umode_t mcp9808_is_visible(const void *drvdata, enum hwmon_sensor_types type, u32 attr, int channel);

// hwmon descriptors for our one temperature channel
static const u32 mcp9808_temp_config[] = {
    HWMON_T_INPUT, // one read-only temperature channel
    0, // terminator
};

// supported attributes for temperature sensor
static const struct hwmon_channel_info mcp9808_temp_info = {
    .type   = hwmon_temp,
    .config = mcp9808_temp_config,
};

// list of supported channel types
static const struct hwmon_channel_info * const mcp9808_info[] = {
    &mcp9808_temp_info,
    NULL,
};

// callback definitions
static const struct hwmon_ops mcp9808_ops = {
    .is_visible = mcp9808_is_visible, // tell hwmon which sysfs files to create
    .read = mcp9808_read, // function that returns value when userspace reads the input
};

// supported operations and descriptors
static const struct hwmon_chip_info mcp9808_chip_info = {
    .ops  = &mcp9808_ops, 
    .info = mcp9808_info,
};

// ID tables for matching: names -> driver ID
static const struct i2c_device_id mcp9808_ids[] = {
	{ "mcp9808-personal", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcp9808_ids); // metadata for user space, allows kernel to auto-load

// probe to have kernel match driver to device 
static int mcp9808_probe(struct i2c_client *client) {

    struct mcp9808_data *data;
    struct device *hwdev;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);

    if (!data) 
			return -ENOMEM; 

    data->client = client;
    mutex_init(&data->lock); // initialize the mutex to stop read and writes from racing

    i2c_set_clientdata(client, data); // stashes data pointer into device so we can retrieve it later
		hwdev = devm_hwmon_device_register_with_info(&client->dev, "mcp9808_personal", data, &mcp9808_chip_info, NULL); // register with hwmon as a temperature sensor

    if (IS_ERR(hwdev)) {
        int err = PTR_ERR(hwdev);
        dev_err(&client->dev, "hwmon register failed: %d\n", err);
        return err;
    }

    return 0;
}

static int mcp9808_read(struct device *dev, enum hwmon_sensor_types type, u32 attr, int channel, long *val)
{
    struct mcp9808_data *data = dev_get_drvdata(dev); // get per-device state
    s32 raw_word; // raw 16-bit word from smbus, but signed 32-bit return for error codes
    u16 temp_reg; // swapped big-endian version of register
    int temp_raw13; // 13-bit signed value extracted from register
    long temp_mc; // final m-celsius result

		// only support temp1_input
    if (type != hwmon_temp || attr != hwmon_temp_input)
			return -EOPNOTSUPP;

		// i2c read
    mutex_lock(&data->lock); // ensure no collisions
    raw_word = i2c_smbus_read_word_data(data->client, 0x05); // read register for temperature
    mutex_unlock(&data->lock);
    if (raw_word < 0) 
        return raw_word; // return word as lsb first, msb second (if no error)

		// swap to fix alignment for bit-masking
    temp_reg = (u16)raw_word; 
    temp_reg = (temp_reg << 8) | (temp_reg >> 8);

		// extract 13-bit signed value
    temp_raw13 = temp_reg & 0x1FFF;
    if (temp_raw13 & 0x1000) temp_raw13 -= 0x2000; // sign extend 13-bit

    temp_mc = DIV_ROUND_CLOSEST((long)temp_raw13 * 625, 10); // convert to milli-celsius
    *val = temp_mc;
    return 0;
}

// tell hwmon what files to export
static umode_t mcp9808_is_visible(const void *drvdata, enum hwmon_sensor_types type, u32 attr, int channel)
{
    if (type == hwmon_temp && channel == 0 && attr == hwmon_temp_input)
        return 0444; // read-only
    return 0;
}

static const struct of_device_id mcp9808_of_match[] = {
    { .compatible = "asolonari,mcp9808-personal" },
    { } // sentinel
};
MODULE_DEVICE_TABLE(of, mcp9808_of_match);

static struct i2c_driver mcp9808_driver = {
    .driver   = {
        .name           = "mcp9808-personal",
        .of_match_table = of_match_ptr(mcp9808_of_match),
    },
    .probe    = mcp9808_probe,
    .id_table = mcp9808_ids,
};
module_i2c_driver(mcp9808_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal hwmon driver for MCP9808");
MODULE_AUTHOR("Alexei Solonari <alexei@asolonari.com>");

