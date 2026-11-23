#include "UI_Logic.h"

namespace {

const UiThemePalette kPrimitivePalette = {
    0xFFFFFF,
    {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12},
    {0x444444, 0x444444, 0x444444, 0x444444},
    {0x1A1A1A, 0x1A1A1A, 0x1A1A1A, 0x1A1A1A},
    0x111111,
    0x222222,
    2,
    0x222A35,
    0xFFFFFF,
    0x63A986,
    0x111111,
    0xFFB19A,
    0x555555
};

const UiThemePalette kDarkHighContrastPalette = {
    0x12161C,
    {0x1D2430, 0x1D2430, 0x1D2430, 0x1D2430},
    {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12},
    {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12},
    0xFFFFFF,
    0xA9B4C2,
    3,
    0x0D1117,
    0xFFFFFF,
    0x18202A,
    0xF0F4F8,
    0x243040,
    0x3A4A5E
};

} // namespace

int normalizeThemeId(int theme_id) {
    return (theme_id == static_cast<int>(UiThemeId::DARK_HIGH_CONTRAST)) ? 1 : 0;
}

const UiThemePalette& getThemePalette(int theme_id) {
    if (normalizeThemeId(theme_id) == 1) {
        return kDarkHighContrastPalette;
    }
    return kPrimitivePalette;
}

HudFreshness computeHudFreshness(bool wifi_connected,
                                   bool has_valid_metrics,
                                   bool fetch_failed,
                                   uint32_t last_success_ms,
                                   uint32_t now_ms,
                                   uint32_t stale_ms) {
    if (!wifi_connected) {
        return HudFreshness::ERROR;
    }
    if (fetch_failed && !has_valid_metrics) {
        return HudFreshness::ERROR;
    }
    if (isMetricsStale(has_valid_metrics, last_success_ms, now_ms, stale_ms)) {
        return fetch_failed ? HudFreshness::ERROR : HudFreshness::STALE;
    }
    if (fetch_failed) {
        return HudFreshness::STALE;
    }
    return HudFreshness::LIVE;
}

uint32_t getFreshnessDotColor(HudFreshness freshness) {
    switch (freshness) {
        case HudFreshness::LIVE:
            return 0x2ECC71;
        case HudFreshness::STALE:
            return 0xF39C12;
        case HudFreshness::ERROR:
        default:
            return 0xE74C3C;
    }
}

int rssiToSignalBars(bool wifi_connected, int32_t rssi_dbm) {
    if (!wifi_connected) {
        return 0;
    }
    if (rssi_dbm < -80) {
        return 1;
    }
    if (rssi_dbm < -67) {
        return 2;
    }
    if (rssi_dbm < -55) {
        return 3;
    }
    return 4;
}
