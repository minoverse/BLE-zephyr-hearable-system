#include "gesture_detect.h"

static inline int16_t i16_abs(int16_t x)
{
	return (x < 0) ? (int16_t)(-x) : x;
}

void gesture_detect_init(struct gesture_state *st)
{
	if (!st) {
		return;
	}
	st->last_fired = GESTURE_NONE;
	st->last_fire_time = 0;
	st->candidate = GESTURE_NONE;
	st->stable_cnt = 0;
}

struct gesture_cfg gesture_cfg_default(void)
{
	struct gesture_cfg cfg = {
		.th_x = 2500,
		.th_y = 2500,
		.deadzone = 1200,
		.stable_n = 3,
		.cooldown_ms = 700,
	};
	return cfg;
}

enum gesture_type gesture_detect_update(struct gesture_state *st,
					const struct gesture_cfg *cfg,
					const struct imu_sample *s,
					uint32_t now_ms)
{
	enum gesture_type g;

	if (!st || !cfg || !s) {
		return GESTURE_NONE;
	}

	/* 1) cooldown */
	if ((now_ms - st->last_fire_time) < cfg->cooldown_ms) {
		return GESTURE_NONE;
	}

	/* 2) must return to neutral (deadzone) before next event */
	if (st->last_fired != GESTURE_NONE) {
		if (i16_abs(s->accel_x) < cfg->deadzone &&
		    i16_abs(s->accel_y) < cfg->deadzone) {
			st->last_fired = GESTURE_NONE;
		} else {
			return GESTURE_NONE;
		}
	}

	/* 3) pick candidate */
	g = GESTURE_NONE;
	if (s->accel_x > cfg->th_x) {
		g = GESTURE_RIGHT;
	} else if (s->accel_x < (int16_t)(-cfg->th_x)) {
		g = GESTURE_LEFT;
	} else if (s->accel_y > cfg->th_y) {
		g = GESTURE_NOD;
	}

	/* 4) stability */
	if (g == GESTURE_NONE) {
		st->candidate = GESTURE_NONE;
		st->stable_cnt = 0;
		return GESTURE_NONE;
	}

	if (g == st->candidate) {
		st->stable_cnt++;
	} else {
		st->candidate = g;
		st->stable_cnt = 1;
	}

	if (st->stable_cnt >= cfg->stable_n) {
		st->last_fire_time = now_ms;
		st->last_fired = g;
		st->candidate = GESTURE_NONE;
		st->stable_cnt = 0;
		return g;
	}

	return GESTURE_NONE;
}
