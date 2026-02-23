#ifndef IMU_H
#define IMU_H

#include <zephyr/kernel.h>
#include "app_types.h"

int imu_init(void);
int imu_read_sample(struct imu_sample *sample);
int imu_trigger_start(struct k_msgq *queue);
int imu_set_odr(uint32_t hz);
void imu_thread_entry(void *p1, void *p2, void *p3);

#endif
