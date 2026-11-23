#include <unity.h>
#include "Metrics_Parser.h"
#include <cmath>

namespace {

void assertDoubleNear(double expected, double actual, double tol) {
    TEST_ASSERT_TRUE(std::isfinite(actual));
    TEST_ASSERT_TRUE(std::abs(expected - actual) <= tol);
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_parse_metrics_example_all_eight_metrics(void) {
    const char *payload =
        "# HELP pzem_voltage Volts\n"
        "# TYPE pzem_voltage gauge\n"
        "pzem_voltage 106.20\n"
        "# HELP pzem_current Amps\n"
        "# TYPE pzem_current gauge\n"
        "pzem_current 4.09\n"
        "# HELP pzem_power Watts\n"
        "# TYPE pzem_power gauge\n"
        "pzem_power 413.20\n"
        "# HELP pzem_energy kWh\n"
        "# TYPE pzem_energy counter\n"
        "pzem_energy 255.03\n"
        "# HELP pzem_frequency Hz\n"
        "# TYPE pzem_frequency gauge\n"
        "pzem_frequency 60.00\n"
        "# HELP pzem_power_factor Ratio\n"
        "# TYPE pzem_power_factor gauge\n"
        "pzem_power_factor 0.95\n"
        "# HELP dht22_temperature Celsius\n"
        "# TYPE dht22_temperature gauge\n"
        "dht22_temperature 26.60\n"
        "# HELP dht22_humidity Percent\n"
        "# TYPE dht22_humidity gauge\n"
        "dht22_humidity 84.30";

    MetricsData out;
    TEST_ASSERT_TRUE(parseMetricsPayload(payload, out));
    assertDoubleNear(106.20, out.voltage, 1e-4);
    assertDoubleNear(4.09, out.current, 1e-4);
    assertDoubleNear(413.20, out.wattage, 1e-4);
    assertDoubleNear(255.03, out.energy, 1e-4);
    assertDoubleNear(60.00, out.frequency, 1e-4);
    assertDoubleNear(0.95, out.power_factor, 1e-4);
    assertDoubleNear(26.60, out.temperature, 1e-4);
    assertDoubleNear(84.30, out.humidity, 1e-4);
}

void test_parse_tab_delimiter_and_crlf(void) {
    const char *payload =
        "  pzem_voltage\t119.50\r\n"
        "pzem_power\t\t512.75\r\n";

    MetricsData out;
    TEST_ASSERT_TRUE(parseMetricsPayload(payload, out));
    assertDoubleNear(119.50, out.voltage, 1e-4);
    assertDoubleNear(512.75, out.wattage, 1e-4);
    TEST_ASSERT_TRUE(std::isnan(out.current));
}

void test_parse_labels_timestamp_and_prefix_isolation(void) {
    const char *payload =
        "pzem_power_factor{device=\"main\",note=\"a } b\"} 0.92 1700000000\n"
        "pzem_power{device=\"main\\\"1\"} 350.25\n";

    MetricsData out;
    TEST_ASSERT_TRUE(parseMetricsPayload(payload, out));
    assertDoubleNear(0.92, out.power_factor, 1e-4);
    assertDoubleNear(350.25, out.wattage, 1e-4);
}

void test_parse_invalid_values_stay_nan(void) {
    const char *payload =
        "pzem_voltage abc\n"
        "pzem_current 4.2xyz\n"
        "pzem_power NaN\n"
        "pzem_energy\n";

    MetricsData out;
    TEST_ASSERT_FALSE(parseMetricsPayload(payload, out));
    TEST_ASSERT_TRUE(std::isnan(out.voltage));
    TEST_ASSERT_TRUE(std::isnan(out.current));
    TEST_ASSERT_TRUE(std::isnan(out.wattage));
    TEST_ASSERT_TRUE(std::isnan(out.energy));
}

void test_parse_empty_null_and_comments_only(void) {
    MetricsData out;
    TEST_ASSERT_FALSE(parseMetricsPayload(nullptr, out));
    TEST_ASSERT_TRUE(std::isnan(out.voltage));

    TEST_ASSERT_FALSE(parseMetricsPayload("", out));
    TEST_ASSERT_FALSE(parseMetricsPayload("# comment only\n   \n# another", out));
}

void test_parse_partial_ambient_only(void) {
    const char *payload =
        "unknown_metric 42\n"
        "dht22_temperature 23.4\n"
        "dht22_humidity 61.2\n";

    MetricsData out;
    TEST_ASSERT_TRUE(parseMetricsPayload(payload, out));
    assertDoubleNear(23.4, out.temperature, 1e-4);
    assertDoubleNear(61.2, out.humidity, 1e-4);
    TEST_ASSERT_TRUE(std::isnan(out.voltage));
    TEST_ASSERT_TRUE(std::isnan(out.wattage));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_parse_metrics_example_all_eight_metrics);
    RUN_TEST(test_parse_tab_delimiter_and_crlf);
    RUN_TEST(test_parse_labels_timestamp_and_prefix_isolation);
    RUN_TEST(test_parse_invalid_values_stay_nan);
    RUN_TEST(test_parse_empty_null_and_comments_only);
    RUN_TEST(test_parse_partial_ambient_only);
    return UNITY_END();
}
