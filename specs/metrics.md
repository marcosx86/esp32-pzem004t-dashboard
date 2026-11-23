# Metrics Parsing, History, Alerts & Session Cost Specification

## 1. Prometheus Text Exposition Parser (`Metrics_Parser`)

### Data Model (`MetricsData`)
| Field | Metric Name | Unit | Default / Missing |
|-------|-------------|------|-------------------|
| `timestamp` | *(millis at fetch)* | ms | `0` |
| `voltage` | `pzem_voltage` | V | `NAN` |
| `current` | `pzem_current` | A | `NAN` |
| `energy` | `pzem_energy` | kWh | `NAN` |
| `wattage` | `pzem_power` | W | `NAN` |
| `frequency` | `pzem_frequency` | Hz | `NAN` |
| `power_factor` | `pzem_power_factor` | Ratio | `NAN` |
| `temperature` | `dht22_temperature` | °C | `NAN` |
| `humidity` | `dht22_humidity` | % | `NAN` |

`makeEmptyMetrics()` returns `MetricsData` with `timestamp = 0` and all 8 `double` fields initialized to `NAN`.

### Parsing Rules (`parseMetricsPayload`)
1. Always reset `out = makeEmptyMetrics()` before scanning `payload`. If `payload` is `nullptr` or empty, return `false`.
2. Split lines on `\n` and strip trailing `\r` (`\r\n` compatible) as well as leading/trailing spaces and tabs.
3. Skip empty lines and lines starting with `#` (`# HELP`, `# TYPE`, comments).
4. **Exact metric name match:** Read the metric identifier up to the first `'{'`, `' '`, or `'\t'`. Compare for **exact equality** against the supported metric names so `pzem_power` never collides with `pzem_power_factor`.
5. **Optional Prometheus labels:** If `'{'` immediately follows the metric name, scan forward to the closing `'}'` (respecting double-quoted strings with `\"` escapes) before searching for the value separator.
6. **Whitespace delimiter:** Require at least one space or tab after the metric name (or `'}'`) before the numeric value.
7. **Strict numeric parsing (`std::strtod`):**
   - Advance past spaces/tabs and parse with `std::strtod`.
   - Reject if no characters were consumed or if trailing text immediately after the number is not whitespace (allowing an optional trailing Prometheus timestamp or `#` comment).
   - Invalid text (e.g., `"abc"`, `""`) leaves the field as `NAN` — never silently coerces to `0.0`.
   - Literal `"NaN"` / `"nan"` yields `NAN`.
8. **Return value:** Returns `true` if **at least one** of the 8 supported metrics parsed as a finite value (`std::isfinite`); otherwise returns `false`.

---

## 2. Initial State & Stale Data Rules (`Metrics_Stats`)

- **Initial / No-Data State:** Before the first valid HTTP 200 parse (`has_valid_metrics == false`), `NetworkManager::getLatestMetrics()` returns `false` and the UI displays `--` instead of `0.00`.
- **Stale Threshold:** `kMetricsStaleMs = 5000` (5 seconds).
- `isMetricsStale(has_valid_metrics, last_success_ms, now_ms, stale_ms)`:
  - Returns `true` if `!has_valid_metrics`.
  - Otherwise computes unsigned wrap-safe difference `static_cast<uint32_t>(now_ms - last_success_ms) > stale_ms`.

---

## 3. Ring Buffer & Summary Statistics

- **Capacity:** `kMetricsHistorySize = 60` samples per core metric (`voltage`, `current`, `energy`, `wattage`) inside `MetricsHistory` (~2 KB static RAM total).
- **Insertion (`MetricRingBuffer::push`):** Non-finite (`NaN` / `±Inf`) values are ignored so invalid polls do not corrupt history or statistics.
- **Chronological Copy (`copyOrdered`):** Copies up to `max_len` valid samples from oldest to newest into a caller buffer for `lv_chart` rendering.
- **Statistics (`computeStats`):** Returns `MetricStats { count, min_val, avg_val, max_val }`. When `count == 0`, `min_val`, `avg_val`, and `max_val` are `NAN`.

---

## 4. Threshold Alerts (`AlertThresholds`)

### Confirmed Defaults
- `voltage_min = 95.0f` V (`0.0f` disables lower bound)
- `voltage_max = 127.0f` V (`0.0f` disables upper bound)
- `current_max = 10.0f` A (`0.0f` disables current and dynamic power limits)

### Dynamic Power Limit
- `computeDynamicPowerLimit(voltage, current_max)`:
  - If `current_max <= 0.0f`: returns `0.0` (disabled).
  - If `std::isfinite(voltage) && voltage > 0.0`: returns `voltage * static_cast<double>(current_max)`.
  - Otherwise (missing/non-positive voltage): falls back to `127.0 * static_cast<double>(current_max)` (`1270.0 W` at default `10 A`).

### Alert Evaluation (`AlertLevel`: `OK = 0`, `WARN = 1`, `CRIT = 2`)
- **Non-finite inputs:** Any `NAN` / non-finite measurement returns `AlertLevel::OK`.
- **Voltage (`evaluateVoltageAlert`):**
  - `CRIT`: `(t.voltage_min > 0 && voltage < t.voltage_min) || (t.voltage_max > 0 && voltage > t.voltage_max)`
  - `WARN`: `(t.voltage_min > 0 && voltage < t.voltage_min + 3.0) || (t.voltage_max > 0 && voltage > t.voltage_max - 3.0)` (3 V warning band inside active bounds)
  - `OK`: otherwise.
- **Current (`evaluateCurrentAlert`):**
  - If `t.current_max <= 0`: `OK`.
  - `CRIT`: `current > t.current_max`.
  - `WARN`: `current >= 0.85 * t.current_max`.
  - `OK`: otherwise.
- **Power (`evaluatePowerAlert`):**
  - Compute `limit = computeDynamicPowerLimit(voltage, t.current_max)`. If `limit <= 0`: `OK`.
  - `CRIT`: `wattage > limit`.
  - `WARN`: `wattage >= 0.85 * limit`.
  - `OK`: otherwise.

---

## 5. Session Energy & Estimated Cost

`computeSessionEnergy(current_energy_kwh, baseline_kwh, wattage, tariff_per_kwh)` returns `SessionEnergyInfo { session_kwh, session_cost, hourly_cost }`:
- If `!std::isfinite(current_energy_kwh)` or `!std::isfinite(baseline_kwh)` or `current_energy_kwh < baseline_kwh` (e.g., PZEM cumulative counter reset): `session_kwh = 0.0` (never negative); else `session_kwh = current_energy_kwh - baseline_kwh`.
- If `tariff_per_kwh > 0.0f && std::isfinite(tariff_per_kwh)`:
  - `session_cost = session_kwh * tariff_per_kwh`.
  - `hourly_cost = (std::isfinite(wattage) && wattage > 0.0) ? (wattage / 1000.0) * tariff_per_kwh : 0.0`.
- Otherwise both costs are `0.0`.
