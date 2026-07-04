#include "Touch_Calibrator.h"
#include <Preferences.h>
#include <cmath>

bool loadTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated) {
    Preferences prefs;
    if (!prefs.begin("touch_cal", true)) { // Read-only
        Serial.println("[Calibrate] NVS touch_cal namespace does not exist or failed to open.");
        calibrated = false;
        return false;
    }
    
    // Check if calibration is marked valid
    calibrated = prefs.getBool("valid", false);
    if (!calibrated) {
        prefs.end();
        return false;
    }
    
    a = prefs.getDouble("cal_A", 0.0);
    b = prefs.getDouble("cal_B", 1.0);
    c = prefs.getDouble("cal_C", 0.0);
    d = prefs.getDouble("cal_D", -1.0);
    e = prefs.getDouble("cal_E", 0.0);
    f = prefs.getDouble("cal_F", 240.0);
    
    prefs.end();
    
    Serial.printf("[Calibrate] Loaded calibration from NVS:\n");
    Serial.printf("  A=%.4f, B=%.4f, C=%.4f\n", a, b, c);
    Serial.printf("  D=%.4f, E=%.4f, F=%.4f\n", d, e, f);
    return true;
}

void saveTouchCalibration(double a, double b, double c, double d, double e, double f, bool calibrated) {
    Preferences prefs;
    if (!prefs.begin("touch_cal", false)) { // Read-write
        Serial.println("[Calibrate] Failed to open NVS for writing!");
        return;
    }
    
    prefs.putDouble("cal_A", a);
    prefs.putDouble("cal_B", b);
    prefs.putDouble("cal_C", c);
    prefs.putDouble("cal_D", d);
    prefs.putDouble("cal_E", e);
    prefs.putDouble("cal_F", f);
    prefs.putBool("valid", calibrated);
    
    prefs.end();
    Serial.println("[Calibrate] Calibration saved to NVS.");
}

void runTouchCalibration(LGFX &tft, CST820 &touch, double &cal_A, double &cal_B, double &cal_C, double &cal_D, double &cal_E, double &cal_F, bool &touch_calibrated) {
    Serial.println("\n=== Touchscreen Calibration ===");
    
    struct {
        uint16_t x_raw, y_raw;
    } cal_points[4];
    
    const char *cal_labels[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-LEFT", "BOTTOM-RIGHT"};
    const uint16_t target_x[4] = {20, 300, 20, 300};
    const uint16_t target_y[4] = {20, 20, 220, 220};
    
    for (int i = 0; i < 4; i++) {
        tft.fillScreen(TFT_BLACK);
        
        // Draw crosshair at target point
        tft.drawCircle(target_x[i], target_y[i], 15, TFT_WHITE);
        tft.drawLine(target_x[i] - 20, target_y[i], target_x[i] + 20, target_y[i], TFT_WHITE);
        tft.drawLine(target_x[i], target_y[i] - 20, target_x[i], target_y[i] + 20, TFT_WHITE);
        
        // Draw text instruction
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(50, 110);
        tft.printf("Click on");
        tft.setCursor(60, 135);
        tft.printf("%s", cal_labels[i]);
        
        Serial.printf("Please click on %s (%d, %d)...\n", cal_labels[i], target_x[i], target_y[i]);
        
        // Wait for touch
        bool touched = false;
        unsigned long timeout = millis() + 10000;  // 10 second timeout
        while (!touched && millis() < timeout) {
            uint16_t x, y;
            uint8_t gesture;
            if (touch.getTouch(&x, &y, &gesture)) {
                cal_points[i].x_raw = x;
                cal_points[i].y_raw = y;
                Serial.printf("  Raw touch: (%d, %d)\n", x, y);
                touched = true;
                delay(300);  // Debounce
            }
            delay(10);
        }
        
        if (!touched) {
            Serial.println("  Timeout! Calibration aborted, using default settings.");
            // Fallback to default rotated values
            cal_A = 0.0; cal_B = 1.0; cal_C = 0.0;
            cal_D = -1.0; cal_E = 0.0; cal_F = 240.0;
            touch_calibrated = false;
            return;
        }
    }
    
    // Calculate calibration offsets (3-point affine mapping)
    // Solving: x_display = A*x_raw + B*y_raw + C
    // Solving: y_display = D*x_raw + E*y_raw + F
    // Using cal_points[0] (TL), cal_points[1] (TR), cal_points[2] (BL)
    
    double xr0 = cal_points[0].x_raw, yr0 = cal_points[0].y_raw;
    double xr1 = cal_points[1].x_raw, yr1 = cal_points[1].y_raw;
    double xr2 = cal_points[2].x_raw, yr2 = cal_points[2].y_raw;
    
    double xs0 = target_x[0], ys0 = target_y[0];
    double xs1 = target_x[1], ys1 = target_y[1];
    double xs2 = target_x[2], ys2 = target_y[2];
    
    double det = xr0 * (yr1 - yr2) + xr1 * (yr2 - yr0) + xr2 * (yr0 - yr1);
    
    if (std::abs(det) > 0.0001) {
        cal_A = (xs0 * (yr1 - yr2) + xs1 * (yr2 - yr0) + xs2 * (yr0 - yr1)) / det;
        cal_B = (xr0 * (xs1 - xs2) + xr1 * (xs2 - xs0) + xr2 * (xs0 - xs1)) / det;
        cal_C = (xr0 * (yr1 * xs2 - yr2 * xs1) + 
                 xr1 * (yr2 * xs0 - yr0 * xs2) + 
                 xr2 * (yr0 * xs1 - yr1 * xs0)) / det;
                 
        cal_D = (ys0 * (yr1 - yr2) + ys1 * (yr2 - yr0) + ys2 * (yr0 - yr1)) / det;
        cal_E = (xr0 * (ys1 - ys2) + xr1 * (ys2 - ys0) + xr2 * (ys0 - ys1)) / det;
        cal_F = (xr0 * (yr1 * ys2 - yr2 * ys1) + 
                 xr1 * (yr2 * ys0 - yr0 * ys2) + 
                 xr2 * (yr0 * ys1 - yr1 * ys0)) / det;
                 
        touch_calibrated = true;
        Serial.println("Calibration solved successfully:");
        Serial.printf("  A=%.4f, B=%.4f, C=%.4f\n", cal_A, cal_B, cal_C);
        Serial.printf("  D=%.4f, E=%.4f, F=%.4f\n", cal_D, cal_E, cal_F);
        
        // Save to NVS
        saveTouchCalibration(cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, true);
    } else {
        Serial.println("Calibration failed: Determinant is zero (collinear points). Using default rotation.");
        cal_A = 0.0; cal_B = 1.0; cal_C = 0.0;
        cal_D = -1.0; cal_E = 0.0; cal_F = 240.0;
        touch_calibrated = false;
    }
    
    Serial.println("Calibration complete!\n");
    delay(500);
}
