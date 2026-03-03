#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_types.h"

LOG_MODULE_REGISTER(net_stress, LOG_LEVEL_INF);

static struct k_msgq *gq;

static struct {
    uint32_t tx_attempts;
    uint32_t tx_success;
    uint32_t tx_failed;
    uint32_t latency_sum_ms;
    uint32_t latency_max_ms;
} metrics = {0};

void network_stress_set_queue(struct k_msgq *q) { gq = q; }

void network_stress_print(void)
{
    uint32_t rate = metrics.tx_attempts ?
                    (metrics.tx_success * 100) / metrics.tx_attempts : 0;
    uint32_t avg = metrics.tx_success ?
                   metrics.latency_sum_ms / metrics.tx_success : 0;

    LOG_INF("=== Stress Test Results ===");
    LOG_INF("Attempts: %u", metrics.tx_attempts);
    LOG_INF("Success:  %u (%u%%)", metrics.tx_success, rate);
    LOG_INF("Failed:   %u", metrics.tx_failed);
    LOG_INF("Avg latency: %u ms", avg);
    LOG_INF("Max latency: %u ms", metrics.latency_max_ms);
    memset(&metrics, 0, sizeof(metrics));
}

void network_stress_run(void)
{
    if (!gq) { LOG_ERR("Queue not set"); return; }

    LOG_INF("=== Network Stress: 100 bursts ===");

    for (int i = 0; i < 100; i++) {
        uint32_t start = k_uptime_get_32();
        metrics.tx_attempts++;

        struct gesture_event ev = { .type = 1, .timestamp = start };
        int ret = k_msgq_put(gq, &ev, K_MSEC(50));

        uint32_t lat = k_uptime_get_32() - start;

        if (ret == 0) {
            metrics.tx_success++;
            metrics.latency_sum_ms += lat;
            if (lat > metrics.latency_max_ms) metrics.latency_max_ms = lat;
        } else {
            metrics.tx_failed++;
        }
        k_msleep(50);
    }
    network_stress_print();
}
