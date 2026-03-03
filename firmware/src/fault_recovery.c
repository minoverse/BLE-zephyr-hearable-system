#include "fault_recovery.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include "imu.h"
#include "gesture_service.h"

LOG_MODULE_REGISTER(fault_recovery, LOG_LEVEL_INF);

typedef enum {
    STATE_HEALTHY,
    STATE_DEGRADED,
    STATE_RECOVERING,
    STATE_FAILED,
} system_state_t;

static system_state_t current_state = STATE_HEALTHY;

static struct {
    uint32_t imu_failures;
    uint32_t ble_disconnects;
    uint32_t last_health_check;
} health = {0};

static int recover_imu(void)
{
    LOG_WRN("IMU fault detected, attempting recovery...");
    int ret = imu_init();
    if (ret == 0) {
        LOG_INF("IMU recovered");
        health.imu_failures = 0;
        return 0;
    }
    health.imu_failures++;
    LOG_ERR("IMU recovery failed (%d attempts)", health.imu_failures);
    return -1;
}

static int recover_ble(void)
{
    LOG_WRN("BLE disconnect, restarting advertising...");
    int ret = ble_gesture_start();
    if (ret == 0) {
        LOG_INF("BLE advertising restarted");
        health.ble_disconnects = 0;
        return 0;
    }
    health.ble_disconnects++;
    LOG_ERR("BLE recovery failed");
    return -1;
}

static void update_system_state(void)
{
    uint32_t total = health.imu_failures + health.ble_disconnects;

    switch (current_state) {
    case STATE_HEALTHY:
        if (total > 0) {
            current_state = STATE_DEGRADED;
            LOG_WRN("State: HEALTHY -> DEGRADED");
        }
        break;
    case STATE_DEGRADED:
        if (total == 0) {
            current_state = STATE_HEALTHY;
            LOG_INF("State: DEGRADED -> HEALTHY");
        } else if (total > 5) {
            current_state = STATE_RECOVERING;
            LOG_ERR("State: DEGRADED -> RECOVERING");
        }
        break;
    case STATE_RECOVERING:
        if (health.imu_failures > 0) recover_imu();
        if (health.ble_disconnects > 0) recover_ble();
        if (total == 0) {
            current_state = STATE_HEALTHY;
            LOG_INF("State: RECOVERING -> HEALTHY");
        } else if (total > 10) {
            current_state = STATE_FAILED;
            LOG_ERR("State: RECOVERING -> FAILED");
        }
        break;
    case STATE_FAILED:
        LOG_ERR("System failed, rebooting...");
        k_sleep(K_SECONDS(5));
        sys_reboot(SYS_REBOOT_COLD);
        break;
    }
}

void fault_recovery_report_failure(const char *component)
{
    LOG_ERR("Failure: %s", component);
    if (strcmp(component, "imu") == 0) {
        health.imu_failures++;
        recover_imu();
    } else if (strcmp(component, "ble") == 0) {
        health.ble_disconnects++;
        recover_ble();
    }
    update_system_state();
}

static void health_check_fn(void *a, void *b, void *c)
{
    while (1) {
        update_system_state();
        uint32_t now = k_uptime_get_32();
        if (now - health.last_health_check > 60000) {
            LOG_INF("=== Health === state=%s imu_fail=%u ble_disc=%u",
                    fault_recovery_get_state_string(),
                    health.imu_failures,
                    health.ble_disconnects);
            health.last_health_check = now;
        }
        k_sleep(K_SECONDS(10));
    }
}

K_THREAD_DEFINE(health_tid, 1024, health_check_fn, NULL, NULL, NULL, 5, 0, 0);

const char* fault_recovery_get_state_string(void)
{
    switch (current_state) {
    case STATE_HEALTHY:   return "healthy";
    case STATE_DEGRADED:  return "degraded";
    case STATE_RECOVERING:return "recovering";
    case STATE_FAILED:    return "failed";
    default:              return "unknown";
    }
}

void fault_recovery_init(void)
{
    LOG_INF("Fault recovery initialized");
}
