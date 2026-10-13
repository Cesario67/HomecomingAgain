#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Arguments.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace hca::dlss {

// Ouvre en Vulkan les images et le sémaphore partagés par le mod (jeu 32 bits, sous DXVK), sur la même carte
// graphique, et attend chaque nouvelle image. Équivalent C# : une classe IDisposable qui possède les objets Vulkan.
// Objets Vulkan du lecteur, prêtés au DLSS (non possédés).
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr getInstanceProc = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProc = nullptr;
};

// Images partagées d'un emplacement de l'anneau (disposition GENERAL, posée par le mod).
struct SlotImages {
    VkImage color = VK_NULL_HANDLE;
    VkImage depth = VK_NULL_HANDLE;
    VkImage motion = VK_NULL_HANDLE;
};

class SharedFrameReader {
public:
    using Logger = std::function<void(const std::string&)>;

    SharedFrameReader(const Arguments& args, Logger log);
    ~SharedFrameReader();
    SharedFrameReader(const SharedFrameReader&) = delete;
    SharedFrameReader& operator=(const SharedFrameReader&) = delete;

    // Extensions Vulkan supplémentaires (celles du DLSS), à donner avant Open.
    void AddExtensions(const std::vector<std::string>& instanceExtensions, const std::vector<std::string>& deviceExtensions);
    bool Open();
    VulkanContext Context() const;
    SlotImages Images(std::size_t slot) const;
    // Attend que le compteur dépasse `lastValue` (au plus `timeoutMs`). Renvoie la valeur atteinte, ou `lastValue`.
    std::uint64_t WaitNext(std::uint64_t lastValue, std::uint32_t timeoutMs);
    // Signale au mod que l'image `value` est lue : son emplacement peut être réécrit.
    void MarkConsumed(std::uint64_t value);
    // Copie les trois images de l'emplacement slot vers la mémoire de l'ordinateur (images de contrôle).
    bool ReadBack(std::size_t slot, std::vector<std::uint8_t>& color, std::vector<float>& depth,
                  std::vector<std::uint16_t>& motion);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hca::dlss
