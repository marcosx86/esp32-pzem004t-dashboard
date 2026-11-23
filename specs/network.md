# Network Management, Async Wi-Fi & NVS Schema Specification

## 1. Pure Credential Store & URL Builder (`Network_Store`)

- **Serialization Format:** Newline-separated alternating lines `ssid\npassword` in UTF-8 plain text (no trailing delimiter after the final pair).
- **Parsing (`parseWifiCredentials`):**
  - Splits safely without integer wrap-around at `std::string::npos`.
  - Preserves open networks (empty password line paired with non-empty SSID).
  - Ignores entries only when both SSID and password are empty.
- **Boot-Priority Insertion (`addOrUpdateWifiCredential`):**
  - Rejects empty `ssid` (`return false`).
  - If `index` is valid (`0 <= index < size`), removes the entry at `index` first.
  - Removes any remaining entry whose SSID matches case-insensitively (`ASCII` case-insensitive comparison) to prevent duplicates.
  - Inserts `{ssid, password}` at **index `0`**, prioritizing the most recently saved/explicitly connected network on subsequent boots.
- **Deletion (`deleteWifiCredential`):**
  - Removes entry at `index` if in range, returning `true`; returns `false` on out-of-bounds index.
- **URL Builder (`buildMetricsUrl`):**
  - Prepends `http://` unless `ip` already starts with `http://` or `https://`.
  - Ensures `path` begins with `/` (default `"/metrics"` if empty).

---

## 2. Core-0 Non-Blocking Network State Machine (`NetworkManager`)

### Boot Behavior (`beginWithStoredNetworks`)
1. Loads `cached_ip`, `cached_path`, `AppSettings`, and NVS `session_kwh_base` immediately.
2. Initializes `shared_metrics = makeEmptyMetrics()`, `has_valid_metrics = false`, `last_http_code = 0`, `fetch_failed = false`.
3. Creates `metrics_mutex` and spawns Core-0 FreeRTOS task `network_task` (`stack = 6144` bytes, `priority = 1`, `core = 0`), then **returns immediately** so Core 1 renders the UI without blocking.

### Core-0 Task Loop
1. **Explicit Connect Request (`requestConnect(ssid, pass)`):**
   - Takes highest priority (even interrupting boot network cycling).
   - Sets `connect_state = ConnectState::CONNECTING`.
   - Attempts `WiFi.begin(ssid, pass)` up to `10,000 ms`, aborting early if another `requestConnect` supersedes it.
   - Updates `connect_state = CONNECTED` or `FAILED`.
2. **Boot Auto-Connect Cycling (`boot_connect_pending`):**
   - Iterates `loadSavedNetworks()` in index order (`0..N-1`) attempting `connectToWifi()` (up to `10,000 ms` per SSID), aborting immediately if a manual `requestConnect()` arrives.
3. **Metrics Polling (`1000 ms` cadence):**
   - If `WiFi.status() == WL_CONNECTED`: performs HTTP GET to `buildMetricsUrl(cached_ip, cached_path)` via `fetch_metrics(..., &http_code)`.
     - On `true` (HTTP 200 + >=1 valid metric): acquires `metrics_mutex`, sets `shared_metrics`, `has_valid_metrics = true`, `last_success_ms = millis()`, `fetch_failed = false`, pushes sample into `metrics_history`, and initializes `session_kwh_base` in RAM/NVS if currently `NaN`.
     - On `false`: sets `fetch_failed = true`, records `last_http_code` (HTTP code or negative client error).
   - If not connected: sets `fetch_failed = true`, `last_http_code = -1`.

