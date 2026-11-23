#include <unity.h>
#include "UI_Logic.h"
#include <cmath>

void setUp(void) {}
void tearDown(void) {}

void test_normalize_theme_id_defaults_to_primitive(void) {
    TEST_ASSERT_EQUAL_INT(0, normalizeThemeId(0));
    TEST_ASSERT_EQUAL_INT(1, normalizeThemeId(1));
    TEST_ASSERT_EQUAL_INT(0, normalizeThemeId(-1));
    TEST_ASSERT_EQUAL_INT(0, normalizeThemeId(2));
    TEST_ASSERT_EQUAL_INT(0, normalizeThemeId(99));
}

void test_theme_palettes_match_specification(void) {
    const UiThemePalette &prim = getThemePalette(0);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, prim.screen_bg);
    TEST_ASSERT_EQUAL_HEX32(0x4A90E2, prim.card_bg[0]);
    TEST_ASSERT_EQUAL_HEX32(0xE74C3C, prim.card_bg[1]);
    TEST_ASSERT_EQUAL_HEX32(0x2ECC71, prim.card_bg[2]);
    TEST_ASSERT_EQUAL_HEX32(0xF39C12, prim.card_bg[3]);
    TEST_ASSERT_EQUAL_INT(2, prim.card_border_width);
    TEST_ASSERT_EQUAL_HEX32(0x63A986, prim.settings_bg);
    TEST_ASSERT_EQUAL_HEX32(0xFFB19A, prim.editor_bg);

    const UiThemePalette &dark = getThemePalette(1);
    TEST_ASSERT_EQUAL_HEX32(0x12161C, dark.screen_bg);
    TEST_ASSERT_EQUAL_HEX32(0x1D2430, dark.card_bg[0]);
    TEST_ASSERT_EQUAL_HEX32(0x1D2430, dark.card_bg[3]);
    TEST_ASSERT_EQUAL_HEX32(0x4A90E2, dark.card_border[0]);
    TEST_ASSERT_EQUAL_HEX32(0xF39C12, dark.card_border[3]);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, dark.card_value);
    TEST_ASSERT_EQUAL_INT(3, dark.card_border_width);
    TEST_ASSERT_EQUAL_HEX32(0x18202A, dark.settings_bg);

    TEST_ASSERT_EQUAL_HEX32(0x63A986, getThemePalette(42).settings_bg);
}

void test_hud_freshness_and_colors(void) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(false, true, false, 1000, 1200, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0xE74C3C, getFreshnessDotColor(HudFreshness::ERROR));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::STALE),
                          static_cast<int>(computeHudFreshness(true, false, false, 0, 1200, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0xF39C12, getFreshnessDotColor(HudFreshness::STALE));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(true, false, true, 0, 1200, 5000)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::LIVE),
                          static_cast<int>(computeHudFreshness(true, true, false, 1000, 6000, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0x2ECC71, getFreshnessDotColor(HudFreshness::LIVE));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::STALE),
                          static_cast<int>(computeHudFreshness(true, true, true, 1000, 3000, 5000)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(true, true, true, 1000, 6001, 5000)));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::LIVE),
                          static_cast<int>(computeHudFreshness(true, true, false, 0xFFFFFF00u, 0x00000100u, 5000)));
}

void test_rssi_to_signal_bars(void) {
    TEST_ASSERT_EQUAL_INT(0, rssiToSignalBars(false, -40));
    TEST_ASSERT_EQUAL_INT(1, rssiToSignalBars(true, -90));
    TEST_ASSERT_EQUAL_INT(2, rssiToSignalBars(true, -80));
    TEST_ASSERT_EQUAL_INT(2, rssiToSignalBars(true, -75));
    TEST_ASSERT_EQUAL_INT(3, rssiToSignalBars(true, -67));
    TEST_ASSERT_EQUAL_INT(3, rssiToSignalBars(true, -60));
    TEST_ASSERT_EQUAL_INT(4, rssiToSignalBars(true, -55));
    TEST_ASSERT_EQUAL_INT(4, rssiToSignalBars(true, -45));
}

void test_scale_samples_to_chart(void) {
    int16_t dst[10];
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(scaleSamplesToChart(nullptr, 5, dst, 10)));
    double dummy[2] = {1.0, 2.0};
    TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(scaleSamplesToChart(dummy, 0, dst, 10)));

    // Constant values map to midpoint 500
    double flat[3] = {120.0, 120.0, 120.0};
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(scaleSamplesToChart(flat, 3, dst, 10, 0, 1000)));
    TEST_ASSERT_EQUAL_INT16(500, dst[0]);
    TEST_ASSERT_EQUAL_INT16(500, dst[1]);
    TEST_ASSERT_EQUAL_INT16(500, dst[2]);

    // Normal series scaled with 5% vertical padding inside [0, 1000]
    double ramp[3] = {0.0, 50.0, 100.0};
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(scaleSamplesToChart(ramp, 3, dst, 10, 0, 1000)));
    TEST_ASSERT_TRUE(dst[0] > 0 && dst[0] < 100);
    TEST_ASSERT_EQUAL_INT16(500, dst[1]);
    TEST_ASSERT_TRUE(dst[2] > 900 && dst[2] < 1000);

    // Truncation keeps most recent max_len values; skips non-finite safely
    double tail_src[6] = {1.0, 2.0, 10.0, NAN, 20.0, 30.0};
    TEST_ASSERT_EQUAL_UINT32(3, static_cast<uint32_t>(scaleSamplesToChart(tail_src, 6, dst, 4, 0, 1000)));
    TEST_ASSERT_TRUE(dst[0] < dst[1] && dst[1] < dst[2]);
}

