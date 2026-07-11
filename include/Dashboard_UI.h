#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <Arduino.h>
#include "lvgl.h"
#include "Remote_Metrics.h"

namespace DashboardUI {
    extern lv_obj_t *touch_cursor;

    // Initialize the dashboard UI elements
    void init(uint16_t tft_width, uint16_t tft_height);

    // Show the WiFi settings overlay
    void showSettingsScreen();

    // Update the UI card values thread-safely
    void update(const MetricsData &data);

    // Set the state of the fetch error indicator
    void setFetchError(bool error);
}

#endif // DASHBOARD_UI_H
