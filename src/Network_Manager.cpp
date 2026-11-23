#include "Network_Manager.h"
#include <Preferences.h>
#include <WiFi.h>
#include <algorithm>
#include <cmath>

namespace NetworkManager {
    static MetricsData shared_metrics = makeEmptyMetrics();
    static MetricsHistory shared_history;
    static SemaphoreHandle_t metrics_mutex = NULL;

    static String cached_ip = "192.168.1.10";
    static String cached_path = "/metrics";
    static bool init_done = false;
    static bool has_valid_metrics = false;
    static bool fetch_failed = false;
    static int last_http_code = 0;
    static uint32_t last_success_ms = 0;

    enum class ScanState {
        IDLE = 0,
        SCANNING = 1,
        DONE = 2,
        FAILED = 3
    };

    static bool boot_connect_pending = false;
    static bool pending_connect = false;
    static String pending_ssid;
    static String pending_pass;
    static ConnectState connect_state = ConnectState::IDLE;

    static bool scan_requested = false;
    static ScanState scan_state = ScanState::IDLE;
    static std::vector<WifiScanEntry> cached_scan_results;

    static AppSettings cached_settings;

    static const char* kWifiNamespace = "wifi_settings";
    static const char* kWifiKey = "networks";
    static const char* kSettingsNamespace = "settings";

    static std::vector<StoredWifiCredential> toStoredList(const std::vector<WifiCredential>& networks) {
        std::vector<StoredWifiCredential> out;
        out.reserve(networks.size());
        for (const auto& net : networks) {
            out.push_back(StoredWifiCredential{net.ssid.c_str(), net.password.c_str()});
        }
        return out;
    }

    static std::vector<WifiCredential> toArduinoList(const std::vector<StoredWifiCredential>& networks) {
        std::vector<WifiCredential> out;
        out.reserve(networks.size());
        for (const auto& net : networks) {
            out.push_back(WifiCredential{String(net.ssid.c_str()), String(net.password.c_str())});
        }
        return out;
    }

