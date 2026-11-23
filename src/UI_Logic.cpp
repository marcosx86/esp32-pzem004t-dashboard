#include "UI_Logic.h"

#include <cmath>

namespace {

const UiThemePalette kThemePalettes[kUiThemeCount] = {
    // 0: Primitive
    {
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
        0x555555,
        0x1A1A1A,
        {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF}
    },
    // 1: Dark High Contrast
    {
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
        0x3A4A5E,
        0x0D1117,
        {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12}
    },
    // 2: Daylight Paper
    {
        0xEEF2F6,
        {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF},
        {0x2563EB, 0xDC2626, 0x16A34A, 0xD97706},
        {0x2563EB, 0xDC2626, 0x16A34A, 0xD97706},
        0x0F172A,
        0x475569,
        3,
        0x1E293B,
        0xFFFFFF,
        0xE2E8F0,
        0x0F172A,
        0xF8FAFC,
        0x94A3B8,
        0xF1F5F9,
        {0x2563EB, 0xDC2626, 0x16A34A, 0xD97706}
    },
    // 3: Neon Oscilloscope
    {
        0x090B10,
        {0x121726, 0x121726, 0x121726, 0x121726},
        {0x00E5FF, 0xFF2E97, 0x39FF14, 0xFFB800},
        {0x00E5FF, 0xFF2E97, 0x39FF14, 0xFFB800},
        0xF5FAFF,
        0x8FA8C8,
        3,
        0x0F1524,
        0xFFFFFF,
        0x0E1320,
        0xE6F1FF,
        0x182238,
        0x2B3A55,
        0x070A12,
        {0x00E5FF, 0xFF2E97, 0x39FF14, 0xFFB800}
    },
    // 4: Amber Night
    {
        0x0B0804,
        {0x1A1208, 0x1A1208, 0x1A1208, 0x1A1208},
        {0xFFB000, 0xFF6B00, 0xD4A017, 0xFF9500},
        {0xFFB000, 0xFF6B00, 0xD4A017, 0xFF9500},
        0xFFD166,
        0xB88A44,
        2,
        0x140E06,
        0xFFD166,
        0x160F06,
        0xFFD166,
        0x24180A,
        0x5C3D12,
        0x0F0A04,
        {0xFFB000, 0xFF6B00, 0xD4A017, 0xFF9500}
    }
};

const int kAutoDimSecondsTable[] = {0, 60, 120, 300, 600};

} // namespace

int normalizeThemeId(int theme_id) {
    if (theme_id >= 0 && theme_id < kUiThemeCount) {
        return theme_id;
    }
    return 0;
}

const UiThemePalette& getThemePalette(int theme_id) {
    return kThemePalettes[normalizeThemeId(theme_id)];
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

size_t scaleSamplesToChart(const double *src,
                           size_t count,
                           int16_t *dst,
                           size_t max_len,
                           int16_t y_min,
                           int16_t y_max) {
    if (!src || !dst || count == 0 || max_len == 0 || y_max < y_min) {
        return 0;
    }

    size_t start_idx = (count > max_len) ? (count - max_len) : 0;
    bool has_valid = false;
    double min_v = 0.0;
    double max_v = 0.0;

    for (size_t i = start_idx; i < count; ++i) {
        double v = src[i];
        if (!std::isfinite(v)) {
            continue;
        }
        if (!has_valid) {
            min_v = v;
            max_v = v;
            has_valid = true;
        } else {
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }

    if (!has_valid) {
        return 0;
    }

    double range = max_v - min_v;
    int16_t mid = static_cast<int16_t>((static_cast<int32_t>(y_min) + static_cast<int32_t>(y_max)) / 2);
    double pad = range * 0.05;
    double lo = min_v - pad;
    double hi = max_v + pad;
    double span = hi - lo;

    size_t out_cnt = 0;
    for (size_t i = start_idx; i < count; ++i) {
        double v = src[i];
        if (!std::isfinite(v)) {
            continue;
        }
        if (range < 1e-9 || span <= 0.0) {
            dst[out_cnt++] = mid;
        } else {
            double norm = (v - lo) / span;
            double scaled = static_cast<double>(y_min) + norm * static_cast<double>(y_max - y_min);
            long rounded = std::lround(scaled);
            if (rounded < y_min) rounded = y_min;
            if (rounded > y_max) rounded = y_max;
            dst[out_cnt++] = static_cast<int16_t>(rounded);
        }
    }

    return out_cnt;
}

uint32_t getAlertBorderColor(AlertLevel level, uint32_t normal_border_color) {
    switch (level) {
        case AlertLevel::WARN:
            return 0xF1C40F;
        case AlertLevel::CRIT:
            return 0xFF2D2D;
        case AlertLevel::OK:
        default:
            return normal_border_color;
    }
}

int getAlertBorderWidth(AlertLevel level, int normal_width) {
    switch (level) {
        case AlertLevel::WARN:
            return 4;
        case AlertLevel::CRIT:
            return 5;
        case AlertLevel::OK:
        default:
            return normal_width;
    }
}

int autodimIndexToSeconds(int dropdown_index) {
    if (dropdown_index < 0 ||
        dropdown_index >= static_cast<int>(sizeof(kAutoDimSecondsTable) / sizeof(kAutoDimSecondsTable[0]))) {
        return 0;
    }
    return kAutoDimSecondsTable[dropdown_index];
}

int autodimSecondsToIndex(int seconds) {
    if (seconds <= 0) return 0;
    if (seconds <= 60) return 1;
    if (seconds <= 120) return 2;
    if (seconds <= 300) return 3;
    return 4;
}

AutoDimController::AutoDimController()
    : dimmed_(false), absorbing_wake_touch_(false), last_activity_ms_(0) {}

void AutoDimController::init(uint32_t now_ms) {
    dimmed_ = false;
    absorbing_wake_touch_ = false;
    last_activity_ms_ = now_ms;
}

void AutoDimController::resetActivity(uint32_t now_ms) {
    last_activity_ms_ = now_ms;
}

bool AutoDimController::isDimmed() const {
    return dimmed_;
}

bool AutoDimController::isAbsorbingWakeTouch() const {
    return absorbing_wake_touch_;
}

bool AutoDimController::onTouchSample(bool touched, uint32_t now_ms, bool &out_wake_triggered) {
    out_wake_triggered = false;

    if (absorbing_wake_touch_) {
        last_activity_ms_ = now_ms;
        if (touched) {
            return false;
        }
        absorbing_wake_touch_ = false;
        return true;
    }

    if (dimmed_) {
        if (touched) {
            dimmed_ = false;
            absorbing_wake_touch_ = true;
            last_activity_ms_ = now_ms;
            out_wake_triggered = true;
            return false;
        }
        return true;
    }

    if (touched) {
        last_activity_ms_ = now_ms;
    }
    return true;
}

bool AutoDimController::checkTimeout(uint32_t now_ms, int timeout_sec) {
    if (timeout_sec <= 0 || dimmed_ || absorbing_wake_touch_) {
        return false;
    }

    uint32_t timeout_ms = static_cast<uint32_t>(timeout_sec) * 1000u;
    if (static_cast<uint32_t>(now_ms - last_activity_ms_) >= timeout_ms) {
        dimmed_ = true;
        return true;
    }
    return false;
}
