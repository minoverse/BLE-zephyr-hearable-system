#ifndef BLE_GESTURE_H_
#define BLE_GESTURE_H_

#include <zephyr/kernel.h>
#include <stdint.h>

/* BLE init + advertising */
int ble_gesture_init(void);

/* Bind the gesture_queue so the BLE transmission worker can empty it */
void ble_gesture_bind_queue(struct k_msgq *gesture_q);

/* Start the worker (periodic transmission) */
int ble_gesture_start(void);

#endif /* BLE_GESTURE_H_ */