void test_alert_visuals_and_autodim_mapping(void) {
    TEST_ASSERT_EQUAL_HEX32(0x444444, getAlertBorderColor(AlertLevel::OK, 0x444444));
    TEST_ASSERT_EQUAL_HEX32(0xF1C40F, getAlertBorderColor(AlertLevel::WARN, 0x444444));
    TEST_ASSERT_EQUAL_HEX32(0xFF2D2D, getAlertBorderColor(AlertLevel::CRIT, 0x444444));

    TEST_ASSERT_EQUAL_INT(2, getAlertBorderWidth(AlertLevel::OK, 2));
    TEST_ASSERT_EQUAL_INT(4, getAlertBorderWidth(AlertLevel::WARN, 2));
    TEST_ASSERT_EQUAL_INT(5, getAlertBorderWidth(AlertLevel::CRIT, 2));

    TEST_ASSERT_EQUAL_INT(0, autodimIndexToSeconds(0));
    TEST_ASSERT_EQUAL_INT(60, autodimIndexToSeconds(1));
    TEST_ASSERT_EQUAL_INT(120, autodimIndexToSeconds(2));
    TEST_ASSERT_EQUAL_INT(300, autodimIndexToSeconds(3));
    TEST_ASSERT_EQUAL_INT(600, autodimIndexToSeconds(4));
    TEST_ASSERT_EQUAL_INT(0, autodimIndexToSeconds(-1));
    TEST_ASSERT_EQUAL_INT(0, autodimIndexToSeconds(9));

    TEST_ASSERT_EQUAL_INT(0, autodimSecondsToIndex(0));
    TEST_ASSERT_EQUAL_INT(1, autodimSecondsToIndex(60));
    TEST_ASSERT_EQUAL_INT(2, autodimSecondsToIndex(120));
    TEST_ASSERT_EQUAL_INT(3, autodimSecondsToIndex(300));
    TEST_ASSERT_EQUAL_INT(4, autodimSecondsToIndex(600));
}

void test_autodim_controller_timeout_and_wake_absorption(void) {
    AutoDimController ctrl;
    ctrl.init(1000);
    TEST_ASSERT_FALSE(ctrl.isDimmed());

    // Disabled (timeout == 0) never dims
    TEST_ASSERT_FALSE(ctrl.checkTimeout(1000000u, 0));
    TEST_ASSERT_FALSE(ctrl.isDimmed());

    // Active touch resets timer and prevents premature dimming
    bool wake = false;
    TEST_ASSERT_TRUE(ctrl.onTouchSample(true, 50000u, wake));
    TEST_ASSERT_FALSE(wake);
    TEST_ASSERT_TRUE(ctrl.onTouchSample(false, 50100u, wake));
    TEST_ASSERT_FALSE(ctrl.checkTimeout(109000u, 60)); // 59s elapsed < 60s

    // Transition into dimmed at 60s
    TEST_ASSERT_TRUE(ctrl.checkTimeout(110000u, 60));
    TEST_ASSERT_TRUE(ctrl.isDimmed());
    // Subsequent check does not re-trigger
    TEST_ASSERT_FALSE(ctrl.checkTimeout(120000u, 60));

    // Initial wake press restores backlight and is absorbed
    TEST_ASSERT_FALSE(ctrl.onTouchSample(true, 125000u, wake));
    TEST_ASSERT_TRUE(wake);
    TEST_ASSERT_FALSE(ctrl.isDimmed());
    TEST_ASSERT_TRUE(ctrl.isAbsorbingWakeTouch());

    // Holding finger down continues to be absorbed without re-triggering wake
    TEST_ASSERT_FALSE(ctrl.onTouchSample(true, 125100u, wake));
    TEST_ASSERT_FALSE(wake);
    TEST_ASSERT_TRUE(ctrl.isAbsorbingWakeTouch());

    // Releasing finger ends absorption; subsequent touch is forwarded normally
    TEST_ASSERT_TRUE(ctrl.onTouchSample(false, 125200u, wake));
    TEST_ASSERT_FALSE(ctrl.isAbsorbingWakeTouch());
    TEST_ASSERT_TRUE(ctrl.onTouchSample(true, 126000u, wake));
    TEST_ASSERT_FALSE(wake);

    // uint32_t rollover timeout check
    ctrl.init(0xFFFFFC00u); // 1024ms before wrap
    TEST_ASSERT_TRUE(ctrl.checkTimeout(0x0000E700u, 60)); // 60160ms elapsed
    TEST_ASSERT_TRUE(ctrl.isDimmed());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_normalize_theme_id_defaults_to_primitive);
    RUN_TEST(test_theme_palettes_match_specification);
    RUN_TEST(test_hud_freshness_and_colors);
    RUN_TEST(test_rssi_to_signal_bars);
    RUN_TEST(test_scale_samples_to_chart);
    RUN_TEST(test_alert_visuals_and_autodim_mapping);
    RUN_TEST(test_autodim_controller_timeout_and_wake_absorption);
    return UNITY_END();
}
