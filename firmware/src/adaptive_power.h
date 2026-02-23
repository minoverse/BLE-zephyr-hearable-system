#ifndef ADAPTIVE_POWER_H
#define ADAPTIVE_POWER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE_ULTRA_LOW_POWER,
    MODE_BALANCED,
    MODE_LOW_LATENCY
} power_mode_t;

power_mode_t adaptive_select_mode(bool connected, uint32_t motion_level);
void adaptive_update_motion(uint32_t motion_level);
void adaptive_set_mode(power_mode_t mode);
uint32_t adaptive_get_sampling_interval(void);
const char *adaptive_get_mode_name(void);
uint32_t adaptive_get_odr_hz(void);

#endif
