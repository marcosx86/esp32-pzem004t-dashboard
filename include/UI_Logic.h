#ifndef UI_LOGIC_H
#define UI_LOGIC_H

#include "Metrics_Stats.h"
#include <cstddef>
#include <cstdint>

enum class UiThemeId {
    PRIMITIVE = 0,
    DARK_HIGH_CONTRAST = 1,
    DAYLIGHT_PAPER = 2,
    NEON_OSCILLOSCOPE = 3,
    AMBER_NIGHT = 4
};

static constexpr int kUiThemeCount = 5;

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
    uint32_t chart_bg;
    uint32_t chart_line[4];
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
 * @brief Clamps/normalizes an arbitrary persisted theme ID to 0..(kUiThemeCount - 1), defaulting to PRIMITIVE (0).
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

/**
 * @brief Scales up to @p max_len recent finite samples into [y_min, y_max] with 5% padding.
 *
 * Constant sequences map to the vertical midpoint (y_min + y_max) / 2.
 * Non-finite entries are skipped cleanly.
 *
 * @return Number of scaled points written into @p dst.
 */
size_t scaleSamplesToChart(const double *src,
                           size_t count,
                           int16_t *dst,
                           size_t max_len,
                           int16_t y_min = 0,
                           int16_t y_max = 1000);

/**
 * @brief Maps an AlertLevel to its border color override (or @p normal_border_color when OK).
 */
uint32_t getAlertBorderColor(AlertLevel level, uint32_t normal_border_color);

/**
 * @brief Maps an AlertLevel to its border width (or @p normal_width when OK).
 */
int getAlertBorderWidth(AlertLevel level, int normal_width);

/**
 * @brief Converts Auto-Dim dropdown option index (0..4) to timeout in seconds.
 */
int autodimIndexToSeconds(int dropdown_index);

/**
 * @brief Converts Auto-Dim timeout in seconds to dropdown option index (0..4).
 */
int autodimSecondsToIndex(int seconds);

/**
 * @brief Pure state machine managing inactivity auto-dim and wake-touch absorption.
 */
class AutoDimController {
public:
    AutoDimController();

    void init(uint32_t now_ms);
    void resetActivity(uint32_t now_ms);
    bool isDimmed() const;
    bool isAbsorbingWakeTouch() const;

    /**
     * @brief Processes a raw touch sample.
     *
     * @param touched Current hardware touch press state.
     * @param now_ms Current monotonic millisecond timestamp.
     * @param out_wake_triggered Set true on the single press that wakes the display.
     * @return true if touch state should be forwarded to LVGL; false while absorbing wake press.
     */
    bool onTouchSample(bool touched, uint32_t now_ms, bool &out_wake_triggered);

    /**
     * @brief Checks inactivity timeout; returns true once upon entering dimmed state.
     */
    bool checkTimeout(uint32_t now_ms, int timeout_sec);

private:
    bool dimmed_;
    bool absorbing_wake_touch_;
    uint32_t last_activity_ms_;
};

#endif /* UI_LOGIC_H */
