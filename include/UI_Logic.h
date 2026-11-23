#ifndef UI_LOGIC_H
#define UI_LOGIC_H

#include "Metrics_Stats.h"
#include <cstdint>

enum class UiThemeId {
    PRIMITIVE = 0,
    DARK_HIGH_CONTRAST = 1
};

struct UiThemePalette {
    uint32_t screen_bg;
    uint32_t card_bg[4];
    uint32_t card_border[4];
    uint32_t card_title[4];
    uint32_t card_value;
    uint32_t card_sub;
    int card_border_width;
    uint32_t hud_pill_bg;
    uint32_t hud_pill_text;
    uint32_t settings_bg;
    uint32_t settings_text;
    uint32_t editor_bg;
    uint32_t list_border;
};

enum class HudFreshness {
    LIVE = 0,
    STALE = 1,
    ERROR = 2
};

enum class InputFieldKind {
    TEXT = 0,
    NUMERIC_IP = 1
};

/**
 * @brief Clamps/normalizes an arbitrary persisted theme ID to PRIMITIVE (0) or DARK_HIGH_CONTRAST (1).
 */
int normalizeThemeId(int theme_id);

/**
 * @brief Returns the static color palette for the requested (normalized) theme ID.
 */
const UiThemePalette& getThemePalette(int theme_id);

/**
 * @brief Computes top-left HUD freshness state from Wi-Fi link, fetch outcome, and sample age.
 */
HudFreshness computeHudFreshness(bool wifi_connected,
                                   bool has_valid_metrics,
                                   bool fetch_failed,
                                   uint32_t last_success_ms,
                                   uint32_t now_ms,
                                   uint32_t stale_ms = kMetricsStaleMs);

/**
 * @brief Maps HudFreshness to its indicator dot hex color.
 */
uint32_t getFreshnessDotColor(HudFreshness freshness);

/**
 * @brief Maps Wi-Fi link state and RSSI (dBm) to 0..4 signal bars.
 */
int rssiToSignalBars(bool wifi_connected, int32_t rssi_dbm);

#endif /* UI_LOGIC_H */
