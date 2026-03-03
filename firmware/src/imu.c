#include "imu.h"
#include "fault_recovery.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

static const struct device *imu_dev;
static struct k_msgq *imu_sample_queue;

static void imu_trigger_handler(const struct device *dev,
                                const struct sensor_trigger *trig)
{
    struct imu_sample sample;
    struct sensor_value accel[3];

    if (sensor_sample_fetch(dev) < 0) {
        return;
    }

    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);

    sample.accel_x = (int16_t)(accel[0].val1 * 1000 + accel[0].val2 / 1000);
    sample.accel_y = (int16_t)(accel[1].val1 * 1000 + accel[1].val2 / 1000);
    sample.accel_z = (int16_t)(accel[2].val1 * 1000 + accel[2].val2 / 1000);
    sample.timestamp = k_uptime_get_32();

    if (imu_sample_queue) {
        k_msgq_put(imu_sample_queue, &sample, K_NO_WAIT);
    }
}

int imu_init(void)
{
    imu_dev = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));

    if (!device_is_ready(imu_dev)) {
        LOG_ERR("IMU not ready");
        return -ENODEV;
    }

    LOG_INF("IMU initialized");
    return 0;
}

int imu_read_sample(struct imu_sample *sample)
{
    struct sensor_value accel[3];

    if (sensor_sample_fetch(imu_dev) < 0) {
        return -EIO;
    }

    sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, accel);

    sample->accel_x = (int16_t)(accel[0].val1 * 1000 + accel[0].val2 / 1000);
    sample->accel_y = (int16_t)(accel[1].val1 * 1000 + accel[1].val2 / 1000);
    sample->accel_z = (int16_t)(accel[2].val1 * 1000 + accel[2].val2 / 1000);

    return 0;
}

int imu_set_odr(uint32_t hz)
{
    struct sensor_value odr = { .val1 = (int32_t)hz, .val2 = 0 };
    int ret = sensor_attr_set(imu_dev, SENSOR_CHAN_ACCEL_XYZ,
                              SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
    if (ret) {
        LOG_ERR("ODR set failed: %d", ret);
    }
    return ret;
}

int imu_trigger_start(struct k_msgq *queue)
{
    imu_sample_queue = queue;

    /* CRITICAL: Set sampling frequency FIRST */
    struct sensor_value odr_attr;
    odr_attr.val1 = 1;  /* 1.56 Hz */
    odr_attr.val2 = 0;
    
    if (sensor_attr_set(imu_dev, SENSOR_CHAN_ACCEL_XYZ,
                       SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
        LOG_ERR("Cannot set sampling frequency");
        return -EIO;
    }
    
    LOG_INF("Sampling freq set: 1.56 Hz");

    /* Now set trigger */
    static struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    int ret = sensor_trigger_set(imu_dev, &trig, imu_trigger_handler);
    if (ret) {
        LOG_ERR("Trigger set failed: %d", ret);
        return ret;
    }

    LOG_INF("IMU trigger started (interrupt-driven)");
    return 0;
}

void imu_thread_entry(void *p1, void *p2, void *p3)
{
    struct k_msgq *queue = (struct k_msgq *)p1;

    int ret = imu_trigger_start(queue);
    if (ret) {
        LOG_ERR("Failed to start trigger: %d", ret);
        return;
    }

    LOG_INF("IMU thread: event-driven (CPU sleeping)");

    while (1) {
        k_sleep(K_FOREVER);
    }
}
