#include "watchdog.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

static const struct device *wdt;
static int wdt_channel_id;

static void wdt_callback(const struct device *dev, int channel_id)
{
    LOG_ERR("Watchdog expired! System hung — rebooting");
}

int watchdog_init(void)
{
    wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
    if (!device_is_ready(wdt)) {
        LOG_ERR("WDT device not ready");
        return -ENODEV;
    }

    struct wdt_timeout_cfg cfg = {
        .flags = WDT_FLAG_RESET_SOC,
        .window.min = 0,
        .window.max = 5000, /* 5 second timeout */
        .callback = wdt_callback,
    };

    wdt_channel_id = wdt_install_timeout(wdt, &cfg);
    if (wdt_channel_id < 0) {
        LOG_ERR("WDT install timeout failed: %d", wdt_channel_id);
        return wdt_channel_id;
    }

    int ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (ret < 0) {
        LOG_ERR("WDT setup failed: %d", ret);
        return ret;
    }

    LOG_INF("Watchdog initialized (5s timeout)");
    return 0;
}

void watchdog_feed(void)
{
    wdt_feed(wdt, wdt_channel_id);
}
