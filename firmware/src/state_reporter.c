#include "state_reporter.h"
#include "fault_recovery.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_report, LOG_LEVEL_INF);

static struct {
    uint32_t gestures_detected;
    uint32_t errors_count;
} report_data = {0};

void state_reporter_increment_gestures(void) { report_data.gestures_detected++; }
void state_reporter_increment_errors(void)   { report_data.errors_count++; }

static void do_report(struct k_work *work)
{
    char buf[128];
    uint32_t uptime = k_uptime_get_32() / 1000;

    snprintk(buf, sizeof(buf),
             "{\"state\":\"%s\",\"uptime\":%u,\"gestures\":%u,\"errors\":%u}",
             fault_recovery_get_state_string(),
             uptime,
             report_data.gestures_detected,
             report_data.errors_count);

    LOG_INF("=== State Report === %s", buf);
}

K_WORK_DEFINE(report_work, do_report);

static void report_timer_cb(struct k_timer *t)
{
    k_work_submit(&report_work);
}

K_TIMER_DEFINE(report_timer, report_timer_cb, NULL);

void state_reporter_init(void)
{
    k_timer_start(&report_timer, K_SECONDS(30), K_SECONDS(30));
    LOG_INF("State reporter initialized");
}
