#include <unity.h>
#include "Network_Store.h"

void setUp(void) {}
void tearDown(void) {}

void test_serialize_and_parse_empty_and_open_networks(void) {
    std::vector<StoredWifiCredential> empty;
    TEST_ASSERT_EQUAL_STRING("", serializeWifiCredentials(empty).c_str());
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(parseWifiCredentials("").size()));

    // Single open network (empty password)
    std::vector<StoredWifiCredential> open_only{{"OpenCafe", ""}};
    std::string serialized_open = serializeWifiCredentials(open_only);
    TEST_ASSERT_EQUAL_STRING("OpenCafe\n", serialized_open.c_str());
    auto parsed_open = parseWifiCredentials(serialized_open);
    TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(parsed_open.size()));
    TEST_ASSERT_EQUAL_STRING("OpenCafe", parsed_open[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", parsed_open[0].password.c_str());

    // Multi-network roundtrip with empty password first
    std::vector<StoredWifiCredential> multi{{"Guest", ""}, {"Home", "s3cr3t!"}};
    auto parsed_multi = parseWifiCredentials(serializeWifiCredentials(multi));
    TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(parsed_multi.size()));
    TEST_ASSERT_EQUAL_STRING("Guest", parsed_multi[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", parsed_multi[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Home", parsed_multi[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("s3cr3t!", parsed_multi[1].password.c_str());
}

void test_add_update_promotes_index_zero_and_deduplicates(void) {
    std::vector<StoredWifiCredential> list{{"Office", "1111"}, {"Home", "2222"}};

    // Empty SSID rejected
    TEST_ASSERT_FALSE(addOrUpdateWifiCredential(list, "", "pass"));
    TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(list.size()));

    // Explicit add of new network goes to index 0
    TEST_ASSERT_TRUE(addOrUpdateWifiCredential(list, "Lab", "3333"));
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(list.size()));
    TEST_ASSERT_EQUAL_STRING("Lab", list[0].ssid.c_str());

    // Case-insensitive update moves entry to index 0 without duplicate
    TEST_ASSERT_TRUE(addOrUpdateWifiCredential(list, "home", "newpass"));
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(list.size()));
    TEST_ASSERT_EQUAL_STRING("home", list[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("newpass", list[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("Lab", list[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("Office", list[2].ssid.c_str());

    // Editing index 2 moves updated network to index 0
    TEST_ASSERT_TRUE(addOrUpdateWifiCredential(list, "OfficeRenamed", "9999", 2));
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(list.size()));
    TEST_ASSERT_EQUAL_STRING("OfficeRenamed", list[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("9999", list[0].password.c_str());
}

void test_delete_wifi_credential_bounds(void) {
    std::vector<StoredWifiCredential> list{{"A", "1"}, {"B", "2"}};
    TEST_ASSERT_FALSE(deleteWifiCredential(list, -1));
    TEST_ASSERT_FALSE(deleteWifiCredential(list, 2));
    TEST_ASSERT_TRUE(deleteWifiCredential(list, 0));
    TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(list.size()));
    TEST_ASSERT_EQUAL_STRING("B", list[0].ssid.c_str());
}

void test_build_metrics_url_normalization(void) {
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50/metrics", buildMetricsUrl("192.168.1.50", "/metrics").c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50:8080/metrics", buildMetricsUrl("192.168.1.50:8080/", "").c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50/custom", buildMetricsUrl("http://192.168.1.50/", "custom").c_str());
    TEST_ASSERT_EQUAL_STRING("https://sensor.local/v1/prom", buildMetricsUrl("https://sensor.local", "/v1/prom").c_str());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_serialize_and_parse_empty_and_open_networks);
    RUN_TEST(test_add_update_promotes_index_zero_and_deduplicates);
    RUN_TEST(test_delete_wifi_credential_bounds);
    RUN_TEST(test_build_metrics_url_normalization);
    return UNITY_END();
}
