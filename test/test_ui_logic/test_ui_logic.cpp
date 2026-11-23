#include <unity.h>
#include "UI_Logic.h"

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

    // Out-of-range fallback returns Primitive
    TEST_ASSERT_EQUAL_HEX32(0x63A986, getThemePalette(42).settings_bg);
}

void test_hud_freshness_and_colors(void) {
    // Disconnected Wi-Fi -> ERROR
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(false, true, false, 1000, 1200, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0xE74C3C, getFreshnessDotColor(HudFreshness::ERROR));

    // Connected, awaiting first sample, no error -> STALE (amber)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::STALE),
                          static_cast<int>(computeHudFreshness(true, false, false, 0, 1200, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0xF39C12, getFreshnessDotColor(HudFreshness::STALE));

    // Connected, no valid sample yet, fetch failed -> ERROR
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(true, false, true, 0, 1200, 5000)));

    // Connected, fresh sample (<= 5000ms), no error -> LIVE (green)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::LIVE),
                          static_cast<int>(computeHudFreshness(true, true, false, 1000, 6000, 5000)));
    TEST_ASSERT_EQUAL_HEX32(0x2ECC71, getFreshnessDotColor(HudFreshness::LIVE));

    // Connected, fresh sample, transient single failure -> STALE (amber)
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::STALE),
                          static_cast<int>(computeHudFreshness(true, true, true, 1000, 3000, 5000)));

    // Connected, stale sample (> 5000ms), fetch failed -> ERROR
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HudFreshness::ERROR),
                          static_cast<int>(computeHudFreshness(true, true, true, 1000, 6001, 5000)));

    // Connected, rollover-safe fresh timestamp -> LIVE
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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_normalize_theme_id_defaults_to_primitive);
    RUN_TEST(test_theme_palettes_match_specification);
    RUN_TEST(test_hud_freshness_and_colors);
    RUN_TEST(test_rssi_to_signal_bars);
    return UNITY_END();
}
