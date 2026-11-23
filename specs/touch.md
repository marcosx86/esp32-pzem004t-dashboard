# Touchscreen Calibration, Math & Cursor Specification

## 1. Pure Calibration Math (`Touch_Math`)

### Default Landscape Matrix (`defaultTouchCalibration`)
For the 320×240 panel with `offset_rotation = 1` and raw CST820 coordinates `(x_raw, y_raw)`:
$$A = 0.0,\quad B = 1.0,\quad C = 0.0,\quad D = -1.0,\quad E = 0.0,\quad F = 240.0,\quad \text{calibrated} = \text{false}$$

### 4-Point Least-Squares Affine Solver (`solveAffineCalibration4Pt`)
Given 4 raw tap samples `(xr_i, yr_i)` and screen targets `(xs_i, ys_i)` at corners `(20, 20)`, `(300, 20)`, `(20, 220)`, `(300, 220)`:
1. Solves overdetermined affine system:
   $$x_s = A x_r + B y_r + C, \qquad y_s = D x_r + E y_r + F$$
   via the $3 \times 3$ normal equations $M \theta = b$ where:
   $$M = \begin{bmatrix} \sum x_r^2 & \sum x_r y_r & \sum x_r \\ \sum x_r y_r & \sum y_r^2 & \sum y_r \\ \sum x_r & \sum y_r & 4 \end{bmatrix}$$
2. **Singularity Guard:** If $|\det(M)| < 10^{-6}$ or any solved coefficient is non-finite, rejects the fit, sets `out = defaultTouchCalibration()`, and returns `false`.
3. **Residual Quality Guard:** Computes Euclidean distance $\sqrt{(x_{\text{pred}, i} - x_{s, i})^2 + (y_{\text{pred}, i} - y_{s, i})^2}$ across all 4 targets.
   - Reports maximum residual in `*out_max_residual` (if provided).
   - If `max_residual > max_allowed_residual` (default `25.0 px`), rejects the calibration (catches accidental corner mistaps), sets `out = defaultTouchCalibration()`, and returns `false`.
4. On success, sets `out.calibrated = true` and returns `true`.

### Coordinate Application & Clamping (`applyTouchCalibration`)
- When `cal.calibrated == true`:
  $$x = \text{int16\_t}(A \cdot x_{\text{raw}} + B \cdot y_{\text{raw}} + C), \qquad y = \text{int16\_t}(D \cdot x_{\text{raw}} + E \cdot y_{\text{raw}} + F)$$
- When `cal.calibrated == false`:
  $$x = \text{int16\_t}(y_{\text{raw}}), \qquad y = \text{int16\_t}(\text{screen\_h} - x_{\text{raw}})$$
- Clamps both outputs strictly to `[0, screen_w - 1]` and `[0, screen_h - 1]`.

---

## 2. Calibration Lifecycle & Actions (`Touch_Calibrator`)

- **Boot Loading (`loadTouchCalibration`):**
  1. If `touch_cal/valid == true`: loads `cal_A..cal_F`, sets `calibrated = true`, returns `true`.
  2. Else if `touch_cal/use_default == true`: loads `defaultTouchCalibration()`, sets `calibrated = false`, returns `true` (boots directly to dashboard using default rotation without prompting the 4-corner wizard).
  3. Otherwise returns `false` (triggers `runTouchCalibration()` on boot when `TOUCH_CALIBRATION` is defined).
- **Reset TS Calib (`resetTouchCalibration`):**
  - Writes `valid = false` and `use_default = true` to NVS `touch_cal`.
  - Immediately sets runtime `cal_A..cal_F` and `calibrated` back to `defaultTouchCalibration()` **without rebooting**.
- **Calibrate Touch (`triggerTouchCalibration`):**
  - Writes `valid = false` and `use_default = false` to NVS `touch_cal`, then restarts the ESP32 (`ESP.restart()`) so boot runs the 4-corner interactive calibration wizard.

---

## 3. Red Touch Debug Cursor

- Rendered on `lv_layer_sys()` as a 20×20 red circle (`0xFF0000`, `LV_OPA_80`) centered on `(calibrated_x, calibrated_y)` while pressed, hidden on release.
- Controlled by `settings/touch_cursor` (default **`true` / ON**).
- *(Active — Milestone 2)* Exposed as a live **Red Touch Dot** switch in Settings (and **Reset TS Calib** button alongside **Calibrate Touch**) so adult-finger alignment can be toggled or reset on the fly without rebooting.
