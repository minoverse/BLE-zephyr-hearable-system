/*
 * Combined test main.c:
 * - BLE Vendor service (abcdef1) Indication (only when CCC enabled)
 * - LSM6DSO: IMU thread -> sample_queue
 * - Gesture thread -> state-based detect + debounce -> gesture_queue
 *
 * Fixes:
 * - NO ABS macro usage (uses i16_abs)
 * - Safe write_vnd (no void* arithmetic)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static inline int16_t i16_abs(int16_t x) { return (x < 0) ? (int16_t)-x : x; }

/* ================= IMU sample + queue ================= */
struct imu_sample {
	int16_t accel_x, accel_y, accel_z;
	int16_t gyro_x,  gyro_y,  gyro_z;
	uint32_t timestamp;
};

#define SAMPLE_QUEUE_SIZE 32
K_MSGQ_DEFINE(sample_queue, sizeof(struct imu_sample), SAMPLE_QUEUE_SIZE, 4);

/* sensor_value -> int16 (milli scaling) */
static inline int16_t sv_to_i16_milli(struct sensor_value v)
{
	int32_t x = (int32_t)v.val1 * 1000 + (int32_t)(v.val2 / 1000);
	if (x > INT16_MAX) x = INT16_MAX;
	if (x < INT16_MIN) x = INT16_MIN;
	return (int16_t)x;
}

/* ================= Gesture ================= */
enum gesture_type {
	GESTURE_NONE  = 0,
	GESTURE_NOD   = 1,
	GESTURE_LEFT  = 2,
	GESTURE_RIGHT = 3,
};

struct gesture_event {
	uint8_t type;
	uint32_t timestamp;
};

#define GESTURE_QUEUE_SIZE 16
K_MSGQ_DEFINE(gesture_queue, sizeof(struct gesture_event), GESTURE_QUEUE_SIZE, 4);

/* tuning */
#define TH_X        2500
#define TH_Y        2500
#define DEADZONE    1200
#define STABLE_N    3
#define COOLDOWN_MS 700

static enum gesture_type detect_gesture_state(const struct imu_sample *s, uint32_t now_ms)
{
	static enum gesture_type last_fired = GESTURE_NONE;
	static uint32_t last_fire_time = 0;

	static enum gesture_type candidate = GESTURE_NONE;
	static uint8_t stable_cnt = 0;

	if (now_ms - last_fire_time < COOLDOWN_MS) {
		return GESTURE_NONE;
	}

	if (last_fired != GESTURE_NONE) {
		if (i16_abs(s->accel_x) < DEADZONE && i16_abs(s->accel_y) < DEADZONE) {
			last_fired = GESTURE_NONE;
		} else {
			return GESTURE_NONE;
		}
	}

	enum gesture_type g = GESTURE_NONE;
	if (s->accel_x > TH_X)        g = GESTURE_RIGHT;
	else if (s->accel_x < -TH_X)  g = GESTURE_LEFT;
	else if (s->accel_y > TH_Y)   g = GESTURE_NOD;

	if (g == GESTURE_NONE) {
		candidate = GESTURE_NONE;
		stable_cnt = 0;
		return GESTURE_NONE;
	}

	if (g == candidate) {
		stable_cnt++;
	} else {
		candidate = g;
		stable_cnt = 1;
	}

	if (stable_cnt >= STABLE_N) {
		last_fire_time = now_ms;
		last_fired = g;
		candidate = GESTURE_NONE;
		stable_cnt = 0;
		return g;
	}

	return GESTURE_NONE;
}

/* ================= BLE Vendor Service ================= */
#define BT_UUID_CUSTOM_SERVICE_VAL \
	BT_UUID_128_ENCODE(0x12345678,0x1234,0x5678,0x1234,0x56789abcdef0)

static const struct bt_uuid_128 vnd_uuid =
	BT_UUID_INIT_128(BT_UUID_CUSTOM_SERVICE_VAL);

static const struct bt_uuid_128 vnd_ind_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x12345678,0x1234,0x5678,0x1234,0x56789abcdef1));

#define VND_MAX_LEN 20
static uint8_t vnd_value[VND_MAX_LEN + 1] = { 'V','e','n','d','o','r',0 };

static uint8_t simulate_vnd;
static uint8_t indicating;
static struct bt_gatt_indicate_params ind_params;
static uint8_t ind_data[5];

static ssize_t read_vnd(struct bt_conn *c, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const char *value = (const char *)attr->user_data;
	return bt_gatt_attr_read(c, attr, buf, len, offset, value, strlen(value));
}

static ssize_t write_vnd(struct bt_conn *c, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(c);
	ARG_UNUSED(flags);

	uint8_t *value = (uint8_t *)attr->user_data;

	if (offset + len > VND_MAX_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);
	value[offset + len] = 0;

	return len;
}

static void vnd_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t v)
{
	ARG_UNUSED(attr);
	simulate_vnd = (v == BT_GATT_CCC_INDICATE);
	LOG_INF("Indication %s", simulate_vnd ? "ON" : "OFF");
}