### Asynchronous Wi-Fi Scan API *(Core-0 Serialized Execution)*
- **Core-0 Ownership:** All `WiFi.scan*` calls run exclusively on Core 0 inside `network_task`, serialized against boot cycling, explicit connects, and HTTP polling to avoid `ESP_ERR_WIFI_STATE` rejections and cross-core Wi-Fi driver contention:
  - `startWifiScan()`: queues `scan_requested = true`, sets `scan_state = ScanState::SCANNING`, and clears `cached_scan_results` under `metrics_mutex` (also interrupting active boot auto-connect cycling so scanning starts promptly).
  - **STA Idle Preparation:** When `WiFi.status() != WL_CONNECTED`, Core 0 temporarily disables `WiFi.setAutoReconnect(false)` and calls `WiFi.disconnect(false)` + `150 ms` settle delay so ESP-IDF STA leaves `WIFI_STATE_CONNECTING` cleanly before calling `WiFi.scanNetworks(false, false, false, 200)`.
  - **Retry & Normalization:** If `WiFi.scanNetworks` returns `WIFI_SCAN_FAILED (-2)`, Core 0 waits `250 ms` and retries once before marking `ScanState::FAILED`. Raw entries are normalized via `Network_Store::normalizeScanResults()` (dropping empty SSIDs, deduplicating by strongest RSSI, sorting RSSI descending with deterministic SSID tie-break), stored in `cached_scan_results`, and freed via `WiFi.scanDelete()`. Auto-reconnect is restored when previously disconnected.
- `getWifiScanStatus()`: returns `-1` while `SCANNING`, `-2` on `FAILED`/`IDLE`, or `>= 0` (discovered count) on `DONE`.
- `getWifiScanResults()`: returns a thread-safe copy of `cached_scan_results` (`ssid`, `rssi`, `encrypted`) when `DONE` (else empty).

### Diagnostics (`NetworkDiagnostics`)
Exposes thread-safe snapshot for HUD & Settings:
- `wifi_connected` (`bool`), `rssi_dbm` (`int32_t`), `ssid` (`String`), `ip` (`String`), `has_valid_metrics` (`bool`), `fetch_failed` (`bool`), `last_http_code` (`int`), `last_success_ms` (`uint32_t`), `connect_state` (`ConnectState`).
- `getSessionEnergyBaseline()`: thread-safely returns `cached_settings.session_kwh_base` (`double`, `NAN` if uninitialized) for Core 1 UI session delta calculation. `saveAppSettings()` preserves an already-initialized finite baseline if the incoming struct carries `NAN`.

---

## 3. Complete ESP32 NVS Schema

| Namespace | Key | Type | Default | Description |
|-----------|-----|------|---------|-------------|
| `wifi_settings` | `networks` | `String` | `""` | Newline-delimited `ssid\npassword` list (index 0 = boot priority). |
| `settings` | `brightness` | `int32` | `128` | LCD backlight PWM duty (`10..255`). |
| `settings` | `metrics_ip` | `String` | `"192.168.1.10"` | Target metrics server host/IP (optionally with `:port`). |
| `settings` | `metrics_path` | `String` | `"/metrics"` | Target Prometheus endpoint path. |
| `settings` | `theme` | `int32` | `0` | UI theme (`0 = Primitive`, `1 = Dark High Contrast`). |
| `settings` | `touch_cursor` | `bool` | `true` | Red touch debug dot visibility (`ON` by default). |
| `settings` | `autodim_sec` | `int32` | `0` | Inactivity auto-dim timeout in seconds (`0 = Off`). |
| `settings` | `autodim_lvl` | `int32` | `15` | PWM brightness when auto-dimmed. |
| `settings` | `alert_vmin` | `float` | `95.0f` | Voltage lower alert threshold (V). |
| `settings` | `alert_vmax` | `float` | `127.0f` | Voltage upper alert threshold (V). |
| `settings` | `alert_imax` | `float` | `10.0f` | Current limit (A); dynamic power limit = `V * alert_imax`. |
| `settings` | `tariff` | `float` | `0.0f` | Electricity cost per kWh. |
| `settings` | `session_base` | `double` | `NAN` | Session baseline energy (kWh). |
| `touch_cal` | `valid` | `bool` | `false` | `true` when custom 4-point affine matrix is stored. |
| `touch_cal` | `use_default` | `bool` | `false` | `true` after **Reset TS Calib** so boot skips calibration wizard. |
| `touch_cal` | `cal_A`..`cal_F` | `double` | Default matrix | 6 affine transformation coefficients. |
