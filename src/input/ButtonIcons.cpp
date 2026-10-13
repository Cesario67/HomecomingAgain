#include "input/ButtonIcons.h"

#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"

namespace hca::input {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
// Fonction du moteur qui renvoie le type de manette (voir ButtonIcons.h).
constexpr std::uintptr_t kControllerTypeFunction = 0x104EC050;
// Octets d'origine (lus dans g_SilentHill.sgl le 13/10/2026) : mov eax,[111D2460h] ; mov ecx,[eax+38h].
constexpr std::uint8_t kExpectedBytes[] = {0xA1, 0x60, 0x24, 0x1D, 0x11, 0x8B, 0x48, 0x38};

constexpr int kSettingAuto = 0;
constexpr int kSettingPc = 1;
constexpr int kSettingXbox = 2;
constexpr int kSettingPs3 = 3;

// Remplaçante : mêmes conventions que l'original (aucun argument, résultat dans eax, ecx inutilisé).
int ControllerType() {
    return IconTypeFor(GetConfig().buttonIcons, PadConnected());
}

}  // namespace

int IconTypeFor(int setting, bool padConnected) noexcept {
    switch (setting) {
        case kSettingXbox:
            return 1;
        case kSettingPs3:
            return 2;
        case kSettingAuto:
            return padConnected ? 1 : 0;
        case kSettingPc:
        default:
            return 0;
    }
}

void InstallButtonIcons(HMODULE engine) {
    const int setting = GetConfig().buttonIcons;
    if (setting == kSettingPc || engine == nullptr) {
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engine) != kExpectedEngineBase) {
        log::Warn("Icônes de boutons : moteur chargé à une autre adresse, non modifié");
        return;
    }
    auto* const function = reinterpret_cast<std::uint8_t*>(kControllerTypeFunction);
    if (std::memcmp(function, kExpectedBytes, sizeof(kExpectedBytes)) != 0) {
        log::Warn("Icônes de boutons : fonction du moteur différente de celle attendue (autre version ?), non modifiée");
        return;
    }
    // jmp rel32 vers ControllerType.
    std::uint8_t jump[5] = {0xE9};
    const auto target = reinterpret_cast<std::intptr_t>(&ControllerType);
    const std::int32_t offset =
        static_cast<std::int32_t>(target - static_cast<std::intptr_t>(kControllerTypeFunction + sizeof(jump)));
    std::memcpy(jump + 1, &offset, sizeof(offset));
    const bool ok = hooking::WriteCode(function, jump, sizeof(jump));
    log::Info("Icônes de boutons : {} ({})", setting == kSettingAuto ? "auto, Xbox 360 si une manette est branchée"
                                             : setting == kSettingXbox ? "Xbox 360"
                                                                       : "PS3",
              ok ? "appliqué" : "échec");
}

}  // namespace hca::input
