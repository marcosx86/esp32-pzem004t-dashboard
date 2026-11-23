#include <unity.h>
#include "Touch_Math.h"
#include <cmath>

namespace {

void assertDoubleNear(double expected, double actual, double tol) {
    TEST_ASSERT_TRUE(std::isfinite(actual));
    TEST_ASSERT_TRUE(std::abs(expected - actual) <= tol);
}

const TouchPoint kTargets[4] = {
    {20.0, 20.0},
    {300.0, 20.0},
    {20.0, 220.0},
    {300.0, 220.0}
};

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_exact_4pt_affine_recovery(void) {
    // True forward mapping matching default 90 deg rotation + small scale/offset
    const TouchPoint raw[4] = {
        {220.0, 20.0},
        {220.0, 300.0},
        {20.0, 20.0},
        {20.0, 300.0}
    };

    TouchCalibration solved;
    double max_err = -1.0;
    TEST_ASSERT_TRUE(solveAffineCalibration4Pt(raw, kTargets, solved, &max_err));
    TEST_ASSERT_TRUE(solved.calibrated);
    assertDoubleNear(0.0, max_err, 1e-6);

    assertDoubleNear(0.0, solved.a, 1e-6);
    assertDoubleNear(1.0, solved.b, 1e-6);
    assertDoubleNear(0.0, solved.c, 1e-6);
    assertDoubleNear(-1.0, solved.d, 1e-6);
    assertDoubleNear(0.0, solved.e, 1e-6);
    assertDoubleNear(240.0, solved.f, 1e-6);
}

void test_noisy_4pt_least_squares_succeeds(void) {
    const TouchPoint raw[4] = {
        {221.0, 19.0},
        {219.0, 301.0},
        {19.0, 21.0},
        {21.0, 299.0}
    };

    TouchCalibration solved;
    double max_err = -1.0;
    TEST_ASSERT_TRUE(solveAffineCalibration4Pt(raw, kTargets, solved, &max_err));
    TEST_ASSERT_TRUE(solved.calibrated);
    TEST_ASSERT_TRUE(max_err <= 5.0);
}

void test_high_residual_mistap_rejected(void) {
    // 4th corner tapped wildly wrong
    const TouchPoint raw[4] = {
        {220.0, 20.0},
        {220.0, 300.0},
        {20.0, 20.0},
        {180.0, 50.0}
    };

    TouchCalibration solved;
    double max_err = 0.0;
    TEST_ASSERT_FALSE(solveAffineCalibration4Pt(raw, kTargets, solved, &max_err, 25.0));
    TEST_ASSERT_FALSE(solved.calibrated);
    TEST_ASSERT_TRUE(max_err > 25.0);
}

void test_collinear_points_rejected(void) {
    const TouchPoint raw[4] = {
        {100.0, 20.0},
        {100.0, 100.0},
        {100.0, 200.0},
        {100.0, 300.0}
    };

    TouchCalibration solved;
    TEST_ASSERT_FALSE(solveAffineCalibration4Pt(raw, kTargets, solved));
    TEST_ASSERT_FALSE(solved.calibrated);
    assertDoubleNear(240.0, solved.f, 1e-6);
}

void test_apply_touch_calibration_and_clamping(void) {
    TouchCalibration def = defaultTouchCalibration();
    int16_t x = 0, y = 0;

    // Default landscape: x = raw_y, y = 240 - raw_x
    applyTouchCalibration(def, 50, 120, 320, 240, x, y);
    TEST_ASSERT_EQUAL_INT16(120, x);
    TEST_ASSERT_EQUAL_INT16(190, y);

    // Boundary clamping
    applyTouchCalibration(def, 300, 500, 320, 240, x, y);
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(0, y);

    // Calibrated mapping with offset/scale + upper clamp
    TouchCalibration custom{0.0, 1.0, 5.0, -1.0, 0.0, 245.0, true};
    applyTouchCalibration(custom, 20, 100, 320, 240, x, y);
    TEST_ASSERT_EQUAL_INT16(105, x);
    TEST_ASSERT_EQUAL_INT16(225, y);

    applyTouchCalibration(custom, 0, 319, 320, 240, x, y);
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(239, y);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_exact_4pt_affine_recovery);
    RUN_TEST(test_noisy_4pt_least_squares_succeeds);
    RUN_TEST(test_high_residual_mistap_rejected);
    RUN_TEST(test_collinear_points_rejected);
    RUN_TEST(test_apply_touch_calibration_and_clamping);
    return UNITY_END();
}
