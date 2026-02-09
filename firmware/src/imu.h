#ifndef IMU_H_
#define IMU_H_

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "app_types.h"

/* IMU module: init + read_sample  */
int imu_init(void);
int imu_read_sample(struct imu_sample *out);

#endif /* IMU_H_ */
