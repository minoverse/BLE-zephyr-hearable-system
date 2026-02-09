#ifndef GESTURE_DETECT_H_
#define GESTURE_DETECT_H_

#include <stdint.h>
#include "app_types.h"

struct gesture_state {
	enum gesture_type last_fired;
	uint32_t last_fire_time;

	enum gesture_type candidate;
	uint8_t stable_cnt;
};

/* Tunning parameter (Later easy for Kconfig) */
struct gesture_cfg {
	int16_t th_x;        /* left/right threshold (milli-units) */
	int16_t th_y;        /* nod threshold (milli-units) */
	int16_t deadzone;    /* must return to neutral */
	uint8_t stable_n;    /* consecutive samples required */
	uint32_t cooldown_ms;/* debounce */
};

/* default value */
void gesture_detect_init(struct gesture_state *st);
struct gesture_cfg gesture_cfg_default(void);

/* Update: input sample + now_ms -> gesture_type */
enum gesture_type gesture_detect_update(struct gesture_state *st,
					const struct gesture_cfg *cfg,
					const struct imu_sample *s,
					uint32_t now_ms);

#endif /* GESTURE_DETECT_H_ */
