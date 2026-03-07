#include <zephyr/ztest.h>
#include "gesture_detect.h"

static struct gesture_state st;
static struct gesture_cfg cfg;

static void gesture_before(void *f)
{
    gesture_detect_init(&st);
    cfg = gesture_cfg_default();
}

/* Test 1: NULL input returns GESTURE_NONE */
ZTEST(gesture_suite, test_null_input)
{
    enum gesture_type r = gesture_detect_update(NULL, &cfg, NULL, 0);
    zassert_equal(r, GESTURE_NONE, "NULL input should return GESTURE_NONE");
}

/* Test 2: Right gesture detected */
ZTEST(gesture_suite, test_right_gesture)
{
    struct imu_sample s = { .accel_x = 3000, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    /* Need stable_n=3 consecutive samples */
    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_RIGHT, "Expected GESTURE_RIGHT");
}

/* Test 3: Left gesture detected */
ZTEST(gesture_suite, test_left_gesture)
{
    struct imu_sample s = { .accel_x = -3000, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_LEFT, "Expected GESTURE_LEFT");
}

/* Test 4: Below threshold returns GESTURE_NONE */
ZTEST(gesture_suite, test_below_threshold)
{
    struct imu_sample s = { .accel_x = 1000, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r = gesture_detect_update(&st, &cfg, &s, 1000);
    zassert_equal(r, GESTURE_NONE, "Below threshold should return GESTURE_NONE");
}

/* Test 5: Cooldown prevents immediate re-trigger */
ZTEST(gesture_suite, test_cooldown)
{
    struct imu_sample s_trig = { .accel_x = 3000, .accel_y = 0, .accel_z = 0 };
    struct imu_sample s_neutral = { .accel_x = 0, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    /* Trigger gesture */
    gesture_detect_update(&st, &cfg, &s_trig, 1000);
    gesture_detect_update(&st, &cfg, &s_trig, 1001);
    gesture_detect_update(&st, &cfg, &s_trig, 1002);

    /* Return to neutral */
    gesture_detect_update(&st, &cfg, &s_neutral, 1003);

    /* Try again immediately — should be blocked by cooldown */
    gesture_detect_update(&st, &cfg, &s_trig, 1004);
    gesture_detect_update(&st, &cfg, &s_trig, 1005);
    r = gesture_detect_update(&st, &cfg, &s_trig, 1006);

    zassert_equal(r, GESTURE_NONE, "Cooldown should block re-trigger");
}

/* Test 6: NOD gesture detected */
ZTEST(gesture_suite, test_nod_gesture)
{
    struct imu_sample s = { .accel_x = 0, .accel_y = 3000, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_NOD, "Expected GESTURE_NOD");
}

ZTEST_SUITE(gesture_suite, NULL, NULL, gesture_before, NULL, NULL);

/* Test 7: Boundary — exactly at threshold does NOT trigger */
ZTEST(gesture_suite, test_boundary_at_threshold)
{
    struct imu_sample s = { .accel_x = 2500, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_NONE, "At threshold should NOT trigger");
}

/* Test 8: Boundary — threshold+1 DOES trigger */
ZTEST(gesture_suite, test_boundary_above_threshold)
{
    struct imu_sample s = { .accel_x = 2501, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_RIGHT, "threshold+1 should trigger GESTURE_RIGHT");
}

/* Test 9: Boundary — threshold-1 does NOT trigger */
ZTEST(gesture_suite, test_boundary_below_threshold)
{
    struct imu_sample s = { .accel_x = 2499, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    gesture_detect_update(&st, &cfg, &s, 1001);
    r = gesture_detect_update(&st, &cfg, &s, 1002);

    zassert_equal(r, GESTURE_NONE, "threshold-1 should NOT trigger");
}

/* Test 10: Stability filter — 2 samples not enough */
ZTEST(gesture_suite, test_stability_insufficient)
{
    struct imu_sample s = { .accel_x = 3000, .accel_y = 0, .accel_z = 0 };
    enum gesture_type r;

    gesture_detect_update(&st, &cfg, &s, 1000);
    r = gesture_detect_update(&st, &cfg, &s, 1001);

    zassert_equal(r, GESTURE_NONE, "2 samples insufficient for stable_n=3");
}
