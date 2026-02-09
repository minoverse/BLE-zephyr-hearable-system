#include "gesture_thread.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_types.h"
#include "imu.h"
#include "gesture_detect.h"

LOG_MODULE_REGISTER(gesture_thread, LOG_LEVEL_INF);

/* queues set by start() */
static struct k_msgq *sq;
static struct k_msgq *gq;

/* gesture state/cfg */
static struct gesture_state gst;
static struct gesture_cfg gcfg;

/* threads */
#define IMU_STACK_SIZE     2048
#define IMU_THREAD_PRIO    7
#define GESTURE_STACK_SIZE 1024
#define GESTURE_THREAD_PRIO 8

K_THREAD_STACK_DEFINE(imu_stack, IMU_STACK_SIZE);
K_THREAD_STACK_DEFINE(gesture_stack, GESTURE_STACK_SIZE);

static struct k_thread imu_t;
static struct k_thread gest_t;

static void imu_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

	LOG_INF("IMU thread started");

	uint32_t cnt = 0;

	while (1) {
		struct imu_sample s;
		int ret = imu_read_sample(&s);

		if (ret == 0 && sq) {
			(void)k_msgq_put(sq, &s, K_NO_WAIT);
		}

		/* 너무 로그 많으면 느려짐: 2초에 한번 정도만 */
		cnt++;
		if ((cnt % 200) == 0) {
			LOG_INF("ACC(milli): x=%d y=%d z=%d",
				s.accel_x, s.accel_y, s.accel_z);
		}

		k_msleep(10); /* ~100Hz */
	}
}

static void gesture_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

	LOG_INF("Gesture thread started");

	struct imu_sample s;

	while (1) {
		if (!sq || !gq) {
			k_msleep(100);
			continue;
		}

		(void)k_msgq_get(sq, &s, K_FOREVER);

		enum gesture_type g =
			gesture_detect_update(&gst, &gcfg, &s, k_uptime_get_32());

		if (g != GESTURE_NONE) {
			struct gesture_event ev = {
				.type = (uint8_t)g,
				.timestamp = k_uptime_get_32(),
			};

			(void)k_msgq_put(gq, &ev, K_NO_WAIT);

			if (g == GESTURE_LEFT)  LOG_INF("Gesture: LEFT");
			if (g == GESTURE_RIGHT) LOG_INF("Gesture: RIGHT");
			if (g == GESTURE_NOD)   LOG_INF("Gesture: NOD");
		}
	}
}

int gesture_threads_start(struct k_msgq *sample_q, struct k_msgq *gesture_q)
{
	sq = sample_q;
	gq = gesture_q;

	gesture_detect_init(&gst);
	gcfg = gesture_cfg_default();

	/* init IMU once */
	int ret = imu_init();
	if (ret) {
		return ret;
	}

	k_thread_create(&imu_t, imu_stack, K_THREAD_STACK_SIZEOF(imu_stack),
			imu_thread_fn, NULL, NULL, NULL,
			IMU_THREAD_PRIO, 0, K_NO_WAIT);

	k_thread_create(&gest_t, gesture_stack, K_THREAD_STACK_SIZEOF(gesture_stack),
			gesture_thread_fn, NULL, NULL, NULL,
			GESTURE_THREAD_PRIO, 0, K_NO_WAIT);

	return 0;
}
