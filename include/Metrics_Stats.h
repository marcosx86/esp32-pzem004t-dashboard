#ifndef METRICS_STATS_H
#define METRICS_STATS_H

#include "Metrics_Parser.h"
#include <cstddef>
#include <cstdint>

static constexpr size_t kMetricsHistorySize = 60;
static constexpr uint32_t kMetricsStaleMs = 5000;

enum class AlertLevel {
    OK = 0,
    WARN = 1,
    CRIT = 2
};

struct AlertThresholds {
    float voltage_min = 95.0f;   /* 0 disables lower voltage bound */
    float voltage_max = 127.0f;  /* 0 disables upper voltage bound */
    float current_max = 10.0f;   /* 0 disables current & dynamic power alerts */
};

struct MetricStats {
    size_t count;
    double min_val;
    double avg_val;
    double max_val;
};

struct SessionEnergyInfo {
    double session_kwh;
    double session_cost;
    double hourly_cost;
};

/**
 * @brief Fixed-capacity ring buffer storing up to kMetricsHistorySize finite samples.
 */
class MetricRingBuffer {
public:
    MetricRingBuffer();

    void clear();
    void push(double value);
    size_t size() const;

    /**
     * @brief Copies up to @p max_len most recent samples in chronological order (oldest -> newest).
     * @return Number of samples copied.
     */
    size_t copyOrdered(double *dst, size_t max_len) const;

    MetricStats computeStats() const;

private:
    double samples_[kMetricsHistorySize];
    size_t head_;
    size_t count_;
};

/**
 * @brief Ring-buffer history for the four primary power telemetry channels.
 */
struct MetricsHistory {
    MetricRingBuffer voltage;
    MetricRingBuffer current;
    MetricRingBuffer energy;
    MetricRingBuffer wattage;

    void clear();
    void push(const MetricsData &m);
};

/**
 * @brief Checks whether telemetry is missing or older than @p stale_ms (rollover-safe).
 */
bool isMetricsStale(bool has_valid_metrics, uint32_t last_success_ms, uint32_t now_ms, uint32_t stale_ms = kMetricsStaleMs);

/**
 * @brief Computes dynamic power ceiling (voltage * current_max), falling back to 127 V when voltage is unavailable.
 */
double computeDynamicPowerLimit(double voltage, float current_max);

AlertLevel evaluateVoltageAlert(double voltage, const AlertThresholds &t);
AlertLevel evaluateCurrentAlert(double current, const AlertThresholds &t);
AlertLevel evaluatePowerAlert(double wattage, double voltage, const AlertThresholds &t);

/**
 * @brief Computes session energy delta and tariff costs, guarding against cumulative counter resets.
 */
SessionEnergyInfo computeSessionEnergy(double current_energy_kwh, double baseline_kwh, double wattage, float tariff_per_kwh);

#endif /* METRICS_STATS_H */
