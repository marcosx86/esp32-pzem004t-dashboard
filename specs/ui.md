# Dashboard UI & Ergonomics Specification (`320×240` Landscape)

> **Rollout Note:** Baseline dashboard layout (2×2 cards, zoom, scrollable Settings modal, brightness cancel overlay, persistent keyboard, red touch cursor) is currently active. The floating top-50px HUD enhancements, theming switcher, async Wi-Fi scan UI, zoom sparklines, alert highlights, and auto-dim listed below target **Milestones 2 & 3**.

---

## 1. Ergonomic Layout & Top-50px Floating HUD

To preserve maximum touch area for an adult finger on a `320×240` panel, **no fixed/reserved top bar space is deducted from the 2×2 metric cards**. HUD elements hover cleanly inside the top `50px` band (`y = 4..48`):

- **Top-Left Cluster:** Compact floating pill/buttons showing Wi-Fi connectivity + RSSI strength and data freshness dot (`green` = live `<= 5s`, `amber` = stale `> 5s`, `red` = Wi-Fi down or HTTP error). Tapping opens a compact diagnostic popup showing SSID, IP, RSSI (dBm), and last HTTP code/age.
- **Top-Center Pill:** Floating ambient readout from `dht22_temperature` & `dht22_humidity` (e.g., `26.6°C · 84%`), hidden or `--` when DHT22 metrics are unavailable.
- **Top-Right Buttons:** Circular Brightness (`40×40`) and Settings (`40×40`) buttons.

---

## 2. Dashboard Cards & Secondary Telemetry

- **2×2 Card Grid:** `Voltage (V)`, `Current (A)`, `Energy (kWh)`, `Power (W)` keeping full usable touch height.
- **Before First Valid Sample:** Cards display `--` (never `0.00`).
- **Secondary Sub-Labels *(Milestone 2/3)*:**
  - `Voltage` card displays grid frequency (`60.0 Hz`).
  - `Power` card displays power factor (`PF 0.95`).
  - `Energy` card displays session kWh / cost when `tariff > 0`.
- **Alert Visual Feedback *(Milestone 3)*:** Cards evaluate `AlertLevel` (`OK`, `WARN`, `CRIT`) against configured `alert_vmin` (`95 V`), `alert_vmax` (`127 V`), `alert_imax` (`10 A`), and dynamic power (`V * 10 A`), tinting card borders/badges amber (`WARN`) or red (`CRIT`).

---

## 3. Multi-Theme Support *(Milestone 2)*

Persisted via `settings/theme`:
1. **`Primitive` (Default, `theme = 0`):** Current high-saturation card palette (`#4A90E2` Voltage, `#E74C3C` Current, `#2ECC71` Energy, `#F39C12` Power) and Teal (`#63A986`) Settings background.
2. **`Dark High Contrast` (`theme = 1`):** Dark background (`#12161C`), dark slate cards (`#1D2430`) with colored left/top accent bars (`#4A90E2`, `#E74C3C`, `#2ECC71`, `#F39C12`), crisp white numerals, and dark slate Settings modal (`#18202A`).

---

## 4. Zoom Modal with Sparklines *(Milestone 3)*

- Requires `#define LV_USE_CHART 1` in `include/lv_conf.h`.
- Tapping any dashboard card opens a centered modal showing:
  - Enlarged live numeric reading + unit.
  - `lv_chart` line sparkline populated from `MetricsHistory` (60 samples).
  - `Min`, `Avg`, and `Max` values over the buffered window.
  - **Energy card only:** A **Reset Session** button that sets `session_kwh_base = current_energy` and persists to NVS.

---

## 5. Settings Modal, Keyboard & Touch Controls *(Milestone 2/3)*

- **Keyboard Dimensions (Preserved):** Global `lv_keyboard` on `lv_layer_top()` keeps `width = 320`, `height = 160` (`screen_height / 2 + 40`) with 75% opacity so keys stay usable for adult fingers.
- **Numeric Keyboard Mode & Scroll-Into-View:** Focusing `metrics_ip_ta` or numeric alert/tariff inputs switches `lv_keyboard` to `LV_KEYBOARD_MODE_NUMBER`, and Settings scrolls the focused input into the top `80px` band above the keyboard.
- **Settings Controls:**
  - **Metrics Config:** IP, Path, Save.
  - **Saved Networks + Async Scan:** **Scan WiFi** button lists discovered SSIDs + RSSI for tap-to-fill; **Connect** uses async Core-0 connect with a live `lv_spinner` overlay.
  - **Appearance & Touch:** Theme dropdown/selector (`Primitive` / `Dark High Contrast`), **Touch Dot** switch (`ON` by default, updates immediately), **Reset TS Calib** button (immediate default calibration, no reboot), **Calibrate Touch** button (reboot wizard).
  - **Alerts, Tariff & Auto-Dim *(Milestone 3)*:** Editable `V min`, `V max`, `I max`, `Tariff (/kWh)`, and `Auto-Dim` timeout (`Off`, `60s`, `120s`, `300s`, `600s`).

---

## 6. Clean Boot & Inactivity Auto-Dim

- **Clean Boot *(Milestone 2)*:** Remove the synchronous RGB screen-flash delay from `setup()` so the dashboard renders immediately while Core 0 connects in the background.
- **Auto-Dim Wake Absorption *(Milestone 3)*:** When `autodim_sec > 0` elapses without touch, PWM brightness drops to `autodim_lvl`. The first touch restores user brightness and absorbs that tap (`LV_INDEV_STATE_REL` until release) so no card or button triggers accidentally.
