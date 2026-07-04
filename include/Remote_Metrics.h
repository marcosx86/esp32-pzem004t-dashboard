#ifndef REMOTE_METRICS_H
#define REMOTE_METRICS_H

#include <Arduino.h>
#include "lvgl.h"

/* Default metric data points */
#define METRICS_POINTS_DEFAULT 4

/* ESPHome/Home Assistant etc. devices endpoint template */
const char* const METRICS_ENDPOINT = "/metrics";

/* Define the metric data structure */
typedef struct {
    uint32_t timestamp;          /* Time stamp (Unix epoch seconds) */
    double voltage;              /* Voltage V */
    double current;              /* Current A */
    double energy;               /* Energy Wh */
    double wattage;              /* Power W */
} MetricsData;

/* Initialize the metrics data parser for reading sensor data from the /metrics endpoint */
// Fetch metrics from a full URL like "http://192.168.1.50/metrics".
// Returns true on success and fills `out`, false on failure.
bool fetch_metrics(const char* url, MetricsData &out);

#endif /*REMOTE_METRICS_H*/
