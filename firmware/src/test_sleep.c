#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>

LOG_MODULE_REGISTER(test_sleep, LOG_LEVEL_INF);

static void test_print(struct k_timer *t)
{
    LOG_INF("=== ALIVE ===");
}

K_TIMER_DEFINE(test_timer, test_print, NULL);

void test_sleep_init(void)
{
    k_timer_start(&test_timer, K_SECONDS(10), K_SECONDS(10));
}
