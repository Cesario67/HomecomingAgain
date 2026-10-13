#include "input/ButtonIcons.h"

#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "input/MenuBridge.h"
#include "input/TextureNameProbe.h"

namespace hca::input {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
// Fonction du moteur qui renvoie le type de manette (voir ButtonIcons.h).
constexpr std::uintptr_t kControllerTypeFunction = 0x104EC050;
// Octets d'origine (lus dans g_SilentHill.sgl le 13/10/2026) : mov eax,[111D2460h] ; mov ecx,[eax+38h].
constexpr std::uint8_t kExpectedBytes[] = {0xA1, 0x60, 0x24, 0x1D, 0x11, 0x8B, 0x48, 0x38};

// Changement de texture d'un élément d'interface (voir ButtonIcons.h) : relais 0x10054DCC (jmp 0x1050E5C0).
constexpr std::uintptr_t kSetTextureRelay = 0x10054DCC;
constexpr std::uintptr_t kSetTexture = 0x1050E5C0;
// Texte du moteur (un mot : pointeur vers les caractères) : construction depuis un char* (relais 0x10052EEB, vu à
// chaque appel de 0x1050E5C0 en 0x10907764 et 0x1092E567) et destruction (relais 0x1001F0E1, appelé juste après).
constexpr std::uintptr_t kStringConstruct = 0x10052EEB;
constexpr std::uintptr_t kStringDestroy = 0x1001F0E1;

using SetTextureFunction = void(__thiscall*)(void* item, void* nameObject);
using StringConstructFunction = void*(__thiscall*)(void* self, const char* text);
using StringDestroyFunction = void(__thiscall*)(void* self);

constexpr int kSettingAuto = 0;
constexpr int kSettingPc = 1;
constexpr int kSettingXbox = 2;
constexpr int kSettingPs3 = 3;

constexpr int kFaceSlots = 8;
constexpr const char* kNames[3][kFaceSlots] = {
    {"shv_buttonpccontrol1", "shv_buttonpccontrol2", "shv_buttonpccontrol3", "shv_buttonpccontrol4", "pc_1flash",
     "pc_2flash", "pc_3flash", "pc_4flash"},
    {"shv_buttonxbox360_a_32", "shv_buttonxbox360_b_32", "shv_buttonxbox360_x_32", "shv_buttonxbox360_y_32",
     "shv_aflash", "shv_bflash", "shv_x360xflash", "shv_yflash"},
    {"shv_buttonps3_x_32", "shv_buttonps3_circle_32", "shv_buttonps3_square_32", "shv_buttonps3_triangle_32",
     "shv_xflash", "shv_circleflash", "shv_squareflash", "shv_triangleflash"},
};

std::atomic<int> g_iconType{0};
bool g_textureHooked = false;

// Remplaçante : mêmes conventions que l'original (aucun argument, résultat dans eax, ecx inutilisé).
int ControllerType() {
    return IconTypeFor(GetConfig().buttonIcons, PadConnected());
}

// Lecture protégée : la structure du texte n'est connue que par le désassemblage.
bool ReadEngineString(const void* nameObject, char (&buffer)[96]) noexcept {
    __try {
        const char* text = *static_cast<const char* const*>(nameObject);
        std::size_t i = 0;
        for (; i + 1 < sizeof(buffer) && text[i] != '\0'; ++i) {
            buffer[i] = text[i];
        }
        buffer[i] = '\0';
        return i > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Même convention que l'original (thiscall, un argument retiré par l'appelé) : en fastcall, ecx = élément,
// edx inutilisé, argument sur la pile.
void __fastcall SetTextureHook(void* item, void* /*edx*/, void* nameObject) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto original = reinterpret_cast<SetTextureFunction>(kSetTexture);
    char name[96]{};
    const bool named = ReadEngineString(nameObject, name);
    const char* replacement = named ? ReplacementIconName(name, g_iconType.load(std::memory_order_relaxed)) : nullptr;
    if (replacement == nullptr) {
        original(item, nameObject);
    } else {
        std::uintptr_t replacementObject = 0;
        reinterpret_cast<StringConstructFunction>(kStringConstruct)(&replacementObject, replacement);
        original(item, &replacementObject);
        reinterpret_cast<StringDestroyFunction>(kStringDestroy)(&replacementObject);
    }
    if (named) {
        OnTextureApplied(item, name, replacement, caller);
    }
}

// Icônes insérées dans un texte (invite d'action « 1 Sauvegarder ») : chaque entrée de la liste du texte ([+0xFC],
// [+0xF8] entrées, cherchée par nom en 0x10907540 depuis une balise) porte son nom en +4 et sa texture en +8. La
// texture est trouvée par une clé construite depuis le nom (relais 0x1000E3F9, jmp 0x100BBA20 : thiscall, texte du
// moteur dont seul le pointeur vers les caractères est lu, et un octet), à trois endroits vérifiés : chargement
// d'une entrée (0x109070F0), de toutes les entrées (0x10907173), libération (0x10907224). Ces trois appels passent
// par nos fonctions, qui changent le nom pour la clé seulement (la recherche par balise garde le nom PC). La
// libération réutilise le nom choisi au chargement de la même entrée, pour garder le compte des références juste
// même si le type d'icônes a changé entre-temps.
constexpr std::uintptr_t kMakeKey = 0x1000E3F9;
constexpr std::uintptr_t kInlineLoadOne = 0x109070F0;
constexpr std::uintptr_t kInlineLoadAll = 0x10907173;
constexpr std::uintptr_t kInlineRelease = 0x10907224;

using MakeKeyFunction = void*(__thiscall*)(void* key, const void* nameObject, int flag);

std::mutex g_inlineLock;
std::unordered_map<std::uintptr_t, const char*> g_inlineNames;

void* __fastcall InlineLoadKey(void* key, void* /*edx*/, const void* nameObject, int flag) {
    const auto original = reinterpret_cast<MakeKeyFunction>(kMakeKey);
    char name[96]{};
    const char* replacement = ReadEngineString(nameObject, name)
                                  ? ReplacementIconName(name, g_iconType.load(std::memory_order_relaxed))
                                  : nullptr;
    try {
        std::lock_guard<std::mutex> lock(g_inlineLock);
        if (replacement != nullptr) {
            g_inlineNames[reinterpret_cast<std::uintptr_t>(nameObject)] = replacement;
        } else {
            g_inlineNames.erase(reinterpret_cast<std::uintptr_t>(nameObject));
        }
    } catch (...) {
        replacement = nullptr;
    }
    if (replacement == nullptr) {
        return original(key, nameObject, flag);
    }
    OnTextureApplied(nullptr, name, replacement, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const char* const replacementObject = replacement;
    return original(key, &replacementObject, flag);
}

void* __fastcall InlineReleaseKey(void* key, void* /*edx*/, const void* nameObject, int flag) {
    const auto original = reinterpret_cast<MakeKeyFunction>(kMakeKey);
    const char* replacement = nullptr;
    try {
        std::lock_guard<std::mutex> lock(g_inlineLock);
        const auto found = g_inlineNames.find(reinterpret_cast<std::uintptr_t>(nameObject));
        if (found != g_inlineNames.end()) {
            replacement = found->second;
            g_inlineNames.erase(found);
        }
    } catch (...) {
    }
    if (replacement == nullptr) {
        return original(key, nameObject, flag);
    }
    const char* const replacementObject = replacement;
    return original(key, &replacementObject, flag);
}

// L'instruction en `site` est-elle bien « call rel32 » vers kMakeKey ?
bool CallsMakeKey(std::uintptr_t site) {
    std::uint8_t expected[5] = {0xE8};
    const auto rel = static_cast<std::int32_t>(kMakeKey - (site + 5));
    std::memcpy(expected + 1, &rel, 4);
    return std::memcmp(reinterpret_cast<const void*>(site), expected, 5) == 0;
}

bool RedirectCall(std::uintptr_t site, const void* function) {
    std::uint8_t call[5] = {0xE8};
    const auto target = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(function) -
                                                  static_cast<std::intptr_t>(site + 5));
    std::memcpy(call + 1, &target, 4);
    return hooking::WriteCode(reinterpret_cast<void*>(site), call, 5);
}

bool InstallInlineIconHooks() {
    if (!CallsMakeKey(kInlineLoadOne) || !CallsMakeKey(kInlineLoadAll) || !CallsMakeKey(kInlineRelease)) {
        return false;
    }
    return RedirectCall(kInlineLoadOne, reinterpret_cast<const void*>(&InlineLoadKey)) &&
           RedirectCall(kInlineLoadAll, reinterpret_cast<const void*>(&InlineLoadKey)) &&
           RedirectCall(kInlineRelease, reinterpret_cast<const void*>(&InlineReleaseKey));
}

bool InstallTextureHook() {
    std::uint8_t expected[5] = {0xE9};
    const auto rel = static_cast<std::int32_t>(kSetTexture - (kSetTextureRelay + 5));
    std::memcpy(expected + 1, &rel, 4);
    if (std::memcmp(reinterpret_cast<const void*>(kSetTextureRelay), expected, 5) != 0) {
        return false;
    }
    std::uint8_t jump[5] = {0xE9};
    const auto target = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&SetTextureHook) -
                                                  static_cast<std::intptr_t>(kSetTextureRelay + 5));
    std::memcpy(jump + 1, &target, 4);
    return hooking::WriteCode(reinterpret_cast<void*>(kSetTextureRelay), jump, 5);
}

}  // namespace