static void indicate_cb(struct bt_conn *c, struct bt_gatt_indicate_params *p, uint8_t e)
{
	ARG_UNUSED(c);
	ARG_UNUSED(p);
	indicating = 0;
	if (e) {
		LOG_WRN("Indication fail err=%u", e);
	}
}

BT_GATT_SERVICE_DEFINE(vnd_svc,
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),
	BT_GATT_CHARACTERISTIC(&vnd_ind_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_vnd, write_vnd, vnd_value),
	BT_GATT_CCC(vnd_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

/* Advertising */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_CUSTOM_SERVICE_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ================= Indication worker ================= */
static void ind_work(struct k_work *w);
K_WORK_DELAYABLE_DEFINE(ind_w, ind_work);

static void ind_work(struct k_work *w)
{
	ARG_UNUSED(w);

	if (simulate_vnd && !indicating) {
		struct gesture_event ev;
		if (k_msgq_get(&gesture_queue, &ev, K_NO_WAIT) == 0) {
			ind_data[0] = ev.type;
			sys_put_le32(ev.timestamp, &ind_data[1]);

			ind_params.attr = &vnd_svc.attrs[1];
			ind_params.func = indicate_cb;
			ind_params.data = ind_data;
			ind_params.len  = sizeof(ind_data);

			if (bt_gatt_indicate(NULL, &ind_params) == 0) {
				indicating = 1;
			}
		}
	}

	k_work_schedule(&ind_w, K_MSEC(200));
}

/* ================= IMU thread ================= */
#define IMU_STACK 2048
K_THREAD_STACK_DEFINE(imu_stack, IMU_STACK);
static struct k_thread imu_t;

static void imu_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	const struct device *imu = DEVICE_DT_GET(DT_ALIAS(imu0));
	if (!device_is_ready(imu)) {
		LOG_ERR("IMU not ready (check overlay + CONFIG_LSM6DSO)");
		return;
	}

	struct sensor_value odr = { .val1 = 104, .val2 = 0 };
	(void)sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
	(void)sensor_attr_set(imu, SENSOR_CHAN_GYRO_XYZ,  SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);

	LOG_INF("IMU thread started");

	while (1) {
		struct imu_sample s = {0};
		struct sensor_value ax, ay, az, gx, gy, gz;

		if (sensor_sample_fetch(imu) == 0) {
			(void)sensor_channel_get(imu, SENSOR_CHAN_ACCEL_X, &ax);
			(void)sensor_channel_get(imu, SENSOR_CHAN_ACCEL_Y, &ay);
			(void)sensor_channel_get(imu, SENSOR_CHAN_ACCEL_Z, &az);
			(void)sensor_channel_get(imu, SENSOR_CHAN_GYRO_X,  &gx);
			(void)sensor_channel_get(imu, SENSOR_CHAN_GYRO_Y,  &gy);
			(void)sensor_channel_get(imu, SENSOR_CHAN_GYRO_Z,  &gz);

			s.accel_x = sv_to_i16_milli(ax);
			s.accel_y = sv_to_i16_milli(ay);
			s.accel_z = sv_to_i16_milli(az);
			s.gyro_x  = sv_to_i16_milli(gx);
			s.gyro_y  = sv_to_i16_milli(gy);
			s.gyro_z  = sv_to_i16_milli(gz);
			s.timestamp = k_uptime_get_32();

			(void)k_msgq_put(&sample_queue, &s, K_NO_WAIT);
		}

		k_msleep(10);
	}
}

/* ================= Gesture thread ================= */
#define GESTURE_STACK 1024
K_THREAD_STACK_DEFINE(gesture_stack, GESTURE_STACK);
static struct k_thread gest_t;

static void gesture_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	LOG_INF("Gesture thread started");

	struct imu_sample s;

	while (1) {
		(void)k_msgq_get(&sample_queue, &s, K_FOREVER);

		enum gesture_type g = detect_gesture_state(&s, k_uptime_get_32());
		if (g != GESTURE_NONE) {
			struct gesture_event ev = { .type = (uint8_t)g, .timestamp = k_uptime_get_32() };
			(void)k_msgq_put(&gesture_queue, &ev, K_NO_WAIT);

			if (g == GESTURE_LEFT)  LOG_INF("Gesture: LEFT");
			if (g == GESTURE_RIGHT) LOG_INF("Gesture: RIGHT");
			if (g == GESTURE_NOD)   LOG_INF("Gesture: NOD");
		}
	}
}

int main(void)
{
	LOG_INF("Boot");

	k_thread_create(&imu_t, imu_stack, IMU_STACK,
			imu_thread, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_create(&gest_t, gesture_stack, GESTURE_STACK,
			gesture_thread, NULL, NULL, NULL,
			8, 0, K_NO_WAIT);

	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return 0;
	}
	if (IS_ENABLED(CONFIG_SETTINGS)) {
   	    settings_load();   
	}	

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("adv start failed: %d", err);
		return 0;
	}

	k_work_schedule(&ind_w, K_MSEC(200));

	while (1) {
		k_sleep(K_SECONDS(1));
	}
}
