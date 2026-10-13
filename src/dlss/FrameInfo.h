#pragma once

#include <cstdint>

namespace hca::dlss {

// Données de chaque image partagée qui ne sont pas des images : décalage de caméra (jitter) et réinitialisation de
// l'historique du DLSS. Bloc de mémoire partagée (CreateFileMapping) écrit par le mod (32 bits) et lu par
// HomecomingAgain.Dlss.exe (64 bits) : types de taille fixe, disposition identique dans les deux processus.
//
// Le mod écrit l'entrée de l'emplacement de l'image v avant de soumettre sa copie, `value` en dernier ; le programme DLSS
// la lit après avoir vu le compteur « produites » atteindre v, et ne l'utilise que si `value` vaut v.

constexpr std::uint32_t kFrameInfoVersion = 1;
constexpr std::uint32_t kFrameInfoSlots = 3;  // même anneau que dlss/SharedFrames.h et helper/src/Arguments.h

struct FrameInfo {
    std::uint64_t value;  // numéro de l'image (compteur « produites »)
    float jitterX;        // décalage de caméra en pixels (graphics/Jitter.h), vers la droite
    float jitterY;        // vers le bas
    std::uint32_t reset;  // 1 : historique à oublier (première image, changement de plan)
    std::uint32_t reserved;
};

struct FrameInfoBlock {
    std::uint32_t version;
    std::uint32_t slots;
    FrameInfo frames[kFrameInfoSlots];
};

static_assert(sizeof(FrameInfo) == 24, "disposition identique en 32 et 64 bits");
static_assert(sizeof(FrameInfoBlock) == 8 + 3 * 24, "disposition identique en 32 et 64 bits");

}  // namespace hca::dlss
