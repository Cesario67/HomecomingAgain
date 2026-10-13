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

// Icônes des boutons 0 à 3 de la manette (désassemblage du 13/10/2026) : la fonction qui traduit un bouton en nom
// d'icône renvoie l'adresse d'un nom écrit en dur, par « mov eax, adresse » (B8 + adresse) en 0x10986469,
// 0x10986488, 0x109864A7 et 0x109864C6 ("shv_buttonpccontrol1" à "4" : chiffres entourés, constaté). Ces adresses
// sont remplacées par celles de nos noms, dont le contenu suit le type d'icônes (RefreshButtonIcons). Les icônes Xbox
// 360 et PS3 existent dans les données du jeu (Engine\assets_pc_b.xml).
// Les 4 suivantes (0x1098645D, 0x1098647C, 0x1098649B, 0x109864BA) donnent la variante animée ("pc_1flash" à
// "pc_4flash"), choisie par le moteur selon un indicateur de l'appelant.
constexpr int kFaceSlots = 8;
constexpr std::uintptr_t kFaceIconInstructions[kFaceSlots] = {0x10986469, 0x10986488, 0x109864A7, 0x109864C6,
                                                              0x1098645D, 0x1098647C, 0x1098649B, 0x109864BA};
constexpr std::uint32_t kOriginalFaceNames[kFaceSlots] = {0x11078458, 0x11078430, 0x11078408, 0x110783E0,
                                                          0x11078474, 0x1107844C, 0x11078424, 0x110783FC};
constexpr std::size_t kNameCapacity = 40;
char g_faceNames[kFaceSlots][kNameCapacity]{};
bool g_facePatched = false;
int g_faceType = -1;

}  // namespace

const char* FaceIconName(int type, int button) noexcept {
    static constexpr const char* kNames[3][8] = {
        {"shv_buttonpccontrol1", "shv_buttonpccontrol2", "shv_buttonpccontrol3", "shv_buttonpccontrol4", "pc_1flash",
         "pc_2flash", "pc_3flash", "pc_4flash"},
        {"shv_buttonxbox360_a_32", "shv_buttonxbox360_b_32", "shv_buttonxbox360_x_32", "shv_buttonxbox360_y_32",
         "shv_aflash", "shv_bflash", "shv_x360xflash", "shv_yflash"},
        {"shv_buttonps3_x_32", "shv_buttonps3_circle_32", "shv_buttonps3_square_32", "shv_buttonps3_triangle_32",
         "shv_xflash", "shv_circleflash", "shv_squareflash", "shv_triangleflash"},
    };
    if (type < 0 || type > 2 || button < 0 || button > 7) {
        return kNames[0][0];
    }
    return kNames[type][button];
}

void RefreshButtonIcons() noexcept {
    if (!g_facePatched) {
        return;
    }
    const int type = IconTypeFor(GetConfig().buttonIcons, PadConnected());
    if (type == g_faceType) {
        return;
    }
    g_faceType = type;
    for (int i = 0; i < kFaceSlots; ++i) {
        strcpy_s(g_faceNames[i], FaceIconName(type, i));
    }
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

    // Boutons 0 à 3 : chaque instruction est vérifiée (B8 + adresse du nom d'origine) avant d'être redirigée.
    bool faceOk = true;
    for (int i = 0; i < kFaceSlots; ++i) {
        std::uint8_t expected[5] = {0xB8};
        std::memcpy(expected + 1, &kOriginalFaceNames[i], 4);
        faceOk = faceOk && std::memcmp(reinterpret_cast<const void*>(kFaceIconInstructions[i]), expected, 5) == 0;
    }
    if (faceOk) {
        for (int i = 0; i < kFaceSlots; ++i) {
            strcpy_s(g_faceNames[i], FaceIconName(0, i));
            std::uint8_t instruction[5] = {0xB8};
            const auto name = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_faceNames[i]));
            std::memcpy(instruction + 1, &name, 4);
            faceOk = faceOk && hooking::WriteCode(reinterpret_cast<void*>(kFaceIconInstructions[i]), instruction, 5);
        }
        g_facePatched = faceOk;
        RefreshButtonIcons();
    }
    log::Info("Icônes de boutons : boutons A, B, X, Y {}", faceOk ? "redirigés" : "non modifiés (code différent)");
    log::Info("Icônes de boutons : {} ({})", setting == kSettingAuto ? "auto, Xbox 360 si une manette est branchée"
                                             : setting == kSettingXbox ? "Xbox 360"
                                                                       : "PS3",
              ok ? "appliqué" : "échec");
}

}  // namespace hca::input
