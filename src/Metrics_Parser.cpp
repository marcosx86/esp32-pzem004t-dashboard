#include "Metrics_Parser.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

struct MetricFieldMapping {
    const char* name;
    double MetricsData::* field;
};

const MetricFieldMapping kMetricMappings[] = {
    {"pzem_voltage",      &MetricsData::voltage},
    {"pzem_current",      &MetricsData::current},
    {"pzem_power",        &MetricsData::wattage},
    {"pzem_energy",       &MetricsData::energy},
    {"pzem_frequency",    &MetricsData::frequency},
    {"pzem_power_factor", &MetricsData::power_factor},
    {"dht22_temperature", &MetricsData::temperature},
    {"dht22_humidity",    &MetricsData::humidity},
};

inline bool isSpaceOrTab(char ch) {
    return ch == ' ' || ch == '\t';
}

double MetricsData::* findMetricField(const char* name, size_t len) {
    for (size_t i = 0; i < sizeof(kMetricMappings) / sizeof(kMetricMappings[0]); ++i) {
        if (std::strlen(kMetricMappings[i].name) == len &&
            std::strncmp(kMetricMappings[i].name, name, len) == 0) {
            return kMetricMappings[i].field;
        }
    }
    return nullptr;
}

const char* skipLabelBlock(const char* p, const char* end) {
    // Assumes *p == '{'; advances past the matching '}' while respecting quoted strings.
    ++p;
    bool in_quotes = false;
    bool escaped = false;
    while (p < end) {
        char ch = *p++;
        if (in_quotes) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_quotes = false;
            }
        } else {
            if (ch == '"') {
                in_quotes = true;
            } else if (ch == '}') {
                return p;
            }
        }
    }
    return nullptr;
}

void parseMetricLine(const char* start, const char* end, MetricsData &out) {
    while (start < end && isSpaceOrTab(*start)) {
        ++start;
    }
    while (end > start && (isSpaceOrTab(*(end - 1)) || *(end - 1) == '\r')) {
        --end;
    }

    if (start >= end || *start == '#') {
        return;
    }

    const char* name_end = start;
    while (name_end < end && *name_end != '{' && !isSpaceOrTab(*name_end)) {
        ++name_end;
    }

    size_t name_len = static_cast<size_t>(name_end - start);
    double MetricsData::* field = findMetricField(start, name_len);
    if (!field || name_end >= end) {
        return;
    }

    const char* cursor = name_end;
    if (*cursor == '{') {
        cursor = skipLabelBlock(cursor, end);
        if (!cursor || cursor >= end) {
            return;
        }
    }

    // Require at least one space or tab separating metric/labels from the value.
    if (!isSpaceOrTab(*cursor)) {
        return;
    }
    while (cursor < end && isSpaceOrTab(*cursor)) {
        ++cursor;
    }
    if (cursor >= end) {
        return;
    }

    // Isolate token up to whitespace or comment so strtod has a bounded c-string.
    const char* token_end = cursor;
    while (token_end < end && !isSpaceOrTab(*token_end) && *token_end != '#') {
        ++token_end;
    }

    size_t token_len = static_cast<size_t>(token_end - cursor);
    if (token_len == 0 || token_len >= 64) {
        return;
    }

    char buf[64];
    std::memcpy(buf, cursor, token_len);
    buf[token_len] = '\0';

    char* parse_end = nullptr;
    double value = std::strtod(buf, &parse_end);
    if (parse_end == buf || *parse_end != '\0') {
        return;
    }

    out.*field = std::isfinite(value) ? value : static_cast<double>(NAN);
}

} // namespace

MetricsData makeEmptyMetrics() {
    MetricsData m;
    m.timestamp = 0;
    m.voltage = NAN;
    m.current = NAN;
    m.energy = NAN;
    m.wattage = NAN;
    m.frequency = NAN;
    m.power_factor = NAN;
    m.temperature = NAN;
    m.humidity = NAN;
    return m;
}

bool parseMetricsPayload(const char* payload, MetricsData &out) {
    out = makeEmptyMetrics();
    if (!payload || payload[0] == '\0') {
        return false;
    }

    const char* cursor = payload;
    while (*cursor != '\0') {
        const char* nl = std::strchr(cursor, '\n');
        const char* line_end = nl ? nl : (cursor + std::strlen(cursor));
        parseMetricLine(cursor, line_end, out);
        if (!nl) {
            break;
        }
        cursor = nl + 1;
    }

    return std::isfinite(out.voltage) ||
           std::isfinite(out.current) ||
           std::isfinite(out.energy) ||
           std::isfinite(out.wattage) ||
           std::isfinite(out.frequency) ||
           std::isfinite(out.power_factor) ||
           std::isfinite(out.temperature) ||
           std::isfinite(out.humidity);
}
