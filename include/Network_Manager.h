#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "Remote_Metrics.h"
#include <freertos/semphr.h>
#include <vector>

namespace NetworkManager {
    struct WifiCredential {
        String ssid;
        String password;
    };

    // Start WiFi auto-reconnect and initialize background metrics task on Core 0
    void begin(const char* ssid, const char* pass, const char* metrics_url);

    // Load saved credentials from NVRAM and connect to the first entry, falling back to defaults
    void beginWithStoredNetworks();

    // Load/save WiFi credentials stored as plain text in NVRAM
    std::vector<WifiCredential> loadSavedNetworks();
    bool saveNetworks(const std::vector<WifiCredential>& networks);
    bool addOrUpdateNetwork(const String& ssid, const String& password, int index = -1);
    bool deleteNetwork(int index);
    bool connectToNetwork(const String& ssid, const String& password);

    // Thread-safely get the latest fetched metrics data
    bool getLatestMetrics(MetricsData &out);

    // Get and set metrics config (IP and Path)
    String getMetricsIP();
    String getMetricsPath();
    void setMetricsConfig(const String& ip, const String& path);

    // Check if the background fetch task failed recently
    bool isFetchFailed();
}

#endif // NETWORK_MANAGER_H
