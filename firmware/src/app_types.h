#ifndef APP_TYPES_H_
#define APP_TYPES_H_

#include <stdint.h>

struct imu_sample {
	int16_t accel_x, accel_y, accel_z;
	int16_t gyro_x,  gyro_y,  gyro_z;
	uint32_t timestamp; /* ms */
};

enum gesture_type {
	GESTURE_NONE  = 0,
	GESTURE_NOD   = 1,
	GESTURE_LEFT  = 2,
	GESTURE_RIGHT = 3,
};

struct gesture_event {
	uint8_t type;       /* enum gesture_type */
	uint32_t timestamp; /* ms */
};

#endif /* APP_TYPES_H_ */

