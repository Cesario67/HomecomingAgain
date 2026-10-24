#include "input/InputLog.h"

#include <Xinput.h>

#include <atomic>
#include <mutex>
#include <set>
#include <string>

#include "core/Config.h"
#include "core/Log.h"
#include "input/EngineUiState.h"
#include "input/MenuBridge.h"

namespace hca::input {

namespace {

constexpr int kMaxLines = 4000;
std::atomic<int> g_lines{0};
std::atomic<WORD> g_lastButtons{0};
std::atomic<int> g_lastState{-2};
std::atomic<int> g_lastPadFlag{-2};
std::atomic<bool> g_lastMenu{false};
std::atomic<bool> g_lastScene{false};
bool g_lastReturn = false;
bool g_lastEscape = false;

bool Enabled() noexcept {
    return GetConfig().inputLog && g_lines.load() < kMaxLines;
}

}  // namespace

void LogPadButtons(WORD buttons) noexcept {
    const WORD previous = g_lastButtons.exchange(buttons);
    if (previous == buttons || !Enabled()) {
        return;
    }
    static constexpr struct {
        WORD mask;
        const char* name;
    } kButtons[] = {{XINPUT_GAMEPAD_A, "A"},     {XINPUT_GAMEPAD_B, "B"},         {XINPUT_GAMEPAD_X, "X"},
                    {XINPUT_GAMEPAD_Y, "Y"},     {XINPUT_GAMEPAD_START, "Start"}, {XINPUT_GAMEPAD_BACK, "Back"},
                    {XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"}};
    try {
        std::string text;
        for (const auto& button : kButtons) {
            const bool now = (buttons & button.mask) != 0;
            const bool before = (previous & button.mask) != 0;
            if (now != before) {
                text += text.empty() ? "" : ", ";
                text += std::string(button.name) + (now ? " appuyé" : " relâché");
            }
        }
        if (!text.empty()) {
            ++g_lines;
            log::Info("Entrées : manette {} (état moteur {}, mode manette {}, traduction {})", text, EngineUiState(),
                      EnginePadFlag(), InMenu() ? "active" : "arrêtée");
        }
    } catch (...) {
    }
}

void LogKeyboardRead(const BYTE* state, const std::array<bool, 256>& injected) noexcept {
    if (state == nullptr || !Enabled()) {
        return;
    }
    const bool returnDown = (state[0x1C] & 0x80) != 0;
    const bool escapeDown = (state[0x01] & 0x80) != 0;
    if (returnDown != g_lastReturn) {
        ++g_lines;
        log::Info("Entrées : Entrée {} pour le moteur ({})", returnDown ? "enfoncée" : "relâchée",
                  injected[0x1C] ? "ajoutée par le mod" : "vrai clavier");
    }
    if (escapeDown != g_lastEscape) {
        ++g_lines;
        log::Info("Entrées : Échap {} pour le moteur ({})", escapeDown ? "enfoncée" : "relâchée",
                  injected[0x01] ? "ajoutée par le mod" : "vrai clavier");
    }
    g_lastReturn = returnDown;
    g_lastEscape = escapeDown;
}

void LogDirectInputDevice(REFGUID guid, HRESULT result) noexcept {
    if (!GetConfig().inputLog) {
        return;
    }
    log::Info("Entrées : le jeu crée un périphérique DirectInput {{{:08X}-{:04X}-{:04X}}} -> 0x{:08X}", guid.Data1,
              guid.Data2, guid.Data3, static_cast<unsigned long>(result));
}

void LogEngineState() noexcept {
    if (!Enabled()) {
        return;
    }
    const int state = EngineUiState();
    const int padFlag = EnginePadFlag();
    const bool menu = InMenu();
    const std::uint64_t sceneAge = SceneAgeMs();
    const bool scene = sceneAge != 0 && sceneAge <= kMenuDelayMs;
    if (g_lastState.exchange(state) != state || g_lastPadFlag.exchange(padFlag) != padFlag ||
        g_lastMenu.exchange(menu) != menu || g_lastScene.exchange(scene) != scene) {
        ++g_lines;
        log::Info("Entrées : état moteur {}, mode manette {}, scène 3D {} (dernière il y a {} ms), traduction {}", state,
                  padFlag, scene ? "vue" : "absente", sceneAge, menu ? "active" : "arrêtée");
    }
}

}  // namespace hca::input

namespace hca::input {

namespace {

std::mutex g_iconLock;
std::set<std::string> g_iconSeen;

bool FirstTime(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_iconLock);
    return g_iconSeen.size() < 400 && g_iconSeen.insert(key).second;
}

}  // namespace

void LogIconRequest(unsigned command, const char* engineName, const char* shownName, bool padMode) noexcept {
    if (!GetConfig().inputLog) {
        return;
    }
    try {
        const std::string engine = engineName != nullptr ? engineName : "aucune";
        const std::string shown = shownName != nullptr ? shownName : "aucune";
        if (FirstTime("cmd|" + std::to_string(command) + "|" + engine + "|" + shown + (padMode ? "|1" : "|0"))) {
            log::Info("Icônes : commande 0x{:02X} -> moteur {}, affichée {} (mode manette {})", command, engine, shown,
                      padMode);
        }
    } catch (...) {
    }
}

void LogTextureApplied(const char* name, const char* replacement) noexcept {
    if (!GetConfig().inputLog || name == nullptr) {
        return;
    }
    try {
        const std::string text = name;
        if (text.find("button") == std::string::npos && text.find("flash") == std::string::npos &&
            text.find("mouse") == std::string::npos && text.find("frustration") == std::string::npos) {
            return;
        }
        if (FirstTime("tex|" + text)) {
            log::Info("Icônes : texture « {} » appliquée{}{}", text, replacement != nullptr ? ", remplacée par " : "",
                      replacement != nullptr ? replacement : "");
        }
    } catch (...) {
    }
}

}  // namespace hca::input
