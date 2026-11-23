#ifndef METRICS_PARSER_H
#define METRICS_PARSER_H

#include <cstdint>

/**
 * @brief Telemetry snapshot parsed from a Prometheus text exposition endpoint.
 */
typedef struct {
    uint32_t timestamp;   /* Fetch timestamp (millis) */
    double voltage;       /* Voltage (V) - pzem_voltage */
    double current;       /* Current (A) - pzem_current */
    double energy;        /* Cumulative energy (kWh) - pzem_energy */
    double wattage;       /* Active power (W) - pzem_power */
    double frequency;     /* Line frequency (Hz) - pzem_frequency */
    double power_factor;  /* Power factor (Ratio) - pzem_power_factor */
    double temperature;   /* Ambient temperature (C) - dht22_temperature */
    double humidity;      /* Ambient humidity (%) - dht22_humidity */
} MetricsData;

/**
 * @brief Returns an empty MetricsData struct initialized with NAN values and timestamp 0.
 */
MetricsData makeEmptyMetrics();

/**
 * @brief Parses a Prometheus text exposition payload into MetricsData.
 *
 * Resets @p out via makeEmptyMetrics() before parsing. Matches metric names
 * strictly, supports optional {label="val"} blocks and space/tab delimiters,
 * and rejects non-numeric values without coercing them to 0.
 *
 * @return true if at least one supported metric parsed as a finite value.
 */
bool parseMetricsPayload(const char* payload, MetricsData &out);

#endif /* METRICS_PARSER_H */
