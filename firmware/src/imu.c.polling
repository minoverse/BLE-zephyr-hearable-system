#include "imu.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_DBG);

static const struct device *imu_dev;

static inline int16_t sv_to_i16_milli(struct sensor_value v)
{
	int32_t x = (int32_t)v.val1 * 1000 + (int32_t)(v.val2 / 1000);

	if (x > INT16_MAX) x = INT16_MAX;
	if (x < INT16_MIN) x = INT16_MIN;

	return (int16_t)x;
}

int imu_init(void)
{
	imu_dev = DEVICE_DT_GET(DT_ALIAS(imu0));

	if (!device_is_ready(imu_dev)) {
		LOG_ERR("IMU not ready (check overlay + CONFIG_LSM6DSO + wiring)");
		return -ENODEV;
	}

	/* ODR set (104Hz) */
	struct sensor_value odr = { .val1 = 104, .val2 = 0 };
	int ret;

	ret = sensor_attr_set(imu_dev, SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (ret) {
		LOG_WRN("ACC ODR set failed: %d", ret);
	}

	ret = sensor_attr_set(imu_dev, SENSOR_CHAN_GYRO_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	if (ret) {
		LOG_WRN("GYR ODR set failed: %d", ret);
	}

	LOG_INF("IMU initialized");
	return 0;
}

int imu_read_sample(struct imu_sample *out)
{
	int ret;
	struct sensor_value ax, ay, az;
	struct sensor_value gx, gy, gz;

	if (!out || !imu_dev) {
		return -EINVAL;
	}

	ret = sensor_sample_fetch(imu_dev);
	if (ret) {
		return ret;
	}

	/* split channels (robust) */
	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_X, &ax);
	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_Y, &ay);
	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_Z, &az);

	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_X, &gx);
	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_Y, &gy);
	(void)sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_Z, &gz);

	out->accel_x = sv_to_i16_milli(ax);
	out->accel_y = sv_to_i16_milli(ay);
	out->accel_z = sv_to_i16_milli(az);

	out->gyro_x  = sv_to_i16_milli(gx);
	out->gyro_y  = sv_to_i16_milli(gy);
	out->gyro_z  = sv_to_i16_milli(gz);

	out->timestamp = k_uptime_get_32();

	return 0;
}