    static bool hasPendingConnectRequest() {
        bool result = false;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            result = pending_connect;
            xSemaphoreGive(metrics_mutex);
        } else {
            result = pending_connect;
        }
        return result;
    }

    static bool hasPendingScanRequest() {
        bool result = false;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            result = scan_requested;
            xSemaphoreGive(metrics_mutex);
        } else {
            result = scan_requested;
        }
        return result;
    }

    static bool connectToWifi(const char* ssid, const char* pass, bool abort_on_pending, bool abort_on_scan) {
        Serial.printf("Connecting to WiFi '%s'", ssid);
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        WiFi.setAutoReconnect(true);
        WiFi.begin(ssid, pass);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
            if ((abort_on_pending && hasPendingConnectRequest()) ||
                (abort_on_scan && hasPendingScanRequest())) {
                Serial.println(" aborted");
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            Serial.print('.');
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(" connected");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
            return true;
        }

        Serial.println(" failed to connect");
        return false;
    }

    static void network_task(void *pvParameters) {
        (void)pvParameters;
        uint32_t last_poll_ms = 0;
        bool polled_once = false;

        while (1) {
            // 1. Prioritize explicit connect requests from Core 1
            bool run_explicit = false;
            String req_ssid;
            String req_pass;
            if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (pending_connect) {
                    run_explicit = true;
                    req_ssid = pending_ssid;
                    req_pass = pending_pass;
                    pending_connect = false;
                    boot_connect_pending = false;
                    connect_state = ConnectState::CONNECTING;
                }
                xSemaphoreGive(metrics_mutex);
            }

            if (run_explicit) {
                bool ok = connectToWifi(req_ssid.c_str(), req_pass.c_str(), true, false);
                if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    if (!pending_connect) {
                        connect_state = ok ? ConnectState::CONNECTED : ConnectState::FAILED;
                    }
                    xSemaphoreGive(metrics_mutex);
                }
                polled_once = false;
                continue;
            }

            // 2. Execute Wi-Fi scan exclusively on Core 0
            bool run_scan = false;
            if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (scan_requested) {
                    run_scan = true;
                    scan_requested = false;
                    scan_state = ScanState::SCANNING;
                }
                xSemaphoreGive(metrics_mutex);
            }

            if (run_scan) {
                WiFi.mode(WIFI_STA);
                bool was_connected = (WiFi.status() == WL_CONNECTED);
                if (!was_connected) {
                    WiFi.setAutoReconnect(false);
                    WiFi.disconnect(false);
                    vTaskDelay(pdMS_TO_TICKS(150));
                }

                WiFi.scanDelete();
                int16_t raw_count = WiFi.scanNetworks(false, false, false, 200);
                if (raw_count == WIFI_SCAN_FAILED) {
                    Serial.println("[WiFiScan] Initial scan failed (-2); retrying...");
                    vTaskDelay(pdMS_TO_TICKS(250));
                    raw_count = WiFi.scanNetworks(false, false, false, 200);
                }

                std::vector<WifiScanEntry> entries;
                if (raw_count >= 0) {
                    std::vector<ScannedWifiInfo> raw_list;
                    raw_list.reserve(static_cast<size_t>(raw_count));
                    for (int16_t i = 0; i < raw_count; ++i) {
                        raw_list.push_back(ScannedWifiInfo{
                            WiFi.SSID(i).c_str(),
                            WiFi.RSSI(i),
                            WiFi.encryptionType(i) != WIFI_AUTH_OPEN
                        });
                    }
                    WiFi.scanDelete();

                    std::vector<ScannedWifiInfo> normalized = normalizeScanResults(raw_list);
                    entries.reserve(normalized.size());
                    for (const auto& item : normalized) {
                        entries.push_back(WifiScanEntry{
                            String(item.ssid.c_str()),
                            item.rssi,
                            item.encrypted
                        });
                    }
                } else {
                    WiFi.scanDelete();
                }

                if (!was_connected) {
                    WiFi.setAutoReconnect(true);
                }

                Serial.printf("[WiFiScan] raw=%d unique=%u\n", raw_count, static_cast<unsigned>(entries.size()));

                if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    cached_scan_results = std::move(entries);
                    scan_state = (raw_count >= 0) ? ScanState::DONE : ScanState::FAILED;
                    xSemaphoreGive(metrics_mutex);
                } else {
                    cached_scan_results = std::move(entries);
                    scan_state = (raw_count >= 0) ? ScanState::DONE : ScanState::FAILED;
                }
                continue;
            }

            // 3. Sequential boot auto-connect cycling across saved networks
            bool run_boot = false;
            if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                run_boot = boot_connect_pending;
                boot_connect_pending = false;
                xSemaphoreGive(metrics_mutex);
            }

            if (run_boot) {
                std::vector<WifiCredential> networks = loadSavedNetworks();
                bool connected = false;
                for (const auto& net : networks) {
                    if (hasPendingConnectRequest() || hasPendingScanRequest()) {
                        break;
                    }
                    if (connectToWifi(net.ssid.c_str(), net.password.c_str(), true, true)) {
                        connected = true;
                        break;
                    }
                }
                if (!connected && !hasPendingConnectRequest() && !hasPendingScanRequest()) {
                    Serial.println("All stored networks failed.");
                }
                polled_once = false;
                continue;
            }

            // 3. Metrics polling at 1000 ms cadence
            uint32_t now = millis();
            if (!polled_once || static_cast<uint32_t>(now - last_poll_ms) >= 1000) {
                last_poll_ms = now;
                polled_once = true;

                if (WiFi.status() == WL_CONNECTED) {
                    String ip_copy;
                    String path_copy;
                    if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        ip_copy = cached_ip;
                        path_copy = cached_path;
                        xSemaphoreGive(metrics_mutex);
                    } else {
                        ip_copy = cached_ip;
                        path_copy = cached_path;
                    }

                    std::string url = buildMetricsUrl(ip_copy.c_str(), path_copy.c_str());
                    MetricsData temp_m;
                    int http_code = 0;
                    unsigned long start_time = millis();
                    bool success = fetch_metrics(url.c_str(), temp_m, &http_code);
                    unsigned long duration = millis() - start_time;

                    Serial.printf("[Metrics] Fetch took %lu ms (status: %s, code: %d)\n",
                                  duration, success ? "success" : "failed", http_code);

                    bool save_initial_base = false;
                    double initial_base_val = NAN;

                    if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        fetch_failed = !success;
                        last_http_code = http_code;
                        if (success) {
                            shared_metrics = temp_m;
                            has_valid_metrics = true;
                            last_success_ms = temp_m.timestamp;
                            shared_history.push(temp_m);

                            if (!std::isfinite(cached_settings.session_kwh_base) && std::isfinite(temp_m.energy)) {
                                cached_settings.session_kwh_base = temp_m.energy;
                                initial_base_val = temp_m.energy;
                                save_initial_base = true;
                            }
                        }
                        xSemaphoreGive(metrics_mutex);
                    }

                    if (save_initial_base) {
                        Preferences prefs;
                        if (prefs.begin(kSettingsNamespace, false)) {
                            prefs.putDouble("session_base", initial_base_val);
                            prefs.end();
                        }
                    }
                } else {
                    if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        fetch_failed = true;
                        last_http_code = -1;
                        xSemaphoreGive(metrics_mutex);
                    } else {
                        fetch_failed = true;
                        last_http_code = -1;
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    static void ensureTaskStarted(bool schedule_boot_connect) {
        if (init_done) {
            return;
        }

        WiFi.mode(WIFI_STA);
        cached_ip = getMetricsIP();
        cached_path = getMetricsPath();
        cached_settings = loadAppSettings();
        shared_metrics = makeEmptyMetrics();
        shared_history.clear();
        has_valid_metrics = false;
        fetch_failed = false;
        last_http_code = 0;
        last_success_ms = 0;
        boot_connect_pending = schedule_boot_connect;

        metrics_mutex = xSemaphoreCreateMutex();
        if (metrics_mutex != NULL) {
            xTaskCreatePinnedToCore(
                network_task,
                "network_task",
                6144,
                NULL,
                1,
                NULL,
                0
            );
            Serial.println("Background network/metrics task created on Core 0.");
        }
        init_done = true;
    }

    void begin(const char* ssid, const char* pass, const char* metrics_url) {
        (void)metrics_url;
        ensureTaskStarted(false);
        if (ssid && ssid[0] != '\0') {
            requestConnect(String(ssid), String(pass ? pass : ""));
        }
    }

    void beginWithStoredNetworks() {
        ensureTaskStarted(true);
    }

    std::vector<WifiCredential> loadSavedNetworks() {
        Preferences prefs;
        if (!prefs.begin(kWifiNamespace, true)) {
            return {};
        }

        String content = prefs.getString(kWifiKey, "");
        prefs.end();
        return toArduinoList(parseWifiCredentials(content.c_str()));
    }

    bool saveNetworks(const std::vector<WifiCredential>& networks) {
        Preferences prefs;
        if (!prefs.begin(kWifiNamespace, false)) {
            return false;
        }

        std::string serialized = serializeWifiCredentials(toStoredList(networks));
        prefs.putString(kWifiKey, serialized.c_str());
        prefs.end();
        return true;
    }

    bool addOrUpdateNetwork(const String& ssid, const String& password, int index) {
        std::vector<StoredWifiCredential> stored = toStoredList(loadSavedNetworks());
        if (!addOrUpdateWifiCredential(stored, ssid.c_str(), password.c_str(), index)) {
            return false;
        }
        return saveNetworks(toArduinoList(stored));
    }

    bool deleteNetwork(int index) {
        std::vector<StoredWifiCredential> stored = toStoredList(loadSavedNetworks());
        if (!deleteWifiCredential(stored, index)) {
            return false;
        }
        return saveNetworks(toArduinoList(stored));
    }

    void requestConnect(const String& ssid, const String& password) {
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            pending_ssid = ssid;
            pending_pass = password;
            pending_connect = true;
            connect_state = ConnectState::CONNECTING;
            xSemaphoreGive(metrics_mutex);
        } else {
            pending_ssid = ssid;
            pending_pass = password;
            pending_connect = true;
            connect_state = ConnectState::CONNECTING;
        }
    }

    ConnectState getConnectState() {
        ConnectState state = ConnectState::IDLE;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            state = connect_state;
            xSemaphoreGive(metrics_mutex);
        } else {
            state = connect_state;
        }
        return state;
    }

    void clearConnectState() {
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            connect_state = ConnectState::IDLE;
            xSemaphoreGive(metrics_mutex);
        } else {
            connect_state = ConnectState::IDLE;
        }
    }

    bool connectToNetwork(const String& ssid, const String& password) {
        if (!init_done) {
            return connectToWifi(ssid.c_str(), password.c_str(), false, false);
        }

        requestConnect(ssid, password);
        unsigned long wait_start = millis();
        while (millis() - wait_start < 12000) {
            ConnectState state = getConnectState();
            if (state == ConnectState::CONNECTED) {
                return true;
            }
            if (state == ConnectState::FAILED) {
                return false;
            }
            delay(50);
        }
        return false;
    }

    void startWifiScan() {
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            cached_scan_results.clear();
            scan_state = ScanState::SCANNING;
            scan_requested = true;
            xSemaphoreGive(metrics_mutex);
        } else {
            cached_scan_results.clear();
            scan_state = ScanState::SCANNING;
            scan_requested = true;
        }
    }

    int getWifiScanStatus() {
        int status = -2;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (scan_state == ScanState::SCANNING) {
                status = -1;
            } else if (scan_state == ScanState::DONE) {
                status = static_cast<int>(cached_scan_results.size());
            } else {
                status = -2;
            }
            xSemaphoreGive(metrics_mutex);
        } else {
            if (scan_state == ScanState::SCANNING) {
                status = -1;
            } else if (scan_state == ScanState::DONE) {
                status = static_cast<int>(cached_scan_results.size());
            }
        }
        return status;
    }

    std::vector<WifiScanEntry> getWifiScanResults() {
        std::vector<WifiScanEntry> results;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (scan_state == ScanState::DONE) {
                results = cached_scan_results;
            }
            xSemaphoreGive(metrics_mutex);
        } else if (scan_state == ScanState::DONE) {
            results = cached_scan_results;
        }
        return results;
    }

    bool getLatestMetrics(MetricsData &out) {
        if (!init_done || metrics_mutex == NULL) {
            return false;
        }

        bool valid = false;
        if (xSemaphoreTake(metrics_mutex, 0) == pdTRUE) {
            if (has_valid_metrics) {
                out = shared_metrics;
                valid = true;
            }
            xSemaphoreGive(metrics_mutex);
        }
        return valid;
    }

    bool getMetricsHistory(MetricsHistory &out) {
        if (!init_done || metrics_mutex == NULL) {
            return false;
        }

        if (xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            out = shared_history;
            xSemaphoreGive(metrics_mutex);
            return true;
        }
        return false;
    }

    NetworkDiagnostics getDiagnostics() {
        NetworkDiagnostics diag;
        diag.wifi_connected = (WiFi.status() == WL_CONNECTED);
        diag.rssi_dbm = diag.wifi_connected ? WiFi.RSSI() : 0;
        diag.ssid = diag.wifi_connected ? WiFi.SSID() : String("");
        diag.ip = diag.wifi_connected ? WiFi.localIP().toString() : String("");
        diag.has_valid_metrics = false;
        diag.fetch_failed = fetch_failed;
        diag.last_http_code = last_http_code;
        diag.last_success_ms = last_success_ms;
        diag.connect_state = ConnectState::IDLE;

        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            diag.has_valid_metrics = has_valid_metrics;
            diag.fetch_failed = fetch_failed;
            diag.last_http_code = last_http_code;
            diag.last_success_ms = last_success_ms;
            diag.connect_state = connect_state;
            xSemaphoreGive(metrics_mutex);
        }
        return diag;
    }

    bool isFetchFailed() {
        bool failed = false;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, 0) == pdTRUE) {
            failed = fetch_failed;
            xSemaphoreGive(metrics_mutex);
        } else {
            failed = fetch_failed;
        }
        return failed;
    }

    String getMetricsIP() {
        Preferences prefs;
        if (!prefs.begin(kSettingsNamespace, true)) {
            return String("192.168.1.10");
        }
        String ip = prefs.getString("metrics_ip", "192.168.1.10");
        prefs.end();
        return ip;
    }

    String getMetricsPath() {
        Preferences prefs;
        if (!prefs.begin(kSettingsNamespace, true)) {
            return String("/metrics");
        }
        String path = prefs.getString("metrics_path", "/metrics");
        prefs.end();
        return path;
    }

    void setMetricsConfig(const String& ip, const String& path) {
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            cached_ip = ip;
            cached_path = path;
            xSemaphoreGive(metrics_mutex);
        } else {
            cached_ip = ip;
            cached_path = path;
        }

        Preferences prefs;
        if (prefs.begin(kSettingsNamespace, false)) {
            prefs.putString("metrics_ip", ip);
            prefs.putString("metrics_path", path);
            prefs.end();
        }
    }

    AppSettings loadAppSettings() {
        AppSettings s;
        Preferences prefs;
        if (!prefs.begin(kSettingsNamespace, true)) {
            return s;
        }

        s.theme = prefs.getInt("theme", 0);
        s.touch_cursor = prefs.getBool("touch_cursor", true);
        s.autodim_sec = prefs.getInt("autodim_sec", 0);
        s.autodim_level = prefs.getInt("autodim_lvl", 15);
        s.alerts.voltage_min = prefs.getFloat("alert_vmin", 95.0f);
        s.alerts.voltage_max = prefs.getFloat("alert_vmax", 127.0f);
        s.alerts.current_max = prefs.getFloat("alert_imax", 10.0f);
        s.tariff = prefs.getFloat("tariff", 0.0f);
        s.session_kwh_base = prefs.getDouble("session_base", NAN);
        prefs.end();
        return s;
    }

    void saveAppSettings(const AppSettings &settings) {
        AppSettings to_save = settings;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!std::isfinite(to_save.session_kwh_base) && std::isfinite(cached_settings.session_kwh_base)) {
                to_save.session_kwh_base = cached_settings.session_kwh_base;
            }
            cached_settings = to_save;
            xSemaphoreGive(metrics_mutex);
        } else {
            if (!std::isfinite(to_save.session_kwh_base) && std::isfinite(cached_settings.session_kwh_base)) {
                to_save.session_kwh_base = cached_settings.session_kwh_base;
            }
            cached_settings = to_save;
        }

        Preferences prefs;
        if (prefs.begin(kSettingsNamespace, false)) {
            prefs.putInt("theme", to_save.theme);
            prefs.putBool("touch_cursor", to_save.touch_cursor);
            prefs.putInt("autodim_sec", to_save.autodim_sec);
            prefs.putInt("autodim_lvl", to_save.autodim_level);
            prefs.putFloat("alert_vmin", to_save.alerts.voltage_min);
            prefs.putFloat("alert_vmax", to_save.alerts.voltage_max);
            prefs.putFloat("alert_imax", to_save.alerts.current_max);
            prefs.putFloat("tariff", to_save.tariff);
            prefs.putDouble("session_base", to_save.session_kwh_base);
            prefs.end();
        }
    }

    double getSessionEnergyBaseline() {
        double base = NAN;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            base = cached_settings.session_kwh_base;
            xSemaphoreGive(metrics_mutex);
        } else {
            base = cached_settings.session_kwh_base;
        }
        return base;
    }

    void resetSessionEnergyBaseline() {
        double new_base = NAN;
        if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (has_valid_metrics && std::isfinite(shared_metrics.energy)) {
                new_base = shared_metrics.energy;
            }
            cached_settings.session_kwh_base = new_base;
            xSemaphoreGive(metrics_mutex);
        }

        Preferences prefs;
        if (prefs.begin(kSettingsNamespace, false)) {
            prefs.putDouble("session_base", new_base);
            prefs.end();
        }
    }
}
