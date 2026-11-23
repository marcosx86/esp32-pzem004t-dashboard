#include "Touch_Math.h"

#include <cmath>

namespace {

double det3(const double m[3][3]) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
           m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

void replaceColumn(const double m[3][3], int col, const double rhs[3], double out[3][3]) {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out[r][c] = (c == col) ? rhs[r] : m[r][c];
        }
    }
}

int16_t clampCoord(int32_t value, int16_t limit) {
    if (limit <= 0) {
        return 0;
    }
    if (value < 0) {
        return 0;
    }
    if (value >= limit) {
        return static_cast<int16_t>(limit - 1);
    }
    return static_cast<int16_t>(value);
}

} // namespace

TouchCalibration defaultTouchCalibration() {
    TouchCalibration cal;
    cal.a = 0.0;
    cal.b = 1.0;
    cal.c = 0.0;
    cal.d = -1.0;
    cal.e = 0.0;
    cal.f = 240.0;
    cal.calibrated = false;
    return cal;
}

bool solveAffineCalibration4Pt(const TouchPoint raw[4],
                               const TouchPoint target[4],
                               TouchCalibration &out,
                               double *out_max_residual,
                               double max_allowed_residual) {
    if (!raw || !target) {
        out = defaultTouchCalibration();
        return false;
    }

    double s_xx = 0.0, s_xy = 0.0, s_yy = 0.0;
    double s_x = 0.0, s_y = 0.0;
    double rhs_x[3] = {0.0, 0.0, 0.0};
    double rhs_y[3] = {0.0, 0.0, 0.0};

    for (int i = 0; i < 4; ++i) {
        double xr = raw[i].x;
        double yr = raw[i].y;
        double xs = target[i].x;
        double ys = target[i].y;
        if (!std::isfinite(xr) || !std::isfinite(yr) || !std::isfinite(xs) || !std::isfinite(ys)) {
            out = defaultTouchCalibration();
            return false;
        }

        s_xx += xr * xr;
        s_xy += xr * yr;
        s_yy += yr * yr;
        s_x += xr;
        s_y += yr;

        rhs_x[0] += xr * xs;
        rhs_x[1] += yr * xs;
        rhs_x[2] += xs;

        rhs_y[0] += xr * ys;
        rhs_y[1] += yr * ys;
        rhs_y[2] += ys;
    }

    const double M[3][3] = {
        {s_xx, s_xy, s_x},
        {s_xy, s_yy, s_y},
        {s_x,  s_y,  4.0}
    };

    double det = det3(M);
    if (!std::isfinite(det) || std::abs(det) < 1e-6) {
        out = defaultTouchCalibration();
        return false;
    }

    double mx0[3][3], mx1[3][3], mx2[3][3];
    double my0[3][3], my1[3][3], my2[3][3];
    replaceColumn(M, 0, rhs_x, mx0);
    replaceColumn(M, 1, rhs_x, mx1);
    replaceColumn(M, 2, rhs_x, mx2);
    replaceColumn(M, 0, rhs_y, my0);
    replaceColumn(M, 1, rhs_y, my1);
    replaceColumn(M, 2, rhs_y, my2);

    TouchCalibration solved;
    solved.a = det3(mx0) / det;
    solved.b = det3(mx1) / det;
    solved.c = det3(mx2) / det;
    solved.d = det3(my0) / det;
    solved.e = det3(my1) / det;
    solved.f = det3(my2) / det;
    solved.calibrated = true;

    if (!std::isfinite(solved.a) || !std::isfinite(solved.b) || !std::isfinite(solved.c) ||
        !std::isfinite(solved.d) || !std::isfinite(solved.e) || !std::isfinite(solved.f)) {
        out = defaultTouchCalibration();
        return false;
    }

    double max_err = 0.0;
    for (int i = 0; i < 4; ++i) {
        double px = solved.a * raw[i].x + solved.b * raw[i].y + solved.c;
        double py = solved.d * raw[i].x + solved.e * raw[i].y + solved.f;
        double dx = px - target[i].x;
        double dy = py - target[i].y;
        double err = std::sqrt(dx * dx + dy * dy);
        if (err > max_err) {
            max_err = err;
        }
    }

    if (out_max_residual) {
        *out_max_residual = max_err;
    }

    if (!std::isfinite(max_err) || max_err > max_allowed_residual) {
        out = defaultTouchCalibration();
        return false;
    }

    out = solved;
    return true;
}

void applyTouchCalibration(const TouchCalibration &cal,
                           uint16_t raw_x,
                           uint16_t raw_y,
                           int16_t screen_w,
                           int16_t screen_h,
                           int16_t &out_x,
                           int16_t &out_y) {
    int32_t mapped_x = 0;
    int32_t mapped_y = 0;

    if (cal.calibrated) {
        mapped_x = static_cast<int32_t>(cal.a * raw_x + cal.b * raw_y + cal.c);
        mapped_y = static_cast<int32_t>(cal.d * raw_x + cal.e * raw_y + cal.f);
    } else {
        // Default 90-degree clockwise landscape mapping
        mapped_x = static_cast<int32_t>(raw_y);
        mapped_y = static_cast<int32_t>(screen_h) - static_cast<int32_t>(raw_x);
    }

    out_x = clampCoord(mapped_x, screen_w);
    out_y = clampCoord(mapped_y, screen_h);
}
