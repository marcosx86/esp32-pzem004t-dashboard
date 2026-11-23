# Dashboard UI & Ergonomics Specification (`320×240` Landscape)

> **Rollout Note:** All Milestones 1–3 UI capabilities are **Active**: Top-50px Floating HUD, Multi-Theme (**Primitive** & **Dark High Contrast**), Async Wi-Fi Scan + Connect Spinner, Custom Numeric/IP Keyboard + Scroll-Into-View, Red Touch Dot Toggle, **Reset TS Calib**, Clean Boot, Zoom `lv_chart` Sparklines + `Min / Avg / Max` + Session Reset, Threshold Alert Highlights, and Auto-Dim with Wake-Touch Absorption.

---

## 1. Ergonomic Layout & Top-50px Floating HUD *(Active)*

To preserve maximum touch area for an adult finger on a `320×240` panel, **no fixed/reserved top bar space is deducted from the 2×2 metric cards**. All HUD elements float inside the top `50px` band (`y = 4..44`):

- **Top-Left Status Pill (`x = 4, y = 6`, `90×36`, radius `18`, `LV_OPA_80`, clickable):**
  - Displays a `10×10` freshness dot (`#2ECC71` LIVE `<= 5s`, `#F39C12` STALE `> 5s` / transient fail / initial connect, `#E74C3C` ERROR Wi-Fi down or fetch failed) beside `LV_SYMBOL_WIFI` and signal bars (`4/4..1/4` or `Off` computed via `rssiToSignalBars`).
  - Tapping opens a **Diagnostics** modal (`250×175`) showing live `NetworkManager::getDiagnostics()` fields (Wi-Fi state, SSID, IP, RSSI dBm, metrics freshness/age, last HTTP code) + `OK` button (also dismissible via backdrop tap).
- **Top-Center Ambient Pill (`LV_ALIGN_TOP_MID, +3, 8`, `LV_SIZE_CONTENT × 32`, radius `16`, `LV_OPA_80`):**
  - Displays ambient DHT22 telemetry using Montserrat-14-supported glyphs (`°` = `\xC2\xB0`, `•` = `\xE2\x80\xA2`), e.g., `26.6°C • 84%` (or `--°C • --%` when unavailable; handles single-sensor availability).
- **Top-Right Buttons:** Circular Brightness (`40×40`, `LV_ALIGN_TOP_RIGHT, -48, 4`) and Settings (`40×40`, `LV_ALIGN_TOP_RIGHT, -4, 4`) buttons.

---

## 2. Dashboard Cards, Secondary Telemetry & Alert Feedback *(Active)*

- **2×2 Card Grid:** `Voltage (V)`, `Current (A)`, `Energy (kWh)`, `Power (W)` keeping full usable touch height (`card_w = (320 - 24)/2`, `card_h = (240 - 24)/2 * 0.80f`), each with a bottom-right sub-label (`lv_font_montserrat_14`).
- **Before First Valid Sample:** Cards display `--` (never `0.00`).
- **Secondary Sub-Labels:**
  - `Voltage`: grid frequency (`60.0 Hz`) + `" WARN"` / `" LOW"` / `" HIGH"` when alerted.
  - `Current`: `"WARN"` / `"OVER"` when alerted.
  - `Energy`: computes `SessionEnergyInfo` from `NetworkManager::getSessionEnergyBaseline()` and `app_settings.tariff`; displays `"S:<kwh>"` (plus `" ($<cost>)"` when `tariff > 0`).
  - `Power`: power factor (`PF 0.95`) + `" WARN"` / `" OVER"` when alerted.
- **Alert Visual Feedback (`getAlertBorderColor` / `getAlertBorderWidth`):**
  - Cards evaluate `AlertLevel` (`OK`, `WARN`, `CRIT`) against configured `alert_vmin` (`95 V`), `alert_vmax` (`127 V`), `alert_imax` (`10 A`), and dynamic power (`V * 10 A`).
  - `WARN` sets border `#F1C40F` (`4px`); `CRIT` sets border `#FF2D2D` (`5px`); `OK` restores theme border color & width.

---

## 3. Multi-Theme Support *(Active)*

Defined in `UI_Logic` (`UiThemePalette`) and persisted via `settings/theme` (`normalizeThemeId` coerces invalid IDs to `0`):

