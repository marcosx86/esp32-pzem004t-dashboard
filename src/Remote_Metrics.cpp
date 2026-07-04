#include "Remote_Metrics.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Simple Prometheus text exposition parser. We only look for a few metric names.
static double parse_value_from_line(const String &line) {
    // line format: metric_name value
    int sp = line.indexOf(' ');
    if (sp < 0) return NAN;
    String val = line.substring(sp + 1);
    return val.toFloat();
}

bool fetch_metrics(const char* url, MetricsData &out) {
    if (!WiFi.isConnected()) return false;

    HTTPClient http;
    http.begin(url);
    http.setConnectTimeout(3000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    // initialize defaults
    out.timestamp = millis();
    out.voltage = NAN;
    out.current = NAN;
    out.energy = NAN;
    out.wattage = NAN;

    // parse line by line
    int start = 0;
    while (start < payload.length()) {
        int nl = payload.indexOf('\n', start);
        String line;
        if (nl < 0) {
            line = payload.substring(start);
            start = payload.length();
        } else {
            line = payload.substring(start, nl);
            start = nl + 1;
        }

        line.trim();
        if (line.length() == 0) continue;
        if (line[0] == '#') continue; // comment

        if (line.startsWith("pzem_voltage")) {
            out.voltage = parse_value_from_line(line);
        } else if (line.startsWith("pzem_current")) {
            out.current = parse_value_from_line(line);
        } else if (line.startsWith("pzem_power ") || line.startsWith("pzem_power\t")) {
            out.wattage = parse_value_from_line(line);
        } else if (line.startsWith("pzem_energy")) {
            out.energy = parse_value_from_line(line);
        }
    }

    return true;
}
