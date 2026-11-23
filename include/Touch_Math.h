#ifndef TOUCH_MATH_H
#define TOUCH_MATH_H

#include <cstdint>

struct TouchCalibration {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    bool calibrated;
};

struct TouchPoint {
    double x;
    double y;
};

/**
 * @brief Returns default 90-degree landscape affine coefficients with calibrated = false.
 */
TouchCalibration defaultTouchCalibration();

/**
 * @brief Solves a 4-point least-squares affine calibration matrix and checks residual error.
 *
 * Rejects singular/collinear geometries (|det| < 1e-6), non-finite outputs, and fits
 * whose max Euclidean target residual exceeds @p max_allowed_residual, resetting @p out
 * to defaultTouchCalibration() on failure.
 */
bool solveAffineCalibration4Pt(const TouchPoint raw[4],
                               const TouchPoint target[4],
                               TouchCalibration &out,
                               double *out_max_residual = nullptr,
                               double max_allowed_residual = 25.0);

/**
 * @brief Maps raw touchscreen coordinates to clamped display coordinates.
 */
void applyTouchCalibration(const TouchCalibration &cal,
                           uint16_t raw_x,
                           uint16_t raw_y,
                           int16_t screen_w,
                           int16_t screen_h,
                           int16_t &out_x,
                           int16_t &out_y);

#endif /* TOUCH_MATH_H */
