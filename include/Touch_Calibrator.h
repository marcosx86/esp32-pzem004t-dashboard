#ifndef TOUCH_CALIBRATOR_H
#define TOUCH_CALIBRATOR_H

#include <Arduino.h>
#include "Display_LGFX.h"
#include "CST820.h"

// Load calibration parameters from Preferences (NVS)
// Returns true if calibration was successfully loaded, false otherwise
bool loadTouchCalibration(double &a, double &b, double &c, double &d, double &e, double &f, bool &calibrated);

// Save calibration parameters to Preferences (NVS)
void saveTouchCalibration(double a, double b, double c, double d, double e, double f, bool calibrated);

// Run 4-point visual calibration routine, solves the 3-point affine transformation matrix, and saves to NVS
void runTouchCalibration(LGFX &tft, CST820 &touch, double &cal_A, double &cal_B, double &cal_C, double &cal_D, double &cal_E, double &cal_F, bool &touch_calibrated);

#endif // TOUCH_CALIBRATOR_H
