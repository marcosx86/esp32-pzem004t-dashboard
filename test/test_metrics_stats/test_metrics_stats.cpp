#include <unity.h>
#include "Metrics_Stats.h"
#include <cmath>

namespace {

void assertDoubleNear(double expected, double actual, double tol) {
    TEST_ASSERT_TRUE(std::isfinite(actual));
    TEST_ASSERT_TRUE(std::abs(expected - actual) <= tol);
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_ring_buffer_empty_nan_and_wraparound(void) {
    MetricRingBuffer rb;
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(rb.size()));
    MetricStats empty_stats = rb.computeStats();
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(empty_stats.count));
    TEST_ASSERT_TRUE(std::isnan(empty_stats.min_val));
    TEST_ASSERT_TRUE(std::isnan(empty_stats.avg_val));
    TEST_ASSERT_TRUE(std::isnan(empty_stats.max_val));

    // Non-finite samples must be ignored
    rb.push(NAN);
    rb.push(INFINITY);
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(rb.size()));

    // Push 75 samples (1..75); last 60 must be 16..75
    for (int i = 1; i <= 75; ++i) {
        rb.push(static_cast<double>(i));
    }
    TEST_ASSERT_EQUAL_UINT32(kMetricsHistorySize, static_cast<uint32_t>(rb.size()));

    MetricStats stats = rb.computeStats();
    TEST_ASSERT_EQUAL_UINT32(kMetricsHistorySize, static_cast<uint32_t>(stats.count));
    assertDoubleNear(16.0, stats.min_val, 1e-6);
    assertDoubleNear(45.5, stats.avg_val, 1e-6);
    assertDoubleNear(75.0, stats.max_val, 1e-6);

    double ordered[kMetricsHistorySize];
    size_t copied = rb.copyOrdered(ordered, kMetricsHistorySize);
    TEST_ASSERT_EQUAL_UINT32(kMetricsHistorySize, static_cast<uint32_t>(copied));
    assertDoubleNear(16.0, ordered[0], 1e-6);
    assertDoubleNear(75.0, ordered[kMetricsHistorySize - 1], 1e-6);

    double tail[5];
    size_t tail_copied = rb.copyOrdered(tail, 5);
    TEST_ASSERT_EQUAL_UINT32(5, static_cast<uint32_t>(tail_copied));
    assertDoubleNear(71.0, tail[0], 1e-6);
    assertDoubleNear(75.0, tail[4], 1e-6);
}

void test_metrics_history_push_and_clear(void) {
    MetricsHistory hist;
    MetricsData m = makeEmptyMetrics();
    m.voltage = 115.0;
    m.current = 2.5;
    m.energy = 10.0;
    m.wattage = 280.0;
    hist.push(m);

    TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(hist.voltage.size()));
    TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(hist.wattage.size()));
    hist.clear();
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(hist.voltage.size()));
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(hist.wattage.size()));
}

void test_stale_detection_and_uint32_rollover(void) {
    TEST_ASSERT_TRUE(isMetricsStale(false, 1000, 1000, 5000));
    TEST_ASSERT_FALSE(isMetricsStale(true, 1000, 6000, 5000));
    TEST_ASSERT_TRUE(isMetricsStale(true, 1000, 6001, 5000));

    // Rollover from 0xFFFFFF00 -> 0x00000100 is 512 ms elapsed (fresh)
    TEST_ASSERT_FALSE(isMetricsStale(true, 0xFFFFFF00u, 0x00000100u, 5000));
    // Rollover from 0xFFFFFF00 -> 0x00002000 is 8448 ms elapsed (stale)
    TEST_ASSERT_TRUE(isMetricsStale(true, 0xFFFFFF00u, 0x00002000u, 5000));
}

void test_voltage_current_and_dynamic_power_alerts(void) {
    AlertThresholds def; // <95V, >127V, 10A
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluateVoltageAlert(NAN, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluateVoltageAlert(115.0, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::WARN), static_cast<int>(evaluateVoltageAlert(96.5, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::WARN), static_cast<int>(evaluateVoltageAlert(125.0, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::CRIT), static_cast<int>(evaluateVoltageAlert(94.9, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::CRIT), static_cast<int>(evaluateVoltageAlert(127.1, def)));

    AlertThresholds disabled_v{0.0f, 0.0f, 0.0f};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluateVoltageAlert(80.0, disabled_v)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluateCurrentAlert(15.0, disabled_v)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluatePowerAlert(3000.0, 120.0, disabled_v)));

    // Current alert
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluateCurrentAlert(8.4, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::WARN), static_cast<int>(evaluateCurrentAlert(8.5, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::CRIT), static_cast<int>(evaluateCurrentAlert(10.1, def)));

    // Dynamic power alert: 110V * 10A = 1100W
    assertDoubleNear(1100.0, computeDynamicPowerLimit(110.0, 10.0f), 1e-6);
    assertDoubleNear(1270.0, computeDynamicPowerLimit(NAN, 10.0f), 1e-6);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::OK), static_cast<int>(evaluatePowerAlert(934.0, 110.0, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::WARN), static_cast<int>(evaluatePowerAlert(935.0, 110.0, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::CRIT), static_cast<int>(evaluatePowerAlert(1101.0, 110.0, def)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(AlertLevel::CRIT), static_cast<int>(evaluatePowerAlert(1271.0, NAN, def)));
}

void test_session_energy_and_tariff_cost(void) {
    SessionEnergyInfo normal = computeSessionEnergy(255.03, 250.03, 500.0, 0.80f);
    assertDoubleNear(5.0, normal.session_kwh, 1e-6);
    assertDoubleNear(4.0, normal.session_cost, 1e-6);
    assertDoubleNear(0.4, normal.hourly_cost, 1e-6);

    // PZEM counter reset guard
    SessionEnergyInfo reset = computeSessionEnergy(2.0, 250.03, 500.0, 0.80f);
    assertDoubleNear(0.0, reset.session_kwh, 1e-6);
    assertDoubleNear(0.0, reset.session_cost, 1e-6);
    assertDoubleNear(0.4, reset.hourly_cost, 1e-6);

    // Missing baseline & zero tariff
    SessionEnergyInfo no_base = computeSessionEnergy(10.0, NAN, 500.0, 0.0f);
    assertDoubleNear(0.0, no_base.session_kwh, 1e-6);
    assertDoubleNear(0.0, no_base.session_cost, 1e-6);
    assertDoubleNear(0.0, no_base.hourly_cost, 1e-6);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_ring_buffer_empty_nan_and_wraparound);
    RUN_TEST(test_metrics_history_push_and_clear);
    RUN_TEST(test_stale_detection_and_uint32_rollover);
    RUN_TEST(test_voltage_current_and_dynamic_power_alerts);
    RUN_TEST(test_session_energy_and_tariff_cost);
    return UNITY_END();
}
