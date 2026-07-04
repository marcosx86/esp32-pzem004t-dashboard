#include "Network_Manager.h"
#include <WiFi.h>

namespace NetworkManager {
    static MetricsData shared_metrics;
    static SemaphoreHandle_t metrics_mutex = NULL;
    static String target_url;
    static bool init_done = false;

    // Background task running on Core 0
    static void fetch_metrics_task(void *pvParameters) {
        while (1) {
            MetricsData temp_m;
            unsigned long start_time = millis();
            bool success = fetch_metrics(target_url.c_str(), temp_m);
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

    void begin(const char* ssid, const char* pass, const char* metrics_url) {
        if (init_done) return;
        target_url = metrics_url;

        // Connect to WiFi
        Serial.printf("Connecting to WiFi '%s'", ssid);
        WiFi.setAutoReconnect(true); // Enable background auto-reconnection
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
        } else {
            Serial.println(" failed to connect (WiFi stack will auto-retry in the background)");
        }

        // Initialize mutex for shared metrics data and spawn background fetch task on Core 0
        metrics_mutex = xSemaphoreCreateMutex();
        if (metrics_mutex != NULL) {
            xTaskCreatePinnedToCore(
                fetch_metrics_task,      /* Task function */
                "fetch_metrics_task",    /* Name of task */
                4096,                    /* Stack size in words */
                NULL,                    /* Task input parameter */
                1,                       /* Priority of the task */
                NULL,                    /* Task handle */
                0                        /* Pin task to Core 0 */
            );
            Serial.println("Background metrics fetch task created successfully on Core 0.");
        }
        init_done = true;
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
}
