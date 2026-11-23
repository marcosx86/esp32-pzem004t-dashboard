#include "Touch_Calibrator.h"
#include <Preferences.h>
#include <cmath>

namespace {

void assignCalibration(const TouchCalibration &src,
                       double &a, double &b, double &c,
                       double &d, double &e, double &f,
                       bool &calibrated) {
    a = src.a;
    b = src.b;
    c = src.c;
    d = src.d;
    e = src.e;
    f = src.f;
    calibrated = src.calibrated;
}

} // namespace

bool loadTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated) {
    TouchCalibration def = defaultTouchCalibration();

    Preferences prefs;
    if (!prefs.begin("touch_cal", true)) {
        Serial.println("[Calibrate] NVS touch_cal namespace does not exist or failed to open.");
        assignCalibration(def, a, b, c, d, e, f, calibrated);
        return false;
    }

    bool valid = prefs.getBool("valid", false);
    bool use_default = prefs.getBool("use_default", false);

    if (valid) {
        a = prefs.getDouble("cal_A", def.a);
        b = prefs.getDouble("cal_B", def.b);
        c = prefs.getDouble("cal_C", def.c);
        d = prefs.getDouble("cal_D", def.d);
        e = prefs.getDouble("cal_E", def.e);
        f = prefs.getDouble("cal_F", def.f);
        calibrated = true;
        prefs.end();

        Serial.printf("[Calibrate] Loaded calibration from NVS:\n");
        Serial.printf("  A=%.4f, B=%.4f, C=%.4f\n", a, b, c);
        Serial.printf("  D=%.4f, E=%.4f, F=%.4f\n", d, e, f);
        return true;
    }

    prefs.end();
    assignCalibration(def, a, b, c, d, e, f, calibrated);

    if (use_default) {
        Serial.println("[Calibrate] Using persisted default rotation (Reset TS Calib active).");
        return true;
    }

    return false;
}

void saveTouchCalibration(double a, double b, double c, double d, double e, double f, bool calibrated) {
    Preferences prefs;
    if (!prefs.begin("touch_cal", false)) {
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
    prefs.putBool("use_default", false);

    prefs.end();
    Serial.println("[Calibrate] Calibration saved to NVS.");
}

void resetTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated) {
    TouchCalibration def = defaultTouchCalibration();
    assignCalibration(def, a, b, c, d, e, f, calibrated);

    Preferences prefs;
    if (prefs.begin("touch_cal", false)) {
        prefs.putBool("valid", false);
        prefs.putBool("use_default", true);
        prefs.end();
    }
    Serial.println("[Calibrate] Touch calibration reset to default rotation.");
}

void runTouchCalibration(LGFX &tft, CST820 &touch, double &cal_A, double &cal_B, double &cal_C, double &cal_D, double &cal_E, double &cal_F, bool &touch_calibrated) {
    Serial.println("\n=== Touchscreen Calibration ===");

    TouchPoint raw_points[4];
    const TouchPoint targets[4] = {
        {20.0, 20.0},
        {300.0, 20.0},
        {20.0, 220.0},
        {300.0, 220.0}
    };
    const char *cal_labels[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-LEFT", "BOTTOM-RIGHT"};

    for (int i = 0; i < 4; i++) {
        uint16_t tx = static_cast<uint16_t>(targets[i].x);
        uint16_t ty = static_cast<uint16_t>(targets[i].y);

        tft.fillScreen(TFT_BLACK);

        // Draw crosshair at target point
        tft.drawCircle(tx, ty, 15, TFT_WHITE);
        tft.drawLine(tx - 20, ty, tx + 20, ty, TFT_WHITE);
        tft.drawLine(tx, ty - 20, tx, ty + 20, TFT_WHITE);

        // Draw text instruction
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(50, 110);
        tft.printf("Click on");
        tft.setCursor(60, 135);
        tft.printf("%s", cal_labels[i]);

        Serial.printf("Please click on %s (%u, %u)...\n", cal_labels[i], tx, ty);

        bool touched = false;
        unsigned long timeout = millis() + 10000; // 10 second timeout
        while (!touched && millis() < timeout) {
            uint16_t x, y;
            uint8_t gesture;
            if (touch.getTouch(&x, &y, &gesture)) {
                raw_points[i].x = static_cast<double>(x);
                raw_points[i].y = static_cast<double>(y);
                Serial.printf("  Raw touch: (%u, %u)\n", x, y);
                touched = true;
                delay(300); // Debounce
            }
            delay(10);
        }

        if (!touched) {
            Serial.println("  Timeout! Calibration aborted, using default settings.");
            assignCalibration(defaultTouchCalibration(), cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, touch_calibrated);
            return;
        }
    }

    TouchCalibration solved;
    double max_residual = 0.0;
    if (solveAffineCalibration4Pt(raw_points, targets, solved, &max_residual)) {
        assignCalibration(solved, cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, touch_calibrated);
        Serial.println("4-point least-squares calibration solved successfully:");
        Serial.printf("  A=%.4f, B=%.4f, C=%.4f\n", cal_A, cal_B, cal_C);
        Serial.printf("  D=%.4f, E=%.4f, F=%.4f\n", cal_D, cal_E, cal_F);
        Serial.printf("  Max residual: %.2f px\n", max_residual);
        saveTouchCalibration(cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, true);
    } else {
        assignCalibration(defaultTouchCalibration(), cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, touch_calibrated);
        Serial.printf("Calibration failed (degenerate or residual %.2f px). Using default rotation.\n", max_residual);
    }

    Serial.println("Calibration complete!\n");
    delay(500);
}

void triggerTouchCalibration() {
    Preferences prefs;
    if (prefs.begin("touch_cal", false)) {
        prefs.putBool("valid", false);
        prefs.putBool("use_default", false);
        prefs.end();
    }

    Serial.println("Calibration cleared. Rebooting to calibrate...");
    delay(500);
    ESP.restart();
}
