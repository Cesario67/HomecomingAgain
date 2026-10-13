// Point d'entrée de HomecomingAgain.
//
// DllMain s'exécute sous le verrou du chargeur de Windows, avant le code du jeu : on n'y fait que des
// opérations sûres (fichiers, écriture de pointeurs dans l'IAT de SilentHill.exe). Ce qui demande de charger
// d'autres DLL est reporté au premier appel du jeu à Direct3D (voir graphics/D3D9Hooks.cpp).

#include <windows.h>

#include <string_view>

#include "audio/AudioProbe.h"
#include "compat/MovieSkip.h"
#include "input/ButtonIcons.h"
#include "input/DirectInputHooks.h"
#include "input/TextureNameProbe.h"
#include "input/XInputProbe.h"
#include "compat/SkipMovies.h"
#include "compat/WindowThreadLock.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"
#include "core/SystemInfo.h"
#include "crash/CrashHandler.h"
#include "framepacing/FrameMonitor.h"
#include "framepacing/WaitProfiler.h"
#include "graphics/D3D9Hooks.h"
#include "patches/FrameRateCap.h"

namespace {

bool g_active = false;

bool IsHostedByGame() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring_view name(path);
    const size_t separator = name.find_last_of(L"\\/");
    if (separator != std::wstring_view::npos) {
        name.remove_prefix(separator + 1);
    }
    // Nom de l'exécutable d'après les témoignages (correctif communautaire de 2018) : à confirmer.
    return hca::EqualsIgnoreCase(name, L"SilentHill.exe");
}

void Startup() {
    // Chargée par un autre programme : on ne fait rien.
    if (!IsHostedByGame() || !hca::InitPaths()) {
        return;
    }
    const hca::Paths& paths = hca::GetPaths();
    const bool iniFound = hca::LoadConfig(paths.iniFile);
    if (!hca::log::Open(paths.logsDir, hca::GetConfig().logKeepCount)) {
        return;
    }
    g_active = true;

    hca::log::Info("Homecoming Again {} : démarrage", HCA_VERSION);
    if (!iniFound) {
        hca::log::Warn("{} absent : valeurs par défaut utilisées", hca::WideToUtf8(paths.iniFile));
    }

    const HMODULE game = GetModuleHandleW(nullptr);
    // Le moteur est une DLL (g_SilentHill.sgl, base 0x10000000) importée par le lanceur SilentHill.exe. Notre
    // version.dll est la première importée par le lanceur : le moteur est déjà mappé et ses imports sont déjà
    // résolus, mais il n'a pas encore démarré. On peut donc modifier son IAT ici (hypothèse à confirmer par le
    // journal au premier lancement).
    const HMODULE engine = GetModuleHandleW(L"g_SilentHill.sgl");
    if (engine == nullptr) {
        hca::log::Warn("g_SilentHill.sgl pas encore chargé : Direct3D, entrées et attentes du moteur non interceptés");
    }
    hca::sysinfo::LogStartupInfo(game);
    hca::crash::Install(game, engine);
    hca::frames::Install(game, engine);
    if (engine != nullptr) {
        hca::graphics::Install(engine);
        hca::patches::ApplyFrameRateCap(engine);
        hca::compat::InstallWindowThreadLock(engine);
        hca::compat::InstallSkipMovies(engine);
        hca::compat::InstallMovieSkip(engine);
        hca::audio::InstallAudioProbe(engine);
        hca::input::InstallXInputProbe(engine);
        hca::input::InstallDirectInputHooks(engine);
        hca::input::InstallButtonIcons(engine);
        hca::input::InstallTextureNameProbe(engine);
        if (hca::GetConfig().frameStats) {
            hca::frames::waits::Install(engine);
        }
    }
    hca::log::Info("Initialisation terminée");
}

void Shutdown(bool processTerminating) {
    if (!g_active) {
        return;
    }
    if (processTerminating) {
        hca::frames::LogFinalReport();
        hca::log::Info("Fermeture normale du jeu");
    }
    hca::log::Close();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            try {
                Startup();
            } catch (...) {
                // Une erreur d'initialisation ne doit pas empêcher le jeu de démarrer.
            }
            break;
        case DLL_PROCESS_DETACH:
            try {
                Shutdown(reserved != nullptr);
            } catch (...) {
            }
            break;
        default:
            break;
    }
    return TRUE;
}
