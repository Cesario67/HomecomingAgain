#include "patches/CameraTuning.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hca::patches::camera {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kRadiansToDegrees = 57.295780181884766f;

}  // namespace

float ClampSpeed(float value) noexcept {
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return std::clamp(value, kMinSpeed, kMaxSpeed);
}

CameraGains ComputeGains(const CameraSpeeds& speeds) noexcept {
    const double vertical = kVerticalReferenceGain * kVerticalUserCalibration * ClampSpeed(speeds.vertical);
    CameraGains gains{};
    gains.yawRadiansPerSecond = kHorizontalReference * ClampSpeed(speeds.horizontal);
    // 60 degrés par seconde : la constante du moteur que le correctif remplace (voir CameraTuning.h).
    gains.padPitchGain = 60.0 * vertical;
    gains.pitchGain = static_cast<float>(vertical);
    gains.aimInputGain = kAimInputGain;
    gains.aimOutputX = ClampSpeed(speeds.aimHorizontal) / kAimOutputDivisor;
    gains.aimOutputY = ClampSpeed(speeds.aimVertical) / kAimOutputDivisor;
    return gains;
}

bool IsStickDeflected(float axis) noexcept {
    return axis * axis > kStickDeadzone * kStickDeadzone;
}

bool IsRawStickDeflected(short axis) noexcept {
    return IsStickDeflected(static_cast<float>(axis) / 32767.0f);
}

float TurnDegrees(float stickX, float elapsedSeconds, float yawRadiansPerSecond) noexcept {
    return -(stickX * elapsedSeconds * yawRadiansPerSecond) * kRadiansToDegrees;
}

float HeadingFromForward(float forwardX, float forwardZ) noexcept {
    const float heading = std::atan2(forwardZ, forwardX);
    return heading < 0.0f ? heading + kTwoPi : heading;
}

bool IsPrecisionCameraName(const char* name) noexcept {
    // Zéro final compris, comme la comparaison du Camera Fix ; strncmp s'arrête au premier zéro du nom lu.
    return name != nullptr && std::strncmp(name, "Precision", 10) == 0;
}

}  // namespace hca::patches::camera
