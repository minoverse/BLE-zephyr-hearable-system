#include "ble_adaptive.h"
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_adaptive, LOG_LEVEL_INF);

/* Raw pointer — valid between on_connected / on_disconnected callbacks. */
static struct bt_conn *current_conn;
static uint32_t tx_count;
static uint32_t last_tx_time;

static struct bt_le_conn_param pending_param;

/* Delayable work — handler runs in system work queue (thread context),
 * so bt_conn_ref() / bt_conn_le_param_update() are both legal here.
 * A plain k_timer ISR is NOT suitable because bt_conn_ref() calls
 * k_sem_take() internally and asserts when called from ISR context. */
static struct k_work_delayable policy_dwork;

static void policy_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    /* Snapshot tx_count before we potentially yield. */
    uint32_t count = tx_count;
    tx_count = 0;

    /* Take a ref in thread context — safe here (not ISR). */
    struct bt_conn *conn = current_conn;
    if (!conn) {
        goto reschedule;
    }
    conn = bt_conn_ref(conn);

    if (count > 100) {
        pending_param.interval_min = 8;
        pending_param.interval_max = 16;
        pending_param.latency = 0;
        pending_param.timeout = 400;
    } else if (count > 20) {
        pending_param.interval_min = 40;
        pending_param.interval_max = 80;
        pending_param.latency = 2;
        pending_param.timeout = 400;
    } else {
        pending_param.interval_min = 80;
        pending_param.interval_max = 160;
        pending_param.latency = 4;
        pending_param.timeout = 400;
    }

    int ret = bt_conn_le_param_update(conn, &pending_param);
    if (ret) {
        LOG_WRN("conn param update failed: %d", ret);
    }
    bt_conn_unref(conn);

reschedule:
    k_work_schedule(&policy_dwork, K_SECONDS(10));
}

/* ---- BT connection callbacks ---- */
static void on_ble_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        return;
    }
    current_conn = conn;
    LOG_INF("ble_adaptive: tracking connection");
}

static void on_ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(reason);
    if (conn == current_conn) {
        current_conn = NULL;
        LOG_INF("ble_adaptive: connection lost");
    }
}

BT_CONN_CB_DEFINE(adaptive_conn_cb) = {
    .connected    = on_ble_connected,
    .disconnected = on_ble_disconnected,
};

void ble_adaptive_register_tx(void) { tx_count++; last_tx_time = k_uptime_get_32(); }

void ble_adaptive_init(void)
{
    k_work_init_delayable(&policy_dwork, policy_work_handler);
    k_work_schedule(&policy_dwork, K_SECONDS(10));
    LOG_INF("BLE adaptive policy enabled");
}
