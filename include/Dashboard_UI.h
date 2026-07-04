#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <Arduino.h>
#include "lvgl.h"
#include "Remote_Metrics.h"

namespace DashboardUI {
    extern lv_obj_t *touch_cursor;
    
    // Initialize the dashboard UI elements
    void init(uint16_t tft_width, uint16_t tft_height);
    
    // Update the UI card values thread-safely
    void update(const MetricsData &data);
}

#endif // DASHBOARD_UI_H
