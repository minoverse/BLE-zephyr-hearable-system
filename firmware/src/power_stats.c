#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/arch/cpu.h>

LOG_MODULE_REGISTER(power_stats, LOG_LEVEL_INF);

/* PM sleep duration tracking */
static uint32_t pm_entry_time_ms;
static uint64_t pm_total_sleep_ms;
static uint32_t pm_sleep_count;
static uint32_t pm_min_sleep_ms = UINT32_MAX;
static uint32_t pm_max_sleep_ms;

static void on_pm_state_entry(enum pm_state state)
{
    pm_entry_time_ms = k_uptime_get_32();
    LOG_DBG("PM entry: state=%d", (int)state);
}

static void on_pm_state_exit(enum pm_state state)
{
    uint32_t dur = k_uptime_get_32() - pm_entry_time_ms;
    LOG_DBG("PM exit: state=%d dur=%u ms", (int)state, dur);

    pm_total_sleep_ms += dur;
    pm_sleep_count++;

    if (dur < pm_min_sleep_ms) {
        pm_min_sleep_ms = dur;
    }
    if (dur > pm_max_sleep_ms) {
        pm_max_sleep_ms = dur;
    }
}

static struct pm_notifier pm_stats_notifier = {
    .state_entry = on_pm_state_entry,
    .state_exit  = on_pm_state_exit,
};

static struct k_work stats_work;
static uint32_t timer_fire_count;

static void do_print_stats(struct k_work *work)
{
    timer_fire_count++;
    struct k_thread_runtime_stats stats;
    int ret = k_thread_runtime_stats_all_get(&stats);
    if (ret != 0) {
        LOG_ERR("Failed to get runtime stats: %d", ret);
        return;
    }

    /*
     * execution_cycles includes active + idle thread time.
     * idle_pct = idle thread residency (plain WFI) — NOT the same as
     * PM state sleep.  PM sleep is a subset: the PM subsystem must also
     * decide to commit to a named power state, which triggers the notifier.
     */
    uint64_t total_cycles = stats.execution_cycles;
    uint32_t idle_pct = total_cycles
        ? (uint32_t)((stats.idle_cycles * 100U) / total_cycles)
        : 0U;

    /* Approximate idle wall-time from uptime × idle% */
    uint64_t idle_ms = (k_uptime_get() * idle_pct) / 100U;

    uint32_t avg_sleep_ms = pm_sleep_count
        ? (uint32_t)(pm_total_sleep_ms / pm_sleep_count)
        : 0U;

    uint32_t min_sleep_ms = (pm_sleep_count && pm_min_sleep_ms != UINT32_MAX)
        ? pm_min_sleep_ms : 0U;

    LOG_INF("=== Power / Sleep Stats [#%u] ===", timer_fire_count);
    LOG_INF("Idle residency:    %u%% (~%llu ms) [kernel WFI]", idle_pct, idle_ms);
    LOG_INF("PM sleep entries:  %u  [PM subsystem state transitions]", pm_sleep_count);
    LOG_INF("PM sleep dur (ms): avg=%u  min=%u  max=%u",
            avg_sleep_ms, min_sleep_ms, pm_max_sleep_ms);
    LOG_INF("Active cycles:     %llu", stats.execution_cycles - stats.idle_cycles);
    LOG_INF("Idle cycles:       %llu", stats.idle_cycles);

    if (idle_pct > 0 && pm_sleep_count == 0) {
        LOG_WRN("CPU is idle (%u%%) but PM subsystem never entered a power state.", idle_pct);
        LOG_WRN("BLE radio events likely keep idle windows shorter than PM min-residency.");
    }
}

static void stats_timer_cb(struct k_timer *timer)
{
    k_work_submit(&stats_work);
}

K_TIMER_DEFINE(stats_timer, stats_timer_cb, NULL);

void power_stats_init(void)
{
    k_work_init(&stats_work, do_print_stats);
    pm_notifier_register(&pm_stats_notifier);
    k_timer_start(&stats_timer, K_SECONDS(10), K_SECONDS(10));
    LOG_INF("Power stats enabled (10s interval)");
}

/* -------------------------------------------------------------------------
 * PM state hooks — required by the Zephyr PM core when HAS_PM is selected.
 *
 * The idle thread calls pm_system_suspend() which selects a power state from
 * the DTS cpu-power-states list, fires pm_notifier callbacks (entry), then
 * calls pm_state_set() with interrupts locked.  After the CPU wakes it calls
 * pm_state_exit_post_ops() to re-enable interrupts.
 * ------------------------------------------------------------------------- */

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
    ARG_UNUSED(state);
    ARG_UNUSED(substate_id);
    /* arch_cpu_idle(): sets BASEPRI=0, executes WFI, clears PRIMASK on wake.
     * Safe to call here — the PM core locks interrupts before entry and
     * pm_state_exit_post_ops() finishes the unlock via irq_unlock(0). */
    arch_cpu_idle();
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
    ARG_UNUSED(state);
    ARG_UNUSED(substate_id);
    /* Restore BASEPRI=0 so all interrupt priorities are unmasked. */
    irq_unlock(0);
}
