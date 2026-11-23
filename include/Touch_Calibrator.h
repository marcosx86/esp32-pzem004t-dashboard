#ifndef TOUCH_CALIBRATOR_H
#define TOUCH_CALIBRATOR_H

#include <Arduino.h>
#include "Display_LGFX.h"
#include "CST820.h"
#include "Touch_Math.h"

// Load calibration parameters (or use_default flag) from Preferences (NVS).
// Returns true if custom calibration OR explicit use_default is stored; false if calibration wizard should run.
bool loadTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated);

// Save calibration parameters to Preferences (NVS)
void saveTouchCalibration(double a, double b, double c, double d, double e, double f, bool calibrated);

// Reset touch calibration to default 90-degree rotation immediately (no reboot) and persist use_default=true.
void resetTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated);

// Run 4-point visual calibration routine, solve 4-point least-squares affine matrix, and save to NVS
void runTouchCalibration(LGFX &tft, CST820 &touch, double &cal_A, double &cal_B, double &cal_C, double &cal_D, double &cal_E, double &cal_F, bool &touch_calibrated);

// Clear NVS calibration + use_default and restart ESP32 to trigger calibration on next boot
void triggerTouchCalibration();

#endif // TOUCH_CALIBRATOR_H
