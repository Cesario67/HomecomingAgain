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

namespace {

// Position de la caméra : le point dont l'image a x = y = w = 0, soit inverse(caméra) x (0, 0, 1, 0) ramené en
// coordonnées homogènes.
bool CameraPosition(const Matrix4& m, double (&position)[3]) noexcept {
    Matrix4 inverse{};
    if (!Inverse(m, inverse) || std::fabs(inverse[14]) < 1e-12f) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        position[i] = static_cast<double>(inverse[i * 4 + 2]) / inverse[14];
    }
    return true;
}

}  // namespace

bool IsCameraCut(const Matrix4& current, const Matrix4& previous, float maxDegrees, float maxDistance) noexcept {
    double a[3]{};
    double b[3]{};
    if (!CameraPosition(current, a) || !CameraPosition(previous, b)) {
        return true;
    }
    const double distance =
        std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
    // Direction du regard : ligne 3 (w = profondeur), normalisée.
    const double lengthA = std::sqrt(static_cast<double>(current[12]) * current[12] + current[13] * current[13] +
                                     current[14] * current[14]);
    const double lengthB = std::sqrt(static_cast<double>(previous[12]) * previous[12] + previous[13] * previous[13] +
                                     previous[14] * previous[14]);
    if (lengthA < 1e-9 || lengthB < 1e-9) {
        return true;
    }
    const double cosine = (static_cast<double>(current[12]) * previous[12] + current[13] * previous[13] +
                           current[14] * previous[14]) /
                          (lengthA * lengthB);
    const double degrees = std::acos(std::fmin(1.0, std::fmax(-1.0, cosine))) * 57.29577951308232;
    return degrees > maxDegrees || distance > maxDistance;
}

}  // namespace hca::graphics
