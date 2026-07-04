#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "Remote_Metrics.h"
#include <freertos/semphr.h>

namespace NetworkManager {
    // Start WiFi auto-reconnect and initialize background metrics task on Core 0
    void begin(const char* ssid, const char* pass, const char* metrics_url);
    
    // Thread-safely get the latest fetched metrics data
    bool getLatestMetrics(MetricsData &out);
}

#endif // NETWORK_MANAGER_H