const char* FaceIconName(int type, int button) noexcept {
    if (type < 0 || type > 2 || button < 0 || button >= kFaceSlots) {
        return kNames[0][0];
    }
    return kNames[type][button];
}

const char* ReplacementIconName(const char* name, int type) noexcept {
    if (name == nullptr || type < 1 || type > 2) {
        return nullptr;
    }
    for (int i = 0; i < kFaceSlots; ++i) {
        const char* pc = kNames[0][i];
        const std::size_t length = std::strlen(pc);
        if (std::strncmp(name, pc, length) != 0) {
            continue;
        }
        // Nom exact, ou variante "_64" des icônes fixes (shv_buttonpccontrol1_64...).
        if (name[length] == '\0' || (i < 4 && std::strcmp(name + length, "_64") == 0)) {
            return kNames[type][i];
        }
    }
    return nullptr;
}

void RefreshButtonIcons() noexcept {
    if (!g_textureHooked) {
        return;
    }
    g_iconType.store(IconTypeFor(GetConfig().buttonIcons, PadConnected()), std::memory_order_relaxed);
}

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

    g_textureHooked = InstallTextureHook();
    RefreshButtonIcons();
    const bool inline_ = g_textureHooked && InstallInlineIconHooks();
    log::Info("Icônes de boutons : textures A, B, X, Y {}, icônes dans les textes {}",
              g_textureHooked ? "remplacées à l'affichage" : "non modifiées (code différent)",
              inline_ ? "remplacées" : "non modifiées (code différent)");
    log::Info("Icônes de boutons : {} ({})", setting == kSettingAuto ? "auto, Xbox 360 si une manette est branchée"
                                             : setting == kSettingXbox ? "Xbox 360"
                                                                       : "PS3",
              ok ? "appliqué" : "échec");
}

}  // namespace hca::input
