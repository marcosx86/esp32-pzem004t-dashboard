# Architectural Specification (`esp32_touchscreen`)

## 1. Purpose & Hardware Target
`esp32_touchscreen` is a real-time power and environment monitor firmware targeting the **Sunton ESP32-2432S022** (`upesy_wroom`, ESP32-WROOM-32, 320×240 landscape ST7789 8-bit MCU8080 parallel display, CST820 I2C capacitive touchscreen) using **PlatformIO**, **Arduino**, **LovyanGFX**, and **LVGL v8.3.11**.

The device polls a local Prometheus text exposition endpoint (e.g., ESPHome / Home Assistant PZEM-004T + DHT22 node) and renders live telemetry, trend diagnostics, alerts, and configuration controls.

Hardware pinouts and vendor datasheets live in [README.md](README.md) and `docs/`.

---

## 2. Core Architecture: Dual-Core FreeRTOS Separation

| Core | Ownership | Responsibilities |
|------|-----------|------------------|
| **Core 0** | `NetworkManager` (`network_task`) | Sequential boot Wi-Fi association, non-blocking explicit Wi-Fi connect requests, asynchronous Wi-Fi scanning, and periodic (`1000 ms`) HTTP Prometheus polling & parsing. |
| **Core 1** | Arduino `loop()` (`main.cpp` + `DashboardUI`) | LVGL rendering (`lv_timer_handler()`), CST820 touch sampling & affine transformation, PWM backlight control, inactivity auto-dimming, and user interaction. |

### Concurrency Contract
- Shared telemetry (`MetricsData`), `MetricsHistory`, diagnostics (`NetworkDiagnostics`), and `cached_ip` / `cached_path` are protected by a FreeRTOS mutex (`metrics_mutex`).
- Core 1 must never block waiting on Wi-Fi association or HTTP sockets; Core 0 must never call LVGL APIs (`lv_*`) directly.

---

## 3. Pure-Logic Decoupling & Verification Contract
To support fast host unit/regression testing and `gcov`/`gcovr` coverage under PlatformIO `[env:native]` (`Unity`), domain logic is strictly isolated from `<Arduino.h>`, `<WiFi.h>`, `<HTTPClient.h>`, `Preferences.h`, `LovyanGFX.hpp`, and `lvgl.h`:

- `Metrics_Parser` (`include/Metrics_Parser.h`, `src/Metrics_Parser.cpp`)
- `Metrics_Stats` (`include/Metrics_Stats.h`, `src/Metrics_Stats.cpp`)
- `Network_Store` (`include/Network_Store.h`, `src/Network_Store.cpp`)
- `Touch_Math` (`include/Touch_Math.h`, `src/Touch_Math.cpp`)
- `UI_Logic` (`include/UI_Logic.h`, `src/UI_Logic.cpp`)

All five modules depend solely on standard C++11 headers.

---

## 4. Specification Index

- [specs/metrics.md](specs/metrics.md) — Prometheus 8-metric parser, initial/stale tracking, 60-sample ring buffer & statistics, voltage/current/dynamic-power alert rules, and session energy/cost calculation.
- [specs/network.md](specs/network.md) — Non-blocking Core-0 Wi-Fi state machine, async scanner, diagnostics contract, credential priority serialization, and complete ESP32 NVS schema.
- [specs/touch.md](specs/touch.md) — 4-point least-squares affine calibration solver, clamping, instant **Reset TS Calib**, reboot **Calibrate Touch**, and Red Touch Cursor toggle.
- [specs/ui.md](specs/ui.md) — 320×240 ergonomic layout (top-50px floating HUD), multi-theme support (**Primitive** default & **Dark High Contrast**), 160px keyboard preservation, zoom sparklines, and wake-safe auto-dim.

---

## 5. Rollout Roadmap

- **Milestone 1 (Completed):** Establish `SPEC.md` & `specs/*.md`, configure `[env:native]`, implement the pure-logic modules + Unity regression suites, fix `0.00` initial startup values, upgrade touch solver to 4-point least-squares + `resetTouchCalibration`, and make Wi-Fi boot/connect/scan non-blocking on Core 0.
- **Milestone 2 (Active):** Top-50px floating HUD (Wi-Fi/RSSI/freshness + tap diagnostic popup, DHT22 Temp/Humidity center pill, Brightness/Settings right icons), **Primitive** / **Dark High Contrast** theme selector, async Wi-Fi scan list + connect spinner, custom IP/numeric keyboard mode + scroll-into-view (keeping 160px keyboard height), toggleable Red Touch Cursor, **Reset TS Calib** button, and clean non-flashing boot.
- **Milestone 3 (Planned Analytics & Power Saving):** Enable `LV_USE_CHART`, zoom modal `lv_chart` sparklines + `Min / Avg / Max`, Voltage/Current/Power threshold alert highlights, Session kWh + Tariff Cost + **Reset Session**, and Auto-Dim inactivity sleep with wake-touch absorption.
