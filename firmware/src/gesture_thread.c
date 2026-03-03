#include "adaptive_power.h"
#include "gesture_thread.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_types.h"
#include "imu.h"
#include "gesture_detect.h"

LOG_MODULE_REGISTER(gesture_thread, LOG_LEVEL_INF);

static struct k_msgq *sq;
static struct k_msgq *gq;

static struct gesture_state gst;
static struct gesture_cfg gcfg;

#define IMU_STACK_SIZE      2048
#define IMU_THREAD_PRIO     7
#define GESTURE_STACK_SIZE  2048  /* needs room for I2C (sensor_attr_set) + LOG formatting */
#define GESTURE_THREAD_PRIO 8

K_THREAD_STACK_DEFINE(imu_stack, IMU_STACK_SIZE);
K_THREAD_STACK_DEFINE(gesture_stack, GESTURE_STACK_SIZE);

static struct k_thread imu_t;
static struct k_thread gest_t;

static void imu_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("IMU thread started");

    int ret = imu_trigger_start(sq);
    if (ret == 0) {
        /* Interrupt-driven: thread just waits, handler feeds the queue */
        while (1) {
            k_sleep(K_FOREVER);
        }
    }

    /* Trigger not available (e.g. TRIGGER_NONE) — fall back to polling */
    LOG_WRN("Trigger unavailable (%d), polling at ~1.56 Hz", ret);
    while (1) {
        struct imu_sample s;

        if (imu_read_sample(&s) == 0) {
            s.timestamp = k_uptime_get_32();
            k_msgq_put(sq, &s, K_NO_WAIT);
        }
        k_sleep(K_MSEC(641)); /* 1.56 Hz */
    }
}

static void gesture_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Gesture thread started");

    gesture_detect_init(&gst);
    gcfg = gesture_cfg_default();

    uint32_t cnt = 0;
    /* Expected period in ms — updated whenever ODR changes */
    uint32_t expected_period_ms = 641U; /* initial: 1.56 Hz */
    power_mode_t last_mode = MODE_BALANCED; /* matches adaptive_power.c initial value */
    /* Minimum acceptable pipeline performance (latency budget) */
    static const uint32_t perf_threshold_pct = 85U;

    while (1) {
        struct imu_sample s;

        int ret = k_msgq_get(sq, &s, K_FOREVER);

        if (ret == 0) {
            uint32_t now_ms = k_uptime_get_32();
            uint32_t latency_ms = now_ms - s.timestamp;

            /* Performance = how much of the period is NOT consumed by latency */
            uint32_t latency_pct = (latency_ms * 100U) / expected_period_ms;
            uint32_t perf_pct = (latency_pct >= 100U) ? 0U : (100U - latency_pct);

            /* Warn on every sample that misses the 85% target */
            if (perf_pct < perf_threshold_pct) {
                LOG_WRN("PERF LOW: latency=%u ms -> perf=%u%% (need >=%u%%)",
                        latency_ms, perf_pct, perf_threshold_pct);
            }

            /* Update adaptive mode.
             * Subtract 1g (9800 milli-g) from Z to remove gravity bias so
             * a stationary device does not look like high motion.
             */
            int32_t az_debiased = s.accel_z - 9800;
            int32_t motion = (s.accel_x < 0 ? -s.accel_x : s.accel_x) +
                             (s.accel_y < 0 ? -s.accel_y : s.accel_y) +
                             (az_debiased < 0 ? -az_debiased : az_debiased);
            uint32_t motion_level = (uint32_t)(motion / 10);
            adaptive_update_motion(motion_level);
            power_mode_t new_mode = adaptive_select_mode(true, motion_level);
            adaptive_set_mode(new_mode);
            if (new_mode != last_mode) {
                last_mode = new_mode;
                uint32_t odr_hz = adaptive_get_odr_hz();
                int r = imu_set_odr(odr_hz);
                if (r) {
                    LOG_WRN("ODR update failed: %d", r);
                } else {
                    /* Sync latency baseline: period_ms = 1000 / odr_hz */
                    expected_period_ms = (odr_hz > 0) ? (1000U / odr_hz) : 641U;
                    LOG_INF("ODR updated: %u Hz (period ~%u ms)",
                            odr_hz, expected_period_ms);
                }
            }

            cnt++;
            /* DBG-level per-sample logs — too frequent at high ODR for INF */
            LOG_DBG("[%u] latency=%u ms | perf=%u%% %s | mode=%s",
                    cnt, latency_ms, perf_pct,
                    (perf_pct >= perf_threshold_pct) ? "OK" : "WARN",
                    adaptive_get_mode_name());

            /* Log sensor data every 100 samples */
            if (cnt % 100 == 0) {
                LOG_INF("[%u] ACC(milli): x=%d y=%d z=%d | mode=%s",
                        cnt, s.accel_x, s.accel_y, s.accel_z,
                        adaptive_get_mode_name());
            }

            enum gesture_type g = gesture_detect_update(&gst, &gcfg,
                                                        &s, now_ms);

            if (g != GESTURE_NONE && gq) {
                LOG_INF("Gesture %d detected: latency=%u ms | perf=%u%%",
                        g, latency_ms, perf_pct);
                struct gesture_event ev = {
                    .type = g,
                    .timestamp = now_ms,
                };
                k_msgq_put(gq, &ev, K_NO_WAIT);
            }
        }
    }
}

int gesture_threads_start(struct k_msgq *sample_q, struct k_msgq *gesture_q)
{
    sq = sample_q;
    gq = gesture_q;

    int ret = imu_init();
    if (ret) {
        return ret;
    }

    k_thread_create(&imu_t, imu_stack, IMU_STACK_SIZE,
                    imu_thread_fn, NULL, NULL, NULL,
                    IMU_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&imu_t, "imu");

    k_thread_create(&gest_t, gesture_stack, GESTURE_STACK_SIZE,
                    gesture_thread_fn, NULL, NULL, NULL,
                    GESTURE_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&gest_t, "gesture");

    return 0;
}
