#ifndef GESTURE_THREAD_H_
#define GESTURE_THREAD_H_

#include <zephyr/kernel.h>

/* sample_queue -> gesture detect -> gesture_queue */
int gesture_threads_start(struct k_msgq *sample_q,
			  struct k_msgq *gesture_q);

#endif /* GESTURE_THREAD_H_ */
