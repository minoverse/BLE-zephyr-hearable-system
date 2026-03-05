#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "power_stats.h"
#include "app_types.h"
#include "gesture_thread.h"
#if defined(CONFIG_BT)
#include "gesture_service.h"
#include "ble_adaptive.h"
#include "network_stress.h"
#include <zephyr/dfu/mcuboot.h>
#include <hal/nrf_power.h>
#include "fault_recovery.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define SAMPLE_QUEUE_SIZE  32
#define GESTURE_QUEUE_SIZE 128

K_MSGQ_DEFINE(sample_queue, sizeof(struct imu_sample), SAMPLE_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(gesture_queue, sizeof(struct gesture_event), GESTURE_QUEUE_SIZE, 4);

int main(void)
{
    LOG_INF("========================================");
    LOG_INF("BLE Hearable System v%d.%d.%d",
            APP_VERSION_MAJOR,
            APP_VERSION_MINOR,
            APP_VERSION_PATCH);
    LOG_INF("========================================");
    uint32_t _boot_ms = k_cyc_to_ms_near32(k_cycle_get_32());
	LOG_INF("Boot time: %u ms", _boot_ms);
	/* Reset reason from POWER register */
	uint32_t resetreas = NRF_POWER->RESETREAS;
	NRF_POWER->RESETREAS = 0xFFFFFFFF;
	if (resetreas & 0x01) LOG_INF("Reset reason: PIN (reset button)");
	if (resetreas & 0x02) LOG_INF("Reset reason: Watchdog");
	if (resetreas & 0x04) LOG_INF("Reset reason: SREQ (soft reset)");
	if (resetreas & 0x08) LOG_INF("Reset reason: LOCKUP (HardFault)");
	if (resetreas == 0)   LOG_INF("Reset reason: Power-on reset");
	LOG_INF("Boot: BLE Hearable (IMU->Gesture->BLE)");

    power_stats_init();
	fault_recovery_init();
	if (boot_is_img_confirmed() == 0) {
		int rc = boot_write_img_confirmed();
		if (rc == 0) LOG_INF("OTA: image confirmed");
		else LOG_ERR("OTA: confirm failed: %d", rc);
	} else {
		LOG_INF("OTA: image already confirmed");
	}

    int ret = gesture_threads_start(&sample_queue, &gesture_queue);
    if (ret) {
        LOG_ERR("gesture_threads_start failed: %d", ret);
        return 0;
    }

#if defined(CONFIG_BT)
    ble_gesture_bind_queue(&gesture_queue);
    network_stress_set_queue(&gesture_queue);

    ret = ble_gesture_init();
    if (ret) {
        LOG_ERR("ble_gesture_init failed: %d", ret);
        return 0;
    }

    ble_adaptive_init();

    ret = ble_gesture_start();
    if (ret) {
        LOG_ERR("ble_gesture_start failed: %d", ret);
        return 0;
    }

    k_sleep(K_SECONDS(10));
    network_stress_run();
#endif

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
