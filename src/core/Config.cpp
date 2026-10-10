#include "core/Config.h"

#include <windows.h>

#include <algorithm>

namespace shhf {

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

    config.borderless = ReadBool(iniPath, L"Window", L"Borderless", defaults.borderless);

    config.frameStats = ReadBool(iniPath, L"Frames", L"FrameStats", defaults.frameStats);
    config.reportIntervalSeconds =
        ReadInt(iniPath, L"Frames", L"ReportIntervalSeconds", defaults.reportIntervalSeconds, 1, 3600);
    config.timerResolutionMs = ReadInt(iniPath, L"Frames", L"TimerResolutionMs", defaults.timerResolutionMs, 0, 15);
    config.frameLimitFps = ReadInt(iniPath, L"Frames", L"FrameLimitFps", defaults.frameLimitFps, 0, 1000);

    config.logKeepCount = ReadInt(iniPath, L"Debug", L"LogKeepCount", defaults.logKeepCount, 1, 500);

    g_config = config;
    return GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Config& GetConfig() noexcept {
    return g_config;
}

}  // namespace shhf
