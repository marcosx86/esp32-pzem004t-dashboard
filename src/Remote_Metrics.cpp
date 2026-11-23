#include "Remote_Metrics.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

bool fetch_metrics(const char* url, MetricsData &out, int *out_http_code) {
    if (!WiFi.isConnected()) {
        if (out_http_code) {
            *out_http_code = -1;
        }
        out = makeEmptyMetrics();
        return false;
    }

    HTTPClient http;
    http.begin(url);
    http.setConnectTimeout(3000);
    int code = http.GET();
    if (out_http_code) {
        *out_http_code = code;
    }
    if (code != HTTP_CODE_OK) {
        http.end();
        out = makeEmptyMetrics();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (!parseMetricsPayload(payload.c_str(), out)) {
        return false;
    }
    out.timestamp = millis();
    return true;
}
