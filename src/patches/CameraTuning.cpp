#include "patches/CameraTuning.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>

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

CameraMode ParseCameraMode(const wchar_t* text) noexcept {
    return text != nullptr && _wcsicmp(text, L"Orbit") == 0 ? CameraMode::Orbit : CameraMode::Follow;
}

float WrapHeading(float radians) noexcept {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    const float wrapped = std::fmod(radians, kTwoPi);
    return wrapped < 0.0f ? wrapped + kTwoPi : wrapped;
}

float OrbitYaw(float yaw, float stickX, float elapsedSeconds, float yawRadiansPerSecond) noexcept {
    if (!IsStickDeflected(stickX)) {
        return WrapHeading(yaw);
    }
    return WrapHeading(yaw - stickX * elapsedSeconds * yawRadiansPerSecond);
}

bool OrbitNeedsReset(unsigned long long lastUpdateMs, unsigned long long nowMs) noexcept {
    return lastUpdateMs == 0 || nowMs < lastUpdateMs || nowMs - lastUpdateMs > kOrbitResetMs;
}

OrbitMove ParseOrbitMove(const wchar_t* text) noexcept {
    return text != nullptr && _wcsicmp(text, L"Stick") == 0 ? OrbitMove::Stick : OrbitMove::Camera;
}

float SignedAngle(float radians) noexcept {
    const float wrapped = WrapHeading(radians);
    return wrapped >= kTwoPi / 2.0f ? wrapped - kTwoPi : wrapped;
}

float StickMagnitude(float forward, float side) noexcept {
    const float magnitude = std::sqrt(forward * forward + side * side);
    return std::isfinite(magnitude) ? std::min(magnitude, 1.0f) : 0.0f;
}

float CameraRelativeHeading(float forward, float side, float cameraYaw) noexcept {
    return WrapHeading(cameraYaw - std::atan2(side, forward));
}

float TurnStep(float error, float maxStep) noexcept {
    return std::clamp(error, -maxStep, maxStep);
}

float ForwardAfterTurn(float magnitude, float remainingError) noexcept {
    return magnitude * std::max(0.0f, std::cos(remainingError));
}

bool IsPrecisionCameraName(const char* name) noexcept {
    // Zéro final compris, comme la comparaison du Camera Fix ; strncmp s'arrête au premier zéro du nom lu.
    return name != nullptr && std::strncmp(name, "Precision", 10) == 0;
}

}  // namespace hca::patches::camera
