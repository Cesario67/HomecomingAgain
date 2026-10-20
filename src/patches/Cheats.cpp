#include "patches/Cheats.h"

#include <cstring>

#include "core/Config.h"
#include "core/Log.h"

namespace hca::patches {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;

// Variables globales du moteur : [0x111D2460]+0x38 puis +8 = objet des réglages de débogage, `god` en +0xF5.
constexpr std::uintptr_t kEngineGlobals = 0x111D2460;
constexpr std::uint32_t kGodOffset = 0xF5;
// Octet « Unlimited Ammo » du personnage.
constexpr std::uint32_t kUnlimitedAmmoOffset = 0xA11;

// IsGodMode : mov eax,[0x111D2460] ; mov ecx,[eax+38h] ; mov edx,[ecx+8] ; mov al,[edx+0F5h] ; ret.
constexpr std::uintptr_t kIsGodMode = 0x10143590;
constexpr std::uint8_t kIsGodModeBytes[] = {0xA1, 0x60, 0x24, 0x1D, 0x11, 0x8B, 0x48, 0x38, 0x8B,
                                            0x51, 0x08, 0x8A, 0x82, 0xF5, 0x00, 0x00, 0x00, 0xC3};

// Personnage du joueur (ou 0) : lit [0x111D2460]+0x38, teste [0x115890EC]+0x61, appelle 0x10023B37(0) et renvoie +4.
constexpr std::uintptr_t kGetPlayer = 0x10143420;
constexpr std::uint8_t kGetPlayerBytes[] = {
    0xA1, 0x60, 0x24, 0x1D, 0x11, 0x8B, 0x48, 0x38, 0x56, 0x33, 0xF6, 0x39, 0x71, 0x04, 0x74, 0x1E, 0x8B,
    0x15, 0xEC, 0x90, 0x58, 0x11, 0x80, 0x7A, 0x61, 0x00, 0x75, 0x12, 0x56, 0xE8, 0xF5, 0x06, 0xEE, 0xFF,
    0x83, 0xC4, 0x04, 0x85, 0xC0, 0x74, 0x05, 0x8B, 0x40, 0x04, 0x5E, 0xC3, 0x8B, 0xC6, 0x5E, 0xC3};

// Fonction de la variable `unlimitedammo` : appelle le relais de 0x10143420 (E8 A5 FA CF FF -> 0x100190B5) et écrit
// `mov [eax+0A11h],bl` (88 98 11 0A 00 00) : confirme l'objet et l'offset utilisés ici.
constexpr std::uintptr_t kUnlimitedAmmoSetter = 0x10319600;
constexpr std::uint8_t kUnlimitedAmmoSetterBytes[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0x38, 0x01, 0x53,
                                                      0x0F, 0x94, 0xC3, 0xE8, 0xA5, 0xFA, 0xCF, 0xFF,
                                                      0x88, 0x98, 0x11, 0x0A, 0x00, 0x00, 0x5B, 0xC3};

using GetPlayerFn = std::uintptr_t(__cdecl*)();

bool g_god = false;
bool g_unlimitedAmmo = false;

template <std::size_t N>
bool Matches(std::uintptr_t address, const std::uint8_t (&expected)[N]) {
    return std::memcmp(reinterpret_cast<const void*>(address), expected, N) == 0;
}

// Lectures et écritures protégées : pendant un chargement, les objets du moteur peuvent être libérés ou absents.
// Pas d'objet C++ à détruire dans ces fonctions (exigé par __try).
bool ReadPointer(std::uintptr_t address, std::uintptr_t& value) noexcept {
    __try {
        value = *reinterpret_cast<const volatile std::uintptr_t*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void SetByte(std::uintptr_t address) noexcept {
    __try {
        auto* const flag = reinterpret_cast<volatile std::uint8_t*>(address);
        if (*flag != 1) {
            *flag = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::uintptr_t GetPlayer() noexcept {
    __try {
        return reinterpret_cast<GetPlayerFn>(kGetPlayer)();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

}  // namespace

void InstallCheats(HMODULE engineModule) {
    const Config& config = GetConfig();
    if (!config.cheatGod && !config.cheatUnlimitedAmmo) {
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engineModule) != kExpectedEngineBase) {
        log::Warn("Triches désactivées : moteur chargé à une adresse inattendue");
        return;
    }
    if (!Matches(kIsGodMode, kIsGodModeBytes) || !Matches(kGetPlayer, kGetPlayerBytes) ||
        !Matches(kUnlimitedAmmoSetter, kUnlimitedAmmoSetterBytes)) {
        log::Warn("Triches désactivées : version du moteur différente de celle analysée");
        return;
    }
    g_god = config.cheatGod;
    g_unlimitedAmmo = config.cheatUnlimitedAmmo;
    log::Info("Triches : mode dieu {}, munitions illimitées {}", g_god ? "activé" : "désactivé",
              g_unlimitedAmmo ? "activées" : "désactivées");
}

void ApplyCheatsEachFrame() noexcept {
    if (g_god) {
        const std::uintptr_t settings = cheats::FollowPointers(kEngineGlobals, {0x38, 0x08}, ReadPointer);
        if (settings != 0) {
            SetByte(settings + kGodOffset);
        }
    }
    if (g_unlimitedAmmo) {
        const std::uintptr_t player = GetPlayer();
        if (player != 0) {
            SetByte(player + kUnlimitedAmmoOffset);
        }
    }
}

}  // namespace hca::patches
