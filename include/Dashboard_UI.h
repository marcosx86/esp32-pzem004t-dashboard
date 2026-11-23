#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <Arduino.h>
#include "lvgl.h"
#include "Remote_Metrics.h"
#include "Network_Manager.h"
#include "UI_Logic.h"

namespace DashboardUI {
    typedef void (*TouchResetCallback)();

    extern lv_obj_t *touch_cursor;

    // Initialize the dashboard UI elements
    void init(uint16_t tft_width, uint16_t tft_height);

    // Register callback invoked when user selects "Reset TS Calib"
    void setTouchResetCallback(TouchResetCallback cb);

    // Query live red touch dot visibility setting
    bool isTouchCursorEnabled();

    // Active backlight brightness and Auto-Dim settings for main loop / wake restore
    int getActiveBrightness();
    int getAutoDimTimeoutSec();
    int getAutoDimLevel();

    // Show the WiFi/Appearance/Touch/Alerts settings overlay
    void showSettingsScreen();

    // Update dashboard card, alert borders, session cost, and ambient telemetry values
    void update(const MetricsData &data);

    // Update top-left floating HUD freshness dot & Wi-Fi bars from Core-0 diagnostics
    void updateDiagnostics(const NetworkManager::NetworkDiagnostics &diag);

    // Legacy compatibility no-op (top-left HUD status pill replaces center warning overlay)
    void setFetchError(bool error);
}

#endif // DASHBOARD_UI_H
