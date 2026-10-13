#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hca::dlss {

// Paramètres transmis par le mod (dlss/SharedFrames.cpp) en ligne de commande. Les handles sont hérités du jeu : leurs
// valeurs numériques sont les mêmes dans ce processus.
constexpr std::size_t kSlots = 3;  // même anneau que le mod (dlss/SharedFrames.h, kSharedSlots)

struct SharedImageArgs {
    std::uint64_t handles[kSlots]{};  // un handle par emplacement de l'anneau
    std::uint32_t format = 0;       // VkFormat
    std::uint64_t allocationSize = 0;
};

struct Arguments {
    std::uint32_t parentPid = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    SharedImageArgs color;
    SharedImageArgs depth;
    std::uint64_t semaphore = 0;  // compteur « produites » (signalé par le mod)
    std::uint64_t consumed = 0;   // compteur « consommées » (signalé par ce processus)
    std::wstring logPath;
    std::wstring outputDir;
};

// Fonction pure (testée) : lit « --clé valeur » ; nombres en décimal ou en 0x hexadécimal, listes de handles séparées
// par des virgules (--color, --depth). nullopt si un paramètre obligatoire manque ou est invalide.
std::optional<Arguments> ParseArguments(const std::vector<std::wstring>& args);

}  // namespace hca::dlss