| Element | `Primitive` (`0`, Default) | `Dark High Contrast` (`1`) |
|---------|----------------------------|----------------------------|
| Screen BG | `#FFFFFF` | `#12161C` |
| Cards BG | `#4A90E2`, `#E74C3C`, `#2ECC71`, `#F39C12` | `#1D2430` (all 4) |
| Card Borders | `#444444` (`2px`) | `#4A90E2`, `#E74C3C`, `#2ECC71`, `#F39C12` (`3px`) |
| Card Titles | `#1A1A1A` | `#4A90E2`, `#E74C3C`, `#2ECC71`, `#F39C12` |
| Card Values / Sub | `#111111` / `#222222` | `#FFFFFF` / `#A9B4C2` |
| HUD Pill BG / Text | `#222A35` / `#FFFFFF` | `#0D1117` / `#FFFFFF` |
| Settings BG / Text | `#63A986` / `#111111` | `#18202A` / `#F0F4F8` |
| Editor Panel BG | `#FFB19A` | `#243040` |
| List Border | `#555555` | `#3A4A5E` |

Selecting a theme in Settings applies styles immediately to the dashboard, HUD pills, Settings modal, and Zoom view and persists to NVS.

---

## 4. Zoom Modal with Sparklines & Session Reset *(Active)*

- Enabled via `#define LV_USE_CHART 1` in `include/lv_conf.h`.
- Tapping any dashboard card opens a `296×206` centered modal containing:
  - Top header with metric title and `"X"` close button (`30×26`), plus backdrop click-out.
  - Enlarged live numeric reading + unit (`zoom_value_label`, Montserrat 32).
  - `lv_chart` line sparkline (`268×74`, or `268×58` on Energy) with `60` points scaled to `0..1000` via `scaleSamplesToChart()` (`5%` vertical margin).
  - `Min / Avg / Max` stats line from `MetricRingBuffer::computeStats()`.
  - **Energy card only:** Session summary (`S:<kwh>`, plus cost & hourly rate when `tariff > 0`) and **Reset** button (`68×26`) invoking `NetworkManager::resetSessionEnergyBaseline()`.

---

## 5. Settings Modal, Keyboard, Async Wi-Fi, Alerts & Auto-Dim *(Active)*

- **Keyboard Dimensions (Preserved):** Global `lv_keyboard` on `lv_layer_top()` keeps `width = 320`, `height = 160` (`screen_height / 2 + 40`) at ~75% opacity (`191`) for adult-finger usability.
- **Custom IP/Numeric Keymap & Scroll-Into-View:**
  - `LV_KEYBOARD_MODE_USER_1` registers a 4×4 grid (`1, 2, 3, Backspace / 4, 5, 6, : / 7, 8, 9, . / abc, 0, Keyboard-hide, OK`).
  - Focusing `metrics_ip_ta` or numeric Alerts/Tariff textareas (`InputFieldKind::NUMERIC_IP`) selects `LV_KEYBOARD_MODE_USER_1`; text inputs (`InputFieldKind::TEXT`) select `LV_KEYBOARD_MODE_TEXT_LOWER`.
  - Focusing an input inside `settings_content` expands `pad_bottom = 170` and scrolls it into view; focusing inside `editor_panel` aligns `editor_panel` to `LV_ALIGN_TOP_MID` (`y = 2`) above the 160px keyboard. `LV_EVENT_READY`, `LV_EVENT_CANCEL`, and `LV_EVENT_DEFOCUSED` restore padding/alignment and hide the keyboard.
- **Settings Sections (Scrollable Flex Column):**
  - **Metrics Config:** IP, Path, **Save**.
  - **Saved Networks + Async Scan / Connect:** **Scan** triggers `NetworkManager::startWifiScan()` + spinner list; **Connect** uses async Core-0 `requestConnect()` + spinner.
  - **Appearance & Touch:** Theme dropdown, **Red Touch Dot** switch (`true` default), **Reset TS Calib** (instant default rotation without reboot), **Calibrate Touch** (reboot wizard).
  - **Alerts & Tariff:** `Vmin`, `Vmax`, `Imax`, `Tariff` numeric inputs + **Save Limits** + **Reset Session**.
  - **Auto-Dim:** Dropdown (`Off`, `1 min`, `2 min`, `5 min`, `10 min` mapped to `{0, 60, 120, 300, 600}` seconds).

---

## 6. Clean Boot & Inactivity Auto-Dim *(Active)*

- **Clean Boot:** No synchronous RGB screen-flash delay during `setup()`; dashboard initializes immediately while Core 0 connects asynchronously.
- **Auto-Dim Wake Absorption (`AutoDimController`):** When `autodim_sec > 0` elapses without touch, PWM brightness drops to `autodim_level` (`15`). The first touch restores `DashboardUI::getActiveBrightness()` immediately and absorbs that press until finger release (`LV_INDEV_STATE_REL`) so no card or button triggers accidentally.
