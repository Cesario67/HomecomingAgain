#include "core/Config.h"

#include <windows.h>

#include <algorithm>

namespace hca {

namespace {

Config g_config;

int ReadInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int fallback, int minValue,
            int maxValue) {
    const int value = static_cast<int>(GetPrivateProfileIntW(section, key, fallback, ini.c_str()));
    return std::clamp(value, minValue, maxValue);
}

bool ReadBool(const std::wstring& ini, const wchar_t* section, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, ini.c_str()) != 0;
}

}  // namespace

bool LoadConfig(const std::wstring& iniPath) {
    const Config defaults;
    Config config;

    config.crashDumps = ReadBool(iniPath, L"Crash", L"CrashDumps", defaults.crashDumps);
    config.fullMemoryDumps = ReadBool(iniPath, L"Crash", L"FullMemoryDumps", defaults.fullMemoryDumps);
    config.dumpKeepCount = ReadInt(iniPath, L"Crash", L"DumpKeepCount", defaults.dumpKeepCount, 1, 100);
    config.hangSeconds = ReadInt(iniPath, L"Crash", L"HangSeconds", defaults.hangSeconds, 0, 60);

    config.borderless = ReadBool(iniPath, L"Window", L"Borderless", defaults.borderless);

    config.smaa = ReadBool(iniPath, L"Graphics", L"SMAA", defaults.smaa);
    config.anisotropy = ReadInt(iniPath, L"Graphics", L"Anisotropy", defaults.anisotropy, 0, 16);
    config.shadowResolution =
        ReadInt(iniPath, L"Graphics", L"ShadowResolution", defaults.shadowResolution, 2048, 8192);
    config.ssao = ReadBool(iniPath, L"Graphics", L"SSAO", defaults.ssao);
    config.ssaoRadius = ReadInt(iniPath, L"Graphics", L"SsaoRadius", defaults.ssaoRadius, 1, 1000);
    config.ssaoStrength = ReadInt(iniPath, L"Graphics", L"SsaoStrength", defaults.ssaoStrength, 10, 400);
    config.ssaoFadeStart = ReadInt(iniPath, L"Graphics", L"SsaoFadeStart", defaults.ssaoFadeStart, 0, 100000);
    config.ssaoFadeEnd = ReadInt(iniPath, L"Graphics", L"SsaoFadeEnd", defaults.ssaoFadeEnd, 1, 100000);
    config.ssaoDebug = ReadBool(iniPath, L"Graphics", L"SsaoDebug", defaults.ssaoDebug);

    config.frameStats = ReadBool(iniPath, L"Frames", L"FrameStats", defaults.frameStats);
    config.reportIntervalSeconds =
        ReadInt(iniPath, L"Frames", L"ReportIntervalSeconds", defaults.reportIntervalSeconds, 1, 3600);
    config.timerResolutionMs = ReadInt(iniPath, L"Frames", L"TimerResolutionMs", defaults.timerResolutionMs, 0, 15);
    config.frameLimitFps = ReadInt(iniPath, L"Frames", L"FrameLimitFps", defaults.frameLimitFps, 0, 1000);
    config.targetFps = ReadInt(iniPath, L"Frames", L"TargetFps", defaults.targetFps, 0, 1000);

    config.windowThreadLock = ReadBool(iniPath, L"Compat", L"WindowThreadLock", defaults.windowThreadLock);
    config.d3dMultithreaded = ReadBool(iniPath, L"Compat", L"D3DMultithreaded", defaults.d3dMultithreaded);

    config.logKeepCount = ReadInt(iniPath, L"Debug", L"LogKeepCount", defaults.logKeepCount, 1, 500);
    config.frameCapture = ReadBool(iniPath, L"Debug", L"FrameCapture", defaults.frameCapture);
    config.frameCaptureKey = ReadInt(iniPath, L"Debug", L"FrameCaptureKey", defaults.frameCaptureKey, 1, 254);

    g_config = config;
    return GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Config& GetConfig() noexcept {
    return g_config;
}

}  // namespace hca
