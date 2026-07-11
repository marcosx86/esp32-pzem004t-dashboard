#include "Network_Manager.h"
#include <Preferences.h>
#include <WiFi.h>

namespace NetworkManager {
    static MetricsData shared_metrics;
    static SemaphoreHandle_t metrics_mutex = NULL;
    static String cached_ip = "192.168.81.16";
    static String cached_path = "/metrics";
    static bool init_done = false;
    static bool fetch_failed = false;
    static const char* kNvsNamespace = "wifi_settings";
    static const char* kNvsKey = "networks";

    static String serializeNetworks(const std::vector<WifiCredential>& networks) {
        String output;
        for (size_t i = 0; i < networks.size(); ++i) {
            if (i > 0) {
                output += '\n';
            }
            output += networks[i].ssid;
            output += '\n';
            output += networks[i].password;
        }
        return output;
    }

    static std::vector<WifiCredential> parseNetworks(const String& content) {
        std::vector<WifiCredential> networks;
        if (content.length() == 0) {
            return networks;
        }

        String current_ssid;
        String current_password;
        int line_index = 0;
        int start = 0;
        while (start <= content.length()) {
            int end = content.indexOf('\n', start);
            if (end < 0) {
                end = content.length();
            }

            String line = content.substring(start, end);
            if (line_index % 2 == 0) {
                current_ssid = line;
            } else {
                current_password = line;
                if (!current_ssid.isEmpty() || !current_password.isEmpty()) {
                    networks.push_back({current_ssid, current_password});
                }
            }

            line_index++;
            start = end + 1;
        }

        return networks;
    }

    // Background task running on Core 0
    static void fetch_metrics_task(void *pvParameters) {
        while (1) {
            String url;
            if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                url = "http://" + cached_ip + cached_path;
                xSemaphoreGive(metrics_mutex);
            } else {
                url = "http://" + cached_ip + cached_path; // Fallback read
            }

            MetricsData temp_m;
            unsigned long start_time = millis();
            bool success = fetch_metrics(url.c_str(), temp_m);
            fetch_failed = !success;
            unsigned long duration = millis() - start_time;

            Serial.printf("[Metrics] Fetch took %lu ms (status: %s)\n", duration, success ? "success" : "failed");

            if (success) {
                if (metrics_mutex && xSemaphoreTake(metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    shared_metrics = temp_m;
                    xSemaphoreGive(metrics_mutex);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
        }
    }

    static bool connectToWifi(const char* ssid, const char* pass) {
        Serial.printf("Connecting to WiFi '%s'", ssid);
        WiFi.disconnect();
        delay(100);
        WiFi.setAutoReconnect(true);
        WiFi.begin(ssid, pass);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
            delay(200);
            Serial.print('.');
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(" connected");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
            return true;
        } else {
            Serial.println(" failed to connect");
            return false;
        }
    }

    void begin(const char* ssid, const char* pass, const char* metrics_url) {
        if (init_done) return;

        connectToWifi(ssid, pass);

        metrics_mutex = xSemaphoreCreateMutex();
        if (metrics_mutex != NULL) {
            xTaskCreatePinnedToCore(
                fetch_metrics_task,
                "fetch_metrics_task",
                4096,
                NULL,
                1,
                NULL,
                0
            );
            Serial.println("Background metrics fetch task created successfully on Core 0.");
        }
        init_done = true;
    }

    void beginWithStoredNetworks() {
        if (init_done) return;

        cached_ip = getMetricsIP();
        cached_path = getMetricsPath();

        std::vector<WifiCredential> networks = loadSavedNetworks();
        bool connected = false;
        
        for (const auto& net : networks) {
            if (connectToWifi(net.ssid.c_str(), net.password.c_str())) {
                connected = true;
                break;
            }
        }
        
        if (!connected) {
            Serial.println("All stored networks failed.");
        }

        metrics_mutex = xSemaphoreCreateMutex();
        if (metrics_mutex != NULL) {
            xTaskCreatePinnedToCore(
                fetch_metrics_task,
                "fetch_metrics_task",
                4096,
                NULL,
                1,
                NULL,
                0
            );
            Serial.println("Background metrics fetch task created successfully on Core 0.");
        }
        init_done = true;
    }

    std::vector<WifiCredential> loadSavedNetworks() {
        std::vector<WifiCredential> networks;
        Preferences prefs;
        if (!prefs.begin(kNvsNamespace, true)) {
            return networks;
        }

        String content = prefs.getString(kNvsKey, "");
        prefs.end();
        return parseNetworks(content);
    }

    bool saveNetworks(const std::vector<WifiCredential>& networks) {
        Preferences prefs;
        if (!prefs.begin(kNvsNamespace, false)) {
            return false;
        }

        String serialized = serializeNetworks(networks);
        prefs.putString(kNvsKey, serialized.c_str());
        prefs.end();
        return true;
    }

    bool addOrUpdateNetwork(const String& ssid, const String& password, int index) {
        std::vector<WifiCredential> networks = loadSavedNetworks();
        
        // If editing a specific index, remove it first
        if (index >= 0 && index < static_cast<int>(networks.size())) {
            networks.erase(networks.begin() + index);
        }
        
        // Ensure no other duplicate SSID exists to prevent duplicates
        for (auto it = networks.begin(); it != networks.end(); ) {
            if (it->ssid.equalsIgnoreCase(ssid)) {
                it = networks.erase(it);
            } else {
                ++it;
            }
        }
        
        // Insert at the front so it's prioritized on boot
        networks.insert(networks.begin(), {ssid, password});
        
        return saveNetworks(networks);
    }

    bool deleteNetwork(int index) {
        std::vector<WifiCredential> networks = loadSavedNetworks();
        if (index < 0 || index >= static_cast<int>(networks.size())) {
            return false;
        }

        networks.erase(networks.begin() + index);
        return saveNetworks(networks);
    }

    bool connectToNetwork(const String& ssid, const String& password) {
        return connectToWifi(ssid.c_str(), password.c_str());
    }

    bool getLatestMetrics(MetricsData &out) {
        if (!init_done || metrics_mutex == NULL) return false;

        if (xSemaphoreTake(metrics_mutex, 0) == pdTRUE) {
            out = shared_metrics;
            xSemaphoreGive(metrics_mutex);
            return true;
        }
        return false;
    }

    bool isFetchFailed() {
        return fetch_failed;
    }

    String getMetricsIP() {
        Preferences prefs;
        prefs.begin("settings", true);
        String ip = prefs.getString("metrics_ip", "192.168.1.10");
        prefs.end();
        return ip;
    }

    String getMetricsPath() {
        Preferences prefs;
        prefs.begin("settings", true);
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
        prefs.begin("settings", false);
        prefs.putString("metrics_ip", ip);
        prefs.putString("metrics_path", path);
        prefs.end();
    }
}
