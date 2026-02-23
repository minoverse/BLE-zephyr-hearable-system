/*
 * BLE gesture service:
 * - Custom 128-bit service + notify characteristic
 * - Worker reads gesture_queue and notifies when CCC enabled
 */

#include "gesture_service.h"
#include "app_types.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(gesture_service, LOG_LEVEL_INF);

static void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	ARG_UNUSED(conn);
	LOG_INF("MTU updated: tx=%u rx=%u", tx, rx);
}

static struct bt_gatt_cb gatt_cb = {
	.att_mtu_updated = mtu_updated,
};


/* UUIDs */
#define BT_UUID_GESTURE_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define BT_UUID_GESTURE_EVENT_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static const struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(BT_UUID_GESTURE_SERVICE_VAL);
static const struct bt_uuid_128 evt_uuid = BT_UUID_INIT_128(BT_UUID_GESTURE_EVENT_VAL);

/* payload: [0]=type, [1..4]=timestamp(le32) */
static uint8_t gesture_event_data[6];

static struct k_msgq *gq;
static bool notify_enabled;

/* ---- GATT callbacks ---- */
static ssize_t read_gesture_event(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 gesture_event_data, sizeof(gesture_event_data));
}

static void gesture_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Notifications %s", notify_enabled ? "enabled" : "disabled");
}

/* ---- Service ---- */
BT_GATT_SERVICE_DEFINE(gesture_svc,
	BT_GATT_PRIMARY_SERVICE(&svc_uuid),
	BT_GATT_CHARACTERISTIC(&evt_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_gesture_event, NULL, gesture_event_data),
	BT_GATT_CCC(gesture_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ---- Advertising ---- */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_GESTURE_SERVICE_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ---- Worker: pop gesture queue -> notify ---- */
static void ble_notify_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(ble_notify_work, ble_notify_work_handler);

static void ble_notify_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (notify_enabled && gq != NULL) {
		struct gesture_event ev;

		if (k_msgq_get(gq, &ev, K_NO_WAIT) == 0) {
			gesture_event_data[0] = 1; /* version */
			gesture_event_data[1] = ev.type;
			sys_put_le32(ev.timestamp, &gesture_event_data[2]);

			int err = bt_gatt_notify(NULL, &gesture_svc.attrs[2],
						 gesture_event_data, sizeof(gesture_event_data));
			if (err) {
				LOG_WRN("bt_gatt_notify err=%d", err);
			} else {
				LOG_INF("Notified: v=%u type=%u ts=%u", gesture_event_data[0], ev.type, ev.timestamp);
			}
		}
	}

	k_work_schedule(&ble_notify_work, K_MSEC(200));
}

/* ---- Connection parameter update (for PM sleep) ---- */
/*
 * Request 100–500 ms connection interval so the BLE radio is idle long
 * enough for the PM subsystem to enter a named power state between events.
 * Unit = 1.25 ms  →  80 = 100 ms,  400 = 500 ms
 */
#define CONN_INTERVAL_MIN  80   /* 100 ms */
#define CONN_INTERVAL_MAX  400  /* 500 ms */

static const struct bt_le_conn_param slow_params =
	BT_LE_CONN_PARAM_INIT(CONN_INTERVAL_MIN, CONN_INTERVAL_MAX, 0, 400);

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed: %u", err);
		return;
	}
	LOG_INF("Connected — requesting slow conn interval (%d–%d * 1.25ms)",
		CONN_INTERVAL_MIN, CONN_INTERVAL_MAX);
	int ret = bt_conn_le_param_update(conn, &slow_params);
	if (ret) {
		LOG_WRN("conn param update request failed: %d", ret);
	}
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
}

BT_CONN_CB_DEFINE(conn_cb) = {
	.connected    = on_connected,
	.disconnected = on_disconnected,
};

/* ---- Public API ---- */
void ble_gesture_bind_queue(struct k_msgq *gesture_q)
{
	gq = gesture_q;
}

int ble_gesture_init(void)
{
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return err;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising start failed: %d", err);
		return err;
	}

	bt_gatt_cb_register(&gatt_cb);
	LOG_INF("BLE advertising started");
	return 0;
}

int ble_gesture_start(void)
{
	/* start periodic worker */
	k_work_schedule(&ble_notify_work, K_MSEC(200));
	return 0;
}
