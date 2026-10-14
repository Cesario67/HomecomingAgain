#pragma once

#include <cstdint>

namespace hca::crash {

// Fonction pure (testée par tests/unit) : temps sans image retenu par le détecteur de blocage, en ticks. Le jeu
// cesse de présenter des images quand sa fenêtre n'est pas au premier plan (constaté le 14/10/2026 : 79 s et 44 s
// sans image pendant un Alt-Tab, puis reprise normale) ; ce temps-là ne compte pas. Le décompte part donc de la
// dernière image, ou du dernier instant où le jeu était en arrière-plan s'il est plus récent.
std::int64_t HangElapsed(std::int64_t now, std::int64_t lastPresent, std::int64_t lastBackground) noexcept;

}  // namespace hca::crash
