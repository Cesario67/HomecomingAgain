#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Arguments.h"

namespace hca::dlss {

// Ouvre en Vulkan les images et le sémaphore partagés par le mod (jeu 32 bits, sous DXVK), sur la même carte
// graphique, et attend chaque nouvelle image. Équivalent C# : une classe IDisposable qui possède les objets Vulkan.
class SharedFrameReader {
public:
    using Logger = std::function<void(const std::string&)>;

    SharedFrameReader(const Arguments& args, Logger log);
    ~SharedFrameReader();
    SharedFrameReader(const SharedFrameReader&) = delete;
    SharedFrameReader& operator=(const SharedFrameReader&) = delete;

    bool Open();
    // Attend que le compteur dépasse `lastValue` (au plus `timeoutMs`). Renvoie la valeur atteinte, ou `lastValue`.
    std::uint64_t WaitNext(std::uint64_t lastValue, std::uint32_t timeoutMs);
    // Copie les deux images vers la mémoire de l'ordinateur (contrôle du jalon 3).
    bool ReadBack(std::vector<std::uint8_t>& color, std::vector<float>& depth);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hca::dlss
