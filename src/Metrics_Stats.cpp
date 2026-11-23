#include "Metrics_Stats.h"

#include <cmath>

MetricRingBuffer::MetricRingBuffer() : head_(0), count_(0) {
    for (size_t i = 0; i < kMetricsHistorySize; ++i) {
        samples_[i] = 0.0;
    }
}

void MetricRingBuffer::clear() {
    head_ = 0;
    count_ = 0;
}

void MetricRingBuffer::push(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    samples_[head_] = value;
    head_ = (head_ + 1) % kMetricsHistorySize;
    if (count_ < kMetricsHistorySize) {
        ++count_;
    }
}

size_t MetricRingBuffer::size() const {
    return count_;
}

size_t MetricRingBuffer::copyOrdered(double *dst, size_t max_len) const {
    if (!dst || max_len == 0 || count_ == 0) {
        return 0;
    }

    size_t to_copy = (count_ < max_len) ? count_ : max_len;
    size_t oldest = (head_ + kMetricsHistorySize - count_) % kMetricsHistorySize;
    size_t start = (oldest + (count_ - to_copy)) % kMetricsHistorySize;

    for (size_t i = 0; i < to_copy; ++i) {
        dst[i] = samples_[(start + i) % kMetricsHistorySize];
    }
    return to_copy;
}

MetricStats MetricRingBuffer::computeStats() const {
    MetricStats stats;
    stats.count = count_;
    if (count_ == 0) {
        stats.min_val = NAN;
        stats.avg_val = NAN;
        stats.max_val = NAN;
        return stats;
    }

    size_t oldest = (head_ + kMetricsHistorySize - count_) % kMetricsHistorySize;
    double first = samples_[oldest];
    stats.min_val = first;
    stats.max_val = first;
    double sum = first;

    for (size_t i = 1; i < count_; ++i) {
        double val = samples_[(oldest + i) % kMetricsHistorySize];
        if (val < stats.min_val) stats.min_val = val;
        if (val > stats.max_val) stats.max_val = val;
        sum += val;
    }
    stats.avg_val = sum / static_cast<double>(count_);
    return stats;
}

void MetricsHistory::clear() {
    voltage.clear();
    current.clear();
    energy.clear();
    wattage.clear();
}

void MetricsHistory::push(const MetricsData &m) {
    voltage.push(m.voltage);
    current.push(m.current);
    energy.push(m.energy);
    wattage.push(m.wattage);
}

bool isMetricsStale(bool has_valid_metrics, uint32_t last_success_ms, uint32_t now_ms, uint32_t stale_ms) {
    if (!has_valid_metrics) {
        return true;
    }
    return static_cast<uint32_t>(now_ms - last_success_ms) > stale_ms;
}

double computeDynamicPowerLimit(double voltage, float current_max) {
    if (!std::isfinite(current_max) || current_max <= 0.0f) {
        return 0.0;
    }
    double ref_v = (std::isfinite(voltage) && voltage > 0.0) ? voltage : 127.0;
    return ref_v * static_cast<double>(current_max);
}

AlertLevel evaluateVoltageAlert(double voltage, const AlertThresholds &t) {
    if (!std::isfinite(voltage)) {
        return AlertLevel::OK;
    }
    if ((t.voltage_min > 0.0f && voltage < static_cast<double>(t.voltage_min)) ||
        (t.voltage_max > 0.0f && voltage > static_cast<double>(t.voltage_max))) {
        return AlertLevel::CRIT;
    }
    if ((t.voltage_min > 0.0f && voltage < static_cast<double>(t.voltage_min) + 3.0) ||
        (t.voltage_max > 0.0f && voltage > static_cast<double>(t.voltage_max) - 3.0)) {
        return AlertLevel::WARN;
    }
    return AlertLevel::OK;
}

AlertLevel evaluateCurrentAlert(double current, const AlertThresholds &t) {
    if (!std::isfinite(current) || !std::isfinite(t.current_max) || t.current_max <= 0.0f) {
        return AlertLevel::OK;
    }
    double limit = static_cast<double>(t.current_max);
    if (current > limit) {
        return AlertLevel::CRIT;
    }
    if (current >= 0.85 * limit) {
        return AlertLevel::WARN;
    }
    return AlertLevel::OK;
}

AlertLevel evaluatePowerAlert(double wattage, double voltage, const AlertThresholds &t) {
    if (!std::isfinite(wattage)) {
        return AlertLevel::OK;
    }
    double limit = computeDynamicPowerLimit(voltage, t.current_max);
    if (limit <= 0.0) {
        return AlertLevel::OK;
    }
    if (wattage > limit) {
        return AlertLevel::CRIT;
    }
    if (wattage >= 0.85 * limit) {
        return AlertLevel::WARN;
    }
    return AlertLevel::OK;
}

SessionEnergyInfo computeSessionEnergy(double current_energy_kwh, double baseline_kwh, double wattage, float tariff_per_kwh) {
    SessionEnergyInfo info{0.0, 0.0, 0.0};

    if (std::isfinite(current_energy_kwh) && std::isfinite(baseline_kwh) && current_energy_kwh >= baseline_kwh) {
        info.session_kwh = current_energy_kwh - baseline_kwh;
    }

    if (std::isfinite(tariff_per_kwh) && tariff_per_kwh > 0.0f) {
        double tariff = static_cast<double>(tariff_per_kwh);
        info.session_cost = info.session_kwh * tariff;
        if (std::isfinite(wattage) && wattage > 0.0) {
            info.hourly_cost = (wattage / 1000.0) * tariff;
        }
    }

    return info;
}
