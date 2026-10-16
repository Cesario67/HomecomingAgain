#include "input/ControllerNames.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Paths.h"

namespace hca::input {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
constexpr std::uintptr_t kAxisNames = 0x111C04C0;    // 6 pointeurs (« Left X Axis »...)
constexpr std::uintptr_t kButtonNames = 0x111C0568;  // 32 pointeurs (« Button 0 »...)
constexpr int kAxes = 6;
constexpr int kButtons = 32;

const char* g_originalAxes[kAxes]{};
const char* g_originalButtons[kButtons]{};
bool g_installed = false;
bool g_french = false;

// Noms d'origine attendus, vérifiés avant toute modification.
constexpr const char* kExpectedAxes[kAxes] = {"Left X Axis",  "Left Y Axis",  "Left Z Axis",
                                              "Right X Axis", "Right Y Axis", "Right Z Axis"};

bool SafeRead(std::uintptr_t address, const char*& value) noexcept {
    __try {
        value = *reinterpret_cast<const char* const*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeEquals(const char* text, const char* expected) noexcept {
    __try {
        return text != nullptr && std::strcmp(text, expected) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadManifestFrench() {
    try {
        // Bin -> Silent Hill Homecoming -> common -> steamapps.
        const std::filesystem::path manifest =
            std::filesystem::path(GetPaths().gameDir).parent_path().parent_path().parent_path() / "appmanifest_19000.acf";
        std::ifstream file(manifest);
        if (!file) {
            return false;
        }
        std::ostringstream text;
        text << file.rdbuf();
        return ManifestIsFrench(text.str());
    } catch (...) {
        return false;
    }
}

void WriteName(std::uintptr_t slot, const char* name) noexcept {
    hooking::WriteCode(reinterpret_cast<void*>(slot), &name, sizeof(name));
}

}  // namespace

const char* ControllerInputLabel(bool axis, int index, int type, bool french) noexcept {
    if (type != 1 && type != 2) {
        return nullptr;
    }
    if (axis) {
        switch (index) {
            case 0: return french ? "Stick gauche horizontal" : "Left stick (horizontal)";
            case 1: return french ? "Stick gauche vertical" : "Left stick (vertical)";
            case 3: return french ? "Stick droit horizontal" : "Right stick (horizontal)";
            case 4: return french ? "Stick droit vertical" : "Right stick (vertical)";
            default: return nullptr;
        }
    }
    const bool xbox = type == 1;
    switch (index) {
        case 0: return xbox ? "A" : (french ? "Croix" : "Cross");
        case 1: return xbox ? "B" : (french ? "Rond" : "Circle");
        case 2: return xbox ? "X" : (french ? "Carr\xE9" : "Square");
        case 3: return xbox ? "Y" : "Triangle";
        case 6: return xbox ? "LB" : "L1";
        case 7: return xbox ? "RB" : "R1";
        case 8: return "Start";
        case 9: return xbox ? "Back" : "Select";
        case 10: return xbox ? (french ? "Clic stick gauche" : "Left stick click") : "L3";
        case 11: return xbox ? (french ? "Clic stick droit" : "Right stick click") : "R3";
        case 12: return french ? "Croix haut" : "D-pad up";
        case 13: return french ? "Croix bas" : "D-pad down";
        case 14: return french ? "Croix gauche" : "D-pad left";
        case 15: return french ? "Croix droite" : "D-pad right";
        case 16: return xbox ? "LT" : "L2";
        case 17: return xbox ? "RT" : "R2";
        default: return nullptr;
    }
}

bool ManifestIsFrench(std::string_view manifest) noexcept {
    constexpr std::string_view kKey = "\"language\"";
    const std::size_t key = manifest.find(kKey);
    if (key == std::string_view::npos) {
        return false;
    }
    const std::size_t open = manifest.find('"', key + kKey.size());
    const std::size_t close = open == std::string_view::npos ? open : manifest.find('"', open + 1);
    if (close == std::string_view::npos) {
        return false;
    }
    const std::string_view value = manifest.substr(open + 1, close - open - 1);
    constexpr std::string_view kFrench = "french";
    if (value.size() != kFrench.size()) {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i] >= 'A' && value[i] <= 'Z' ? static_cast<char>(value[i] - 'A' + 'a') : value[i];
        if (c != kFrench[i]) {
            return false;
        }
    }
    return true;
}

void ApplyControllerNames(int type) noexcept {
    if (!g_installed) {
        return;
    }
    for (int i = 0; i < kAxes; ++i) {
        const char* label = ControllerInputLabel(true, i, type, g_french);
        WriteName(kAxisNames + 4 * i, label != nullptr ? label : g_originalAxes[i]);
    }
    for (int i = 0; i < kButtons; ++i) {
        const char* label = ControllerInputLabel(false, i, type, g_french);
        WriteName(kButtonNames + 4 * i, label != nullptr ? label : g_originalButtons[i]);
    }
}

void InstallControllerNames(HMODULE engine) {
    if (reinterpret_cast<std::uintptr_t>(engine) != kExpectedEngineBase) {
        return;
    }
    bool ok = true;
    for (int i = 0; i < kAxes && ok; ++i) {
        ok = SafeRead(kAxisNames + 4 * i, g_originalAxes[i]) && SafeEquals(g_originalAxes[i], kExpectedAxes[i]);
    }
    for (int i = 0; i < kButtons && ok; ++i) {
        const std::string expected = "Button " + std::to_string(i);
        ok = SafeRead(kButtonNames + 4 * i, g_originalButtons[i]) && SafeEquals(g_originalButtons[i], expected.c_str());
    }
    if (!ok) {
        log::Warn("Noms des boutons de la manette : table du moteur différente de celle attendue, non modifiée");
        return;
    }
    g_french = ReadManifestFrench();
    g_installed = true;
    log::Info("Noms des boutons de la manette : prêts ({})", g_french ? "français" : "anglais");
}

}  // namespace hca::input
