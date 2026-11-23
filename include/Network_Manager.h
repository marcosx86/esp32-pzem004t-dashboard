#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "Remote_Metrics.h"
#include "Metrics_Stats.h"
#include "Network_Store.h"
#include <freertos/semphr.h>
#include <vector>

namespace NetworkManager {
    struct WifiCredential {
        String ssid;
        String password;
    };

    enum class ConnectState {
        IDLE = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        FAILED = 3
    };

    struct WifiScanEntry {
        String ssid;
        int32_t rssi;
        bool encrypted;
    };

    struct NetworkDiagnostics {
        bool wifi_connected;
        int32_t rssi_dbm;
        String ssid;
        String ip;
        bool has_valid_metrics;
        bool fetch_failed;
        int last_http_code;
        uint32_t last_success_ms;
        ConnectState connect_state;
    };

    struct AppSettings {
        int theme = 0;                /* 0 = Primitive, 1 = Dark High Contrast */
        bool touch_cursor = true;     /* Red touch dot default ON */
        int autodim_sec = 0;          /* 0 = Off */
        int autodim_level = 15;       /* Dimmed PWM brightness */
        AlertThresholds alerts;       /* Default <95V, >127V, 10A dynamic */
        float tariff = 0.0f;          /* Cost per kWh */
        double session_kwh_base = NAN;
    };

    // Start Core-0 background task and queue initial connection to (ssid, pass) non-blockingly.
    void begin(const char* ssid, const char* pass, const char* metrics_url);

    // Load NVS config, spawn Core-0 network/metrics task, and return immediately without blocking UI init.
    void beginWithStoredNetworks();

    // Load/save WiFi credentials stored as plain text in NVRAM
    std::vector<WifiCredential> loadSavedNetworks();
    bool saveNetworks(const std::vector<WifiCredential>& networks);
    bool addOrUpdateNetwork(const String& ssid, const String& password, int index = -1);
    bool deleteNetwork(int index);

    // Legacy synchronous helper (routes safely through Core 0 and waits for completion)
    bool connectToNetwork(const String& ssid, const String& password);

    // Non-blocking Core-0 Wi-Fi connect controls
    void requestConnect(const String& ssid, const String& password);
    ConnectState getConnectState();
    void clearConnectState();

    // Asynchronous Wi-Fi scan API
    void startWifiScan();
    int getWifiScanStatus();
    std::vector<WifiScanEntry> getWifiScanResults();

    // Thread-safely get the latest fetched metrics data (returns false before first valid fetch)
    bool getLatestMetrics(MetricsData &out);
    bool getMetricsHistory(MetricsHistory &out);
    NetworkDiagnostics getDiagnostics();

    // Get and set metrics config (IP and Path)
    String getMetricsIP();
    String getMetricsPath();
    void setMetricsConfig(const String& ip, const String& path);

    // Application settings & session baseline NVS persistence
    AppSettings loadAppSettings();
    void saveAppSettings(const AppSettings &settings);
    void resetSessionEnergyBaseline();

    // Check if the background fetch task failed recently
    bool isFetchFailed();
}

#endif // NETWORK_MANAGER_H
