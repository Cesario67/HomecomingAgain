#include "core/Config.h"

#include "audio/AudioProbe.h"
#include "core/Strings.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>

namespace hca {

namespace {

Config g_config;

int ReadInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int fallback, int minValue,
            int maxValue) {
    const int value = static_cast<int>(GetPrivateProfileIntW(section, key, fallback, ini.c_str()));
    return std::clamp(value, minValue, maxValue);
}

float ReadFloat(const std::wstring& ini, const wchar_t* section, const wchar_t* key, float fallback, float minValue,
                float maxValue) {
    wchar_t text[64]{};
    GetPrivateProfileStringW(section, key, L"", text, 64, ini.c_str());
    wchar_t* end = nullptr;
    const float value = std::wcstof(text, &end);
    if (end == text) {
        return fallback;
    }
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
    config.ssaoRadius = ReadFloat(iniPath, L"Graphics", L"SsaoRadius", defaults.ssaoRadius, 0.05f, 100.0f);
    config.ssaoStrength = ReadInt(iniPath, L"Graphics", L"SsaoStrength", defaults.ssaoStrength, 10, 400);
    config.ssaoFadeStart = ReadFloat(iniPath, L"Graphics", L"SsaoFadeStart", defaults.ssaoFadeStart, 0.0f, 100000.0f);
    config.ssaoFadeEnd = ReadFloat(iniPath, L"Graphics", L"SsaoFadeEnd", defaults.ssaoFadeEnd, 1.0f, 100000.0f);
    config.ssaoDebug = ReadBool(iniPath, L"Graphics", L"SsaoDebug", defaults.ssaoDebug);
    config.filmGrain = ReadInt(iniPath, L"Graphics", L"FilmGrain", defaults.filmGrain, 0, 100);
    config.fullScreenUiClip = ReadBool(iniPath, L"Graphics", L"FullScreenUiClip", defaults.fullScreenUiClip);
    config.objectViewPerspective =
        ReadBool(iniPath, L"Graphics", L"ObjectViewPerspective", defaults.objectViewPerspective);
    config.d3d9On12 = ReadBool(iniPath, L"Graphics", L"D3D9On12", defaults.d3d9On12);

    config.frameStats = ReadBool(iniPath, L"Frames", L"FrameStats", defaults.frameStats);
    config.reportIntervalSeconds =
        ReadInt(iniPath, L"Frames", L"ReportIntervalSeconds", defaults.reportIntervalSeconds, 1, 3600);
    config.timerResolutionMs = ReadInt(iniPath, L"Frames", L"TimerResolutionMs", defaults.timerResolutionMs, 0, 15);
    config.frameLimitFps = ReadInt(iniPath, L"Frames", L"FrameLimitFps", defaults.frameLimitFps, 0, 1000);
    config.targetFps = ReadInt(iniPath, L"Frames", L"TargetFps", defaults.targetFps, 0, 1000);

    config.windowThreadLock = ReadBool(iniPath, L"Compat", L"WindowThreadLock", defaults.windowThreadLock);
    config.d3dMultithreaded = ReadBool(iniPath, L"Compat", L"D3DMultithreaded", defaults.d3dMultithreaded);

    config.audioChannels = ReadInt(iniPath, L"Audio", L"Channels", defaults.audioChannels, 0, 8);

    config.audioChannelFix = ReadBool(iniPath, L"Audio", L"ChannelFix", defaults.audioChannelFix);

    {

        char order[64]{};

        GetPrivateProfileStringA("Audio", "ChannelOrder", "", order, sizeof(order), WideToUtf8(iniPath).c_str());

        int parsed[6]{};

        if (audio::ParseChannelOrder(order, parsed)) {

            std::copy(parsed, parsed + 6, config.audioChannelOrder);

        }

    }

    config.dlssShareFrames = ReadBool(iniPath, L"DLSS", L"ShareFrames", defaults.dlssShareFrames);
    config.dlssJitter = ReadFloat(iniPath, L"DLSS", L"Jitter", defaults.dlssJitter, 0.0f, 16.0f);
    config.dlssApply = ReadBool(iniPath, L"DLSS", L"Apply", defaults.dlssApply);

    config.movieSkipKey = ReadInt(iniPath, L"Movies", L"SkipKey", defaults.movieSkipKey, 0, 254);
    config.skipLogos = ReadBool(iniPath, L"Movies", L"SkipLogos", defaults.skipLogos);
    config.buttonIcons = ReadInt(iniPath, L"Input", L"ButtonIcons", defaults.buttonIcons, 0, 3);
    config.animatedIcons = ReadBool(iniPath, L"Input", L"AnimatedIcons", defaults.animatedIcons);
    config.playStationPad = ReadBool(iniPath, L"Input", L"PlayStationPad", defaults.playStationPad);

    config.cameraFix = ReadBool(iniPath, L"Camera", L"CameraFix", defaults.cameraFix);
    config.cameraHorizontalSpeed =
        ReadFloat(iniPath, L"Camera", L"HorizontalSpeed", defaults.cameraHorizontalSpeed, 0.25f, 3.0f);
    config.cameraVerticalSpeed =
        ReadFloat(iniPath, L"Camera", L"VerticalSpeed", defaults.cameraVerticalSpeed, 0.25f, 3.0f);
    config.cameraAimHorizontalSpeed =
        ReadFloat(iniPath, L"Camera", L"AimHorizontalSpeed", defaults.cameraAimHorizontalSpeed, 0.25f, 3.0f);
    config.cameraAimVerticalSpeed =
        ReadFloat(iniPath, L"Camera", L"AimVerticalSpeed", defaults.cameraAimVerticalSpeed, 0.25f, 3.0f);
    {
        wchar_t mode[32]{};
        GetPrivateProfileStringW(L"Camera", L"Mode", L"Follow", mode, 32, iniPath.c_str());
        config.cameraMode = patches::camera::ParseCameraMode(mode);
        config.cameraOrbitGuardFollow = ReadInt(iniPath, L"Camera", L"OrbitGuardFollow", 1, 0, 1) != 0;
    }
    config.cheatGod = ReadBool(iniPath, L"Cheats", L"God", defaults.cheatGod);
    config.cheatUnlimitedAmmo = ReadBool(iniPath, L"Cheats", L"UnlimitedAmmo", defaults.cheatUnlimitedAmmo);


    config.logKeepCount = ReadInt(iniPath, L"Debug", L"LogKeepCount", defaults.logKeepCount, 1, 500);
    config.frameCapture = ReadBool(iniPath, L"Debug", L"FrameCapture", defaults.frameCapture);
    config.frameCaptureKey = ReadInt(iniPath, L"Debug", L"FrameCaptureKey", defaults.frameCaptureKey, 1, 254);
    config.backBufferDump = ReadBool(iniPath, L"Debug", L"BackBufferDump", defaults.backBufferDump);
    config.skipMovies = ReadBool(iniPath, L"Debug", L"SkipMovies", defaults.skipMovies);
    config.inputLog = ReadBool(iniPath, L"Debug", L"InputLog", defaults.inputLog);

    g_config = config;
    return GetFileAttributesW(iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const Config& GetConfig() noexcept {
    return g_config;
}

}  // namespace hca
