#include "graphics/CameraMath.h"

#include <cmath>

namespace hca::graphics {

Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept {
    Matrix4 r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a[row * 4 + k] * b[k * 4 + col];
            }
            r[row * 4 + col] = sum;
        }
    }
    return r;
}

bool Inverse(const Matrix4& m, Matrix4& result) noexcept {
    // Élimination de Gauss-Jordan avec pivot partiel, en double pour la précision (matrices de projection).
    double a[4][8]{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            a[r][c] = m[r * 4 + c];
        }
        a[r][4 + r] = 1.0;
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int r = col + 1; r < 4; ++r) {
            if (std::fabs(a[r][col]) > std::fabs(a[pivot][col])) {
                pivot = r;
            }
        }
        if (std::fabs(a[pivot][col]) < 1e-12) {
            return false;
        }
        if (pivot != col) {
            for (int c = 0; c < 8; ++c) {
                const double t = a[col][c];
                a[col][c] = a[pivot][c];
                a[pivot][c] = t;
            }
        }
        const double inv = 1.0 / a[col][col];
        for (int c = 0; c < 8; ++c) {
            a[col][c] *= inv;
        }
        for (int r = 0; r < 4; ++r) {
            if (r != col) {
                const double f = a[r][col];
                for (int c = 0; c < 8; ++c) {
                    a[r][c] -= f * a[col][c];
                }
            }
        }
    }
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result[r * 4 + c] = static_cast<float>(a[r][4 + c]);
        }
    }
    return true;
}

bool ViewProjectionFromDraw(const float* worldViewProj, const float* world3x4, Matrix4& viewProjection) noexcept {
    if (worldViewProj == nullptr || world3x4 == nullptr) {
        return false;
    }
    Matrix4 wvp{};
    Matrix4 world{};
    for (int i = 0; i < 16; ++i) {
        wvp[i] = worldViewProj[i];
    }
    for (int i = 0; i < 12; ++i) {
        world[i] = world3x4[i];
    }
    world[12] = 0.0f;
    world[13] = 0.0f;
    world[14] = 0.0f;
    world[15] = 1.0f;
    Matrix4 inverseWorld{};
    if (!Inverse(world, inverseWorld)) {
        return false;
    }
    viewProjection = Multiply(wvp, inverseWorld);
    return true;
}

}  // namespace hca::graphics
