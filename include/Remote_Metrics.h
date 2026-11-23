#ifndef REMOTE_METRICS_H
#define REMOTE_METRICS_H

#include "Metrics_Parser.h"

/* Default metric data points */
#define METRICS_POINTS_DEFAULT 4

/* ESPHome/Home Assistant etc. devices endpoint template */
const char* const METRICS_ENDPOINT = "/metrics";

/**
 * @brief Fetch and parse Prometheus metrics from a full HTTP URL.
 *
 * Sets @p out_http_code (if non-null) to -1 when Wi-Fi is disconnected,
 * or to the HTTPClient status/error code otherwise.
 *
 * @return true on HTTP 200 with at least one parsed finite metric.
 */
bool fetch_metrics(const char* url, MetricsData &out, int *out_http_code = nullptr);

#endif /* REMOTE_METRICS_H */
