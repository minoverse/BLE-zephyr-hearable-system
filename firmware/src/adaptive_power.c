#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(adaptive, LOG_LEVEL_INF);

typedef enum {
    MODE_ULTRA_LOW_POWER,
    MODE_BALANCED,
    MODE_LOW_LATENCY
} power_mode_t;

static power_mode_t current_mode = MODE_BALANCED;
static uint32_t last_motion_time = 0;

static const struct {
    uint32_t sampling_interval_ms;
    uint32_t odr_hz;
    const char *name;
} mode_config[] = {
    [MODE_ULTRA_LOW_POWER] = {100,  1, "Ultra Low Power"},
    [MODE_BALANCED]        = { 20, 12, "Balanced"},
    [MODE_LOW_LATENCY]     = { 10, 52, "Low Latency"},
};

power_mode_t adaptive_select_mode(bool connected, uint32_t motion_level)
{
    if (!connected) return MODE_ULTRA_LOW_POWER;
    uint32_t idle_time = k_uptime_get_32() - last_motion_time;
    if (motion_level > 80 || idle_time < 2000) return MODE_LOW_LATENCY;
    if (idle_time < 10000) return MODE_BALANCED;
    return MODE_ULTRA_LOW_POWER;
}

void adaptive_update_motion(uint32_t motion_level)
{
    if (motion_level > 50) last_motion_time = k_uptime_get_32();
}

uint32_t adaptive_get_sampling_interval(void)
{
    return mode_config[current_mode].sampling_interval_ms;
}

void adaptive_set_mode(power_mode_t mode)
{
    if (mode != current_mode) {
        current_mode = mode;
        LOG_INF("Power mode: %s (%ums)",
                mode_config[mode].name,
                mode_config[mode].sampling_interval_ms);
    }
}

const char *adaptive_get_mode_name(void)
{
    return mode_config[current_mode].name;
}

uint32_t adaptive_get_odr_hz(void)
{
    return mode_config[current_mode].odr_hz;
}
