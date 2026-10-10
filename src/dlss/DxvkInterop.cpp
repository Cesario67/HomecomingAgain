#include "dlss/DxvkInterop.h"

#include <d3d9_vk_interop.h>

#include "core/Log.h"

namespace hca::dlss {

namespace {

ID3D9VkInteropDevice* g_interop = nullptr;  // une référence gardée pour la durée du jeu
bool g_probed = false;

void LogImage(const char* name, IDirect3DBaseTexture9* texture) {
    if (texture == nullptr) {
        log::Warn("DXVK : texture {} inconnue", name);
        return;
    }
    ID3D9VkInteropTexture* interop = nullptr;
    if (FAILED(texture->QueryInterface(__uuidof(ID3D9VkInteropTexture), reinterpret_cast<void**>(&interop))) ||
        interop == nullptr) {
        log::Warn("DXVK : la texture {} n'expose pas ID3D9VkInteropTexture", name);
        return;
    }
    VkImage image = 0;
    VkImageLayout layout = 0;
    const HRESULT result = interop->GetVulkanImageInfo(&image, &layout, nullptr);
    log::Info("DXVK : texture {} -> image Vulkan 0x{:X}, disposition {} (0x{:08X})", name, image, layout,
              static_cast<unsigned long>(result));
    interop->Release();
}

}  // namespace

bool OnDeviceCreated(IDirect3DDevice9* device) noexcept {
    if (device == nullptr || g_interop != nullptr) {
        return g_interop != nullptr;
    }
    if (FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice), reinterpret_cast<void**>(&g_interop))) ||
        g_interop == nullptr) {
        g_interop = nullptr;
        return false;
    }
    VkInstance instance = nullptr;
    VkPhysicalDevice physicalDevice = nullptr;
    VkDevice vkDevice = nullptr;
    g_interop->GetVulkanHandles(&instance, &physicalDevice, &vkDevice);
    VkQueue queue = nullptr;
    std::uint32_t queueIndex = 0;
    std::uint32_t queueFamily = 0;
    g_interop->GetSubmissionQueue(&queue, &queueIndex, &queueFamily);
    log::Info("DXVK : interopérabilité Vulkan disponible (instance {}, périphérique {}, file {} famille {} index {})",
              static_cast<const void*>(instance), static_cast<const void*>(vkDevice), static_cast<const void*>(queue),
              queueFamily, queueIndex);
    return true;
}

bool Available() noexcept {
    return g_interop != nullptr;
}

void Probe(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) noexcept {
    if (g_interop == nullptr || g_probed || sceneColor == nullptr || depth == nullptr) {
        return;
    }
    g_probed = true;
    LogImage("de la scène", sceneColor);
    LogImage("de profondeur", depth);
}

}  // namespace hca::dlss
