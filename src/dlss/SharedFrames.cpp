#include "dlss/SharedFrames.h"

#include <windows.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <d3d9_vk_interop.h>
#include <vulkan/vulkan_win32.h>

#include <atomic>
#include <format>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "dlss/DxvkInterop.h"
#include "dlss/FrameInfo.h"
#include "graphics/Jitter.h"

namespace hca::dlss {

namespace {

constexpr ULONGLONG kReportIntervalMs = 10000;

// Fonctions Vulkan chargées depuis le périphérique de DXVK (vulkan-1.dll est déjà chargé par DXVK).
struct VulkanFunctions {
    PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceProperties getProperties = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkGetMemoryWin32HandleKHR getMemoryWin32Handle = nullptr;
    PFN_vkCreateSemaphore createSemaphore = nullptr;
    PFN_vkGetSemaphoreWin32HandleKHR getSemaphoreWin32Handle = nullptr;
    PFN_vkGetSemaphoreCounterValue getSemaphoreCounterValue = nullptr;
    PFN_vkCreateCommandPool createCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers allocateCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer resetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer beginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer endCommandBuffer = nullptr;
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyImage cmdCopyImage = nullptr;
    PFN_vkCreateFence createFence = nullptr;
    PFN_vkGetFenceStatus getFenceStatus = nullptr;
    PFN_vkResetFences resetFences = nullptr;
    PFN_vkQueueSubmit queueSubmit = nullptr;
    // Retour de l'image du DLSS vers le jeu.
    PFN_vkGetPhysicalDeviceFormatProperties getFormatProperties = nullptr;
    PFN_vkCmdBlitImage cmdBlitImage = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
};

struct SharedImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    HANDLE handle = nullptr;  // handle NT exporté, gardé ouvert pour le processus DLSS
    VkExtent3D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkDeviceSize size = 0;     // taille de l'allocation, nécessaire à l'import dans le processus DLSS
    bool initialized = false;  // disposition passée de UNDEFINED à GENERAL
};

struct ExportedTimeline {
    VkSemaphore semaphore = VK_NULL_HANDLE;
    HANDLE handle = nullptr;
};

struct State {
    bool attempted = false;
    bool ready = false;
    VulkanFunctions vk;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    // Anneau : l'image numéro v (valeur du compteur « produites ») va dans l'emplacement SlotForFrame(v, kSharedSlots).
    SharedImage color[kSharedSlots];
    SharedImage depth[kSharedSlots];
    SharedImage motion[kSharedSlots];  // vecteurs de mouvement (G16R16F, graphics/MotionVectors.h)
    ExportedTimeline produced;  // signalé par le mod quand l'image v est copiée
    ExportedTimeline consumed;  // signalé par le processus DLSS quand il a fini avec l'image v
    std::uint64_t producedValue = 0;
    // Décalage de caméra et réinitialisation de chaque image (dlss/FrameInfo.h), partagés avec le processus DLSS.
    HANDLE infoMapping = nullptr;
    FrameInfoBlock* info = nullptr;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer commands[kSharedSlots]{};
    VkFence fences[kSharedSlots]{};
    // Retour de l'image du DLSS ([DLSS] Apply=1) : le processus DLSS écrit dans output[slot] et passe le compteur
    // « traitées » à v ; la carte graphique attend ce compteur avant de recopier la sortie dans la scène du jeu.
    SharedImage output[kSharedSlots];
    ExportedTimeline processed;
    VkCommandBuffer back[kSharedSlots]{};
    std::atomic<bool> returnEnabled{false};  // arrêté pour de bon par le chien de garde (autre thread)
    std::atomic<std::uint64_t> waitingFor{0};  // dernière valeur attendue par la carte graphique
    HANDLE watchdog = nullptr;
    std::uint32_t returned = 0;
    std::uint32_t notReturned = 0;  // processus DLSS en retard : image d'origine gardée
    // Statistiques journalisées toutes les 10 s.
    std::uint32_t shared = 0;
    std::uint32_t skippedCopy = 0;      // copie précédente de l'emplacement pas finie sur la carte
    std::uint32_t skippedConsumer = 0;  // le processus DLSS n'a pas encore libéré l'emplacement
    double cpuMs = 0.0;
    ULONGLONG nextReport = 0;
};

State g_state;

template <typename Fn>
bool LoadDevice(PFN_vkGetDeviceProcAddr getDeviceProc, VkDevice device, const char* name, Fn& out) {
    out = reinterpret_cast<Fn>(getDeviceProc(device, name));
    if (out == nullptr) {
        log::Warn("DLSS : fonction Vulkan {} introuvable", name);
    }
    return out != nullptr;
}

bool LoadFunctions(State& s) {
    const HMODULE vulkan = GetModuleHandleW(L"vulkan-1.dll");
    const auto getInstanceProc =
        vulkan != nullptr ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkan, "vkGetInstanceProcAddr"))
                          : nullptr;
    if (getInstanceProc == nullptr) {
        log::Warn("DLSS : vulkan-1.dll ou vkGetInstanceProcAddr introuvable");
        return false;
    }
    const auto getDeviceProc =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(getInstanceProc(s.instance, "vkGetDeviceProcAddr"));
    s.vk.getMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        getInstanceProc(s.instance, "vkGetPhysicalDeviceMemoryProperties"));
    s.vk.getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        getInstanceProc(s.instance, "vkGetPhysicalDeviceProperties"));
    if (getDeviceProc == nullptr || s.vk.getMemoryProperties == nullptr || s.vk.getProperties == nullptr) {
        log::Warn("DLSS : fonctions Vulkan de base introuvables");
        return false;
    }
    bool ok = true;
    ok &= LoadDevice(getDeviceProc, s.device, "vkCreateImage", s.vk.createImage);
    ok &= LoadDevice(getDeviceProc, s.device, "vkGetImageMemoryRequirements", s.vk.getImageMemoryRequirements);
    ok &= LoadDevice(getDeviceProc, s.device, "vkAllocateMemory", s.vk.allocateMemory);
    ok &= LoadDevice(getDeviceProc, s.device, "vkBindImageMemory", s.vk.bindImageMemory);
    ok &= LoadDevice(getDeviceProc, s.device, "vkGetMemoryWin32HandleKHR", s.vk.getMemoryWin32Handle);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCreateSemaphore", s.vk.createSemaphore);
    ok &= LoadDevice(getDeviceProc, s.device, "vkGetSemaphoreWin32HandleKHR", s.vk.getSemaphoreWin32Handle);
    ok &= LoadDevice(getDeviceProc, s.device, "vkGetSemaphoreCounterValue", s.vk.getSemaphoreCounterValue);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCreateCommandPool", s.vk.createCommandPool);
    ok &= LoadDevice(getDeviceProc, s.device, "vkAllocateCommandBuffers", s.vk.allocateCommandBuffers);
    ok &= LoadDevice(getDeviceProc, s.device, "vkResetCommandBuffer", s.vk.resetCommandBuffer);
    ok &= LoadDevice(getDeviceProc, s.device, "vkBeginCommandBuffer", s.vk.beginCommandBuffer);
    ok &= LoadDevice(getDeviceProc, s.device, "vkEndCommandBuffer", s.vk.endCommandBuffer);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCmdPipelineBarrier", s.vk.cmdPipelineBarrier);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCmdCopyImage", s.vk.cmdCopyImage);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCreateFence", s.vk.createFence);
    ok &= LoadDevice(getDeviceProc, s.device, "vkGetFenceStatus", s.vk.getFenceStatus);
    ok &= LoadDevice(getDeviceProc, s.device, "vkResetFences", s.vk.resetFences);
    ok &= LoadDevice(getDeviceProc, s.device, "vkQueueSubmit", s.vk.queueSubmit);
    ok &= LoadDevice(getDeviceProc, s.device, "vkCmdBlitImage", s.vk.cmdBlitImage);
    ok &= LoadDevice(getDeviceProc, s.device, "vkSignalSemaphore", s.vk.signalSemaphore);
    s.vk.getFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
        getInstanceProc(s.instance, "vkGetPhysicalDeviceFormatProperties"));
    return ok && s.vk.getFormatProperties != nullptr;
}

// Image du jeu (Vulkan) derrière une texture Direct3D 9 de DXVK, avec son format et sa taille.
bool GameImage(IDirect3DBaseTexture9* texture, VkImage& image, VkImageCreateInfo& info) {
    ID3D9VkInteropTexture* interop = nullptr;
    if (texture == nullptr ||
        FAILED(texture->QueryInterface(__uuidof(ID3D9VkInteropTexture), reinterpret_cast<void**>(&interop))) ||
        interop == nullptr) {
        return false;
    }
    std::uint32_t queueFamilies[4]{};
    info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.queueFamilyIndexCount = 4;
    info.pQueueFamilyIndices = queueFamilies;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    const HRESULT result = interop->GetVulkanImageInfo(&image, &layout, &info);
    interop->Release();
    info.pQueueFamilyIndices = nullptr;
    info.queueFamilyIndexCount = 0;
    return SUCCEEDED(result) && image != VK_NULL_HANDLE;
}

// `extraUsage` et `format` (si défini) servent aux images de sortie du DLSS, écrites par un shader (STORAGE).
bool CreateSharedImage(State& s, const VkImageCreateInfo& source, SharedImage& out, const std::string& name,
                       VkImageUsageFlags extraUsage = 0, VkFormat format = VK_FORMAT_UNDEFINED) {
    VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.pNext = &external;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format != VK_FORMAT_UNDEFINED ? format : source.format;
    info.extent = {source.extent.width, source.extent.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extraUsage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (s.vk.createImage(s.device, &info, nullptr, &out.image) != VK_SUCCESS) {
        log::Warn("DLSS : création de l'image partagée {} impossible", name);
        return false;
    }
    VkMemoryRequirements requirements{};
    s.vk.getImageMemoryRequirements(s.device, out.image, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    s.vk.getMemoryProperties(s.physicalDevice, &properties);
    std::uint32_t type = UINT32_MAX;
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
            (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            type = i;
            break;
        }
    }
    if (type == UINT32_MAX) {
        log::Warn("DLSS : pas de mémoire graphique adaptée pour l'image {}", name);
        return false;
    }
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    dedicated.image = out.image;
    VkExportMemoryAllocateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
    exportInfo.pNext = &dedicated;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.pNext = &exportInfo;
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
    if (s.vk.allocateMemory(s.device, &allocate, nullptr, &out.memory) != VK_SUCCESS ||
        s.vk.bindImageMemory(s.device, out.image, out.memory, 0) != VK_SUCCESS) {
        log::Warn("DLSS : allocation exportable impossible pour l'image {}", name);
        return false;
    }
    VkMemoryGetWin32HandleInfoKHR handleInfo{VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR};
    handleInfo.memory = out.memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    if (s.vk.getMemoryWin32Handle(s.device, &handleInfo, &out.handle) != VK_SUCCESS) {
        log::Warn("DLSS : export de la mémoire de l'image {} impossible", name);
        return false;
    }
    out.extent = info.extent;
    out.format = info.format;
    out.size = requirements.size;
    log::Info("DLSS : image partagée {} {}x{}, format Vulkan {}, {} Ko, handle 0x{:X}", name, out.extent.width,
              out.extent.height, static_cast<int>(out.format), requirements.size / 1024,
              reinterpret_cast<std::uintptr_t>(out.handle));
    return true;
}

bool CreateTimeline(State& s, ExportedTimeline& out, const char* name) {
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkExportSemaphoreCreateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
    exportInfo.pNext = &type;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphore.pNext = &exportInfo;
    if (s.vk.createSemaphore(s.device, &semaphore, nullptr, &out.semaphore) != VK_SUCCESS) {
        log::Warn("DLSS : compteur « {} » exportable impossible", name);
        return false;
    }
    VkSemaphoreGetWin32HandleInfoKHR handleInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR};
    handleInfo.semaphore = out.semaphore;
    handleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    if (s.vk.getSemaphoreWin32Handle(s.device, &handleInfo, &out.handle) != VK_SUCCESS) {
        log::Warn("DLSS : export du compteur « {} » impossible", name);
        return false;
    }
    log::Info("DLSS : compteur « {} » exporté, handle 0x{:X}", name, reinterpret_cast<std::uintptr_t>(out.handle));
    return true;
}

bool CreateCommands(State& s) {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = s.queueFamily;
    if (s.vk.createCommandPool(s.device, &poolInfo, nullptr, &s.pool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = s.pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = kSharedSlots;
    if (s.vk.allocateCommandBuffers(s.device, &allocate, s.commands) != VK_SUCCESS) {
        return false;
    }
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (VkFence& fence : s.fences) {
        if (s.vk.createFence(s.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool Initialize(State& s, IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth,
                IDirect3DBaseTexture9* motion) {
    ID3D9VkInteropDevice* interop = Interop();
    if (interop == nullptr) {
        return false;
    }
    interop->GetVulkanHandles(&s.instance, &s.physicalDevice, &s.device);
    std::uint32_t queueIndex = 0;
    interop->GetSubmissionQueue(&s.queue, &queueIndex, &s.queueFamily);
    VkImage image = VK_NULL_HANDLE;
    VkImageCreateInfo colorInfo{};
    VkImageCreateInfo depthInfo{};
    VkImageCreateInfo motionInfo{};
    if (!LoadFunctions(s) || !GameImage(sceneColor, image, colorInfo) || !GameImage(depth, image, depthInfo) ||
        !GameImage(motion, image, motionInfo)) {
        return false;
    }
    for (std::uint32_t i = 0; i < kSharedSlots; ++i) {
        if (!CreateSharedImage(s, colorInfo, s.color[i], std::format("scène {}", i)) ||
            !CreateSharedImage(s, depthInfo, s.depth[i], std::format("profondeur {}", i)) ||
            !CreateSharedImage(s, motionInfo, s.motion[i], std::format("mouvement {}", i))) {
            return false;
        }
    }
    if (!CreateTimeline(s, s.produced, "images produites") || !CreateTimeline(s, s.consumed, "images consommées") ||
        !CreateCommands(s)) {
        return false;
    }
    if (GetConfig().dlssApply) {
        // Sortie du DLSS au format de la scène si la carte permet de l'écrire depuis un shader, sinon en R8G8B8A8
        // (la recopie vers la scène se fait par vkCmdBlitImage, qui convertit l'ordre des canaux).
        VkFormatProperties properties{};
        s.vk.getFormatProperties(s.physicalDevice, colorInfo.format, &properties);
        const VkFormat outputFormat = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0
                                          ? colorInfo.format
                                          : VK_FORMAT_R8G8B8A8_UNORM;
        bool ok = true;
        for (std::uint32_t i = 0; i < kSharedSlots && ok; ++i) {
            ok = CreateSharedImage(s, colorInfo, s.output[i], std::format("sortie {}", i), VK_IMAGE_USAGE_STORAGE_BIT,
                                   outputFormat);
        }
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = s.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = kSharedSlots;
        ok = ok && CreateTimeline(s, s.processed, "images traitées") &&
             s.vk.allocateCommandBuffers(s.device, &allocate, s.back) == VK_SUCCESS;
        s.returnEnabled = ok;
        log::Info("DLSS : retour de l'image vers le jeu {}", ok ? "préparé" : "impossible");
    }
    return true;
}

VkImageMemoryBarrier Barrier(VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                             VkAccessFlags dstAccess);

// Chien de garde du retour : si le processus DLSS ne passe pas le compteur « traitées » à la valeur attendue par la
// carte graphique dans les 300 ms (processus planté ou figé), le mod le fait lui-même depuis le processeur pour débloquer
// la carte, et coupe le retour : le jeu continue avec son image d'origine.
DWORD WINAPI WatchdogThread(void* parameter) {
    State& s = *static_cast<State*>(parameter);
    ULONGLONG stuckSince = 0;
    while (s.returnEnabled) {
        Sleep(50);
        const std::uint64_t waiting = s.waitingFor.load();
        std::uint64_t processed = 0;
        s.vk.getSemaphoreCounterValue(s.device, s.processed.semaphore, &processed);
        if (processed >= waiting) {
            stuckSince = 0;
            continue;
        }
        const ULONGLONG now = GetTickCount64();
        if (stuckSince == 0) {
            stuckSince = now;
        } else if (now - stuckSince > 300) {
            s.returnEnabled = false;
            VkSemaphoreSignalInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
            signal.semaphore = s.processed.semaphore;
            signal.value = waiting;
            s.vk.signalSemaphore(s.device, &signal);
            log::Error("DLSS : le processus DLSS ne répond plus (image {} attendue, {} traitée) : retour de l'image "
                       "arrêté, image d'origine du jeu",
                       waiting, processed);
        }
    }
    return 0;
}

// Recopie de la sortie du DLSS (emplacement slot) dans la scène du jeu, après l'attente du compteur « traitées ».
void RecordReturn(State& s, VkCommandBuffer commands, VkImage game, const SharedImage& output) {
    VkImageMemoryBarrier before[2] = {
        Barrier(output.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT),
        Barrier(game, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT)};
    s.vk.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                            0, nullptr, 2, before);
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.srcOffsets[1] = {static_cast<std::int32_t>(output.extent.width),
                            static_cast<std::int32_t>(output.extent.height), 1};
    region.dstOffsets[1] = region.srcOffsets[1];
    s.vk.cmdBlitImage(commands, output.image, VK_IMAGE_LAYOUT_GENERAL, game, VK_IMAGE_LAYOUT_GENERAL, 1, &region,
                      VK_FILTER_NEAREST);
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    after.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    s.vk.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &after,
                            0, nullptr, 0, nullptr);
}

VkImageMemoryBarrier Barrier(VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                             VkAccessFlags dstAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return barrier;
}

void RecordCopy(State& s, VkCommandBuffer commands, const VkImage (&game)[3], SharedImage* (&shared)[3]) {
    // Images du jeu : laissées en GENERAL (disposition rapportée par DXVK) ; dépendance sur tout le travail déjà soumis.
    VkImageMemoryBarrier before[6]{};
    for (int i = 0; i < 3; ++i) {
        before[i] = Barrier(game[i], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
                            VK_ACCESS_TRANSFER_READ_BIT);
        before[3 + i] = Barrier(shared[i]->image,
                                shared[i]->initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        shared[i]->initialized = true;
    }
    s.vk.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                            0, nullptr, 6, before);
    for (int i = 0; i < 3; ++i) {
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = shared[i]->extent;
        s.vk.cmdCopyImage(commands, game[i], VK_IMAGE_LAYOUT_GENERAL, shared[i]->image, VK_IMAGE_LAYOUT_GENERAL, 1,
                          &region);
    }
    // Après la copie : le travail suivant de DXVK (qui réécrira ces images) attend la fin de nos lectures.
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    s.vk.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &after,
                            0, nullptr, 0, nullptr);
}
void Report(State& s) {
    const ULONGLONG now = GetTickCount64();
    if (s.nextReport == 0) {
        s.nextReport = now + kReportIntervalMs;
        return;
    }
    if (now < s.nextReport) {
        return;
    }
    s.nextReport = now + kReportIntervalMs;
    std::uint64_t consumed = 0;
    s.vk.getSemaphoreCounterValue(s.device, s.consumed.semaphore, &consumed);
    log::Info("DLSS : images partagées {}, sautées {} (copie en cours) et {} (lecteur en retard), temps processeur "
              "moyen {:.3f} ms, produites {}, consommées {}",
              s.shared, s.skippedCopy, s.skippedConsumer, s.shared > 0 ? s.cpuMs / s.shared : 0.0, s.producedValue,
              consumed);
    if (GetConfig().dlssApply) {
        std::uint64_t processed = 0;
        s.vk.getSemaphoreCounterValue(s.device, s.processed.semaphore, &processed);
        log::Info("DLSS : image du DLSS rendue au jeu {} fois, image d'origine gardée {} fois (DLSS en retard), "
                  "traitées {}, retour {}",
                  s.returned, s.notReturned, processed, s.returnEnabled ? "actif" : "arrêté");
    }
    s.returned = 0;
    s.notReturned = 0;
    s.shared = 0;
    s.skippedCopy = 0;
    s.skippedConsumer = 0;
    s.cpuMs = 0.0;
}

// Lance HomecomingAgain.Dlss.exe (64 bits, à côté de SilentHill.exe) en lui transmettant les handles par héritage :
// seuls ces handles sont hérités (liste explicite), leurs valeurs numériques sont les mêmes dans le processus enfant et
// lui sont passées en ligne de commande avec la taille, les formats et la carte graphique.
void LaunchHelper(State& s) {
    const std::wstring exe = GetPaths().gameDir + L"\\HomecomingAgain.Dlss.exe";
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        log::Warn("DLSS : HomecomingAgain.Dlss.exe absent du dossier du jeu, images partagées sans lecteur");
        return;
    }
    // Bloc des données de chaque image (dlss/FrameInfo.h) : mémoire partagée anonyme, transmise par héritage.
    s.infoMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(FrameInfoBlock), nullptr);
    if (s.infoMapping != nullptr) {
        s.info = static_cast<FrameInfoBlock*>(MapViewOfFile(s.infoMapping, FILE_MAP_WRITE, 0, 0, sizeof(FrameInfoBlock)));
    }
    if (s.info != nullptr) {
        s.info->version = kFrameInfoVersion;
        s.info->slots = kFrameInfoSlots;
    } else {
        log::Warn("DLSS : bloc des données d'image impossible (erreur {}), décalage de caméra non transmis",
                  GetLastError());
    }
    VkPhysicalDeviceProperties properties{};
    s.vk.getProperties(s.physicalDevice, &properties);
    std::vector<HANDLE> handles;
    std::wstring colors;
    std::wstring depths;
    std::wstring motions;
    for (std::uint32_t i = 0; i < kSharedSlots; ++i) {
        handles.push_back(s.color[i].handle);
        handles.push_back(s.depth[i].handle);
        handles.push_back(s.motion[i].handle);
        colors += std::format(L"{}0x{:X}", i == 0 ? L"" : L",", reinterpret_cast<std::uintptr_t>(s.color[i].handle));
        depths += std::format(L"{}0x{:X}", i == 0 ? L"" : L",", reinterpret_cast<std::uintptr_t>(s.depth[i].handle));
        motions += std::format(L"{}0x{:X}", i == 0 ? L"" : L",", reinterpret_cast<std::uintptr_t>(s.motion[i].handle));
    }
    handles.push_back(s.produced.handle);
    handles.push_back(s.consumed.handle);
    if (s.info != nullptr) {
        handles.push_back(s.infoMapping);
    }
    for (HANDLE handle : handles) {
        SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    }
    std::wstring command = std::format(
        L"\"{}\" --parent {} --width {} --height {} --color {} --color-format {} --color-size {} --depth {} "
        L"--depth-format {} --depth-size {} --motion {} --motion-format {} --motion-size {} --semaphore 0x{:X} --consumed 0x{:X} --vendor 0x{:X} --device 0x{:X} "
        L"--log \"{}\" --out \"{}\"",
        exe, GetCurrentProcessId(), s.color[0].extent.width, s.color[0].extent.height, colors,
        static_cast<int>(s.color[0].format), s.color[0].size, depths, static_cast<int>(s.depth[0].format),
        s.depth[0].size, motions, static_cast<int>(s.motion[0].format), s.motion[0].size,
        reinterpret_cast<std::uintptr_t>(s.produced.handle),
        reinterpret_cast<std::uintptr_t>(s.consumed.handle), properties.vendorID, properties.deviceID,
        GetPaths().logsDir + L"\\HomecomingAgain.Dlss.log", GetPaths().modDir + L"\\captures");
    if (s.info != nullptr) {
        command += std::format(L" --info 0x{:X}", reinterpret_cast<std::uintptr_t>(s.infoMapping));
    }
    if (s.returnEnabled) {
        std::wstring outputs;
        for (std::uint32_t i = 0; i < kSharedSlots; ++i) {
            outputs += std::format(L"{}0x{:X}", i == 0 ? L"" : L",", reinterpret_cast<std::uintptr_t>(s.output[i].handle));
            handles.push_back(s.output[i].handle);
            SetHandleInformation(s.output[i].handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
        handles.push_back(s.processed.handle);
        SetHandleInformation(s.processed.handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        command += std::format(L" --output {} --output-format {} --output-size {} --processed 0x{:X}", outputs,
                               static_cast<int>(s.output[0].format), s.output[0].size,
                               reinterpret_cast<std::uintptr_t>(s.processed.handle));
    }

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<BYTE> attributeBuffer(attributeSize);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    bool ok = InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize) != FALSE;
    const bool listReady = ok;
    ok = ok && UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(),
                                         handles.size() * sizeof(HANDLE), nullptr, nullptr) != FALSE;
    if (ok) {
        startup.lpAttributeList = attributes;
        ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                            EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, GetPaths().gameDir.c_str(),
                            &startup.StartupInfo, &process) != FALSE;
    }
    const DWORD error = GetLastError();
    if (listReady) {
        DeleteProcThreadAttributeList(attributes);
    }
    if (!ok) {
        log::Warn("DLSS : lancement de HomecomingAgain.Dlss.exe impossible (erreur {})", error);
        s.returnEnabled = false;
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    log::Info("DLSS : HomecomingAgain.Dlss.exe lancé (processus {})", process.dwProcessId);
    if (s.returnEnabled) {
        s.watchdog = CreateThread(nullptr, 0, &WatchdogThread, &s, 0, nullptr);
        if (s.watchdog == nullptr) {
            s.returnEnabled = false;  // sans chien de garde, pas d'attente de la carte graphique
        }
    }
}

}  // namespace

bool ShareFrame(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth, IDirect3DBaseTexture9* motion) noexcept {
    State& s = g_state;
    // Les vecteurs de mouvement manquent à la première image (pas encore de caméra précédente) : image ignorée.
    if (!GetConfig().dlssShareFrames || sceneColor == nullptr || depth == nullptr || motion == nullptr) {
        return false;
    }
    if (!s.attempted) {
        s.attempted = true;
        s.ready = Initialize(s, sceneColor, depth, motion);
        log::Info("DLSS : partage des images {}", s.ready ? "prêt" : "indisponible");
        if (s.ready) {
            LaunchHelper(s);
        }
    }
    if (!s.ready) {
        return false;
    }
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&start);

    const std::uint64_t value = s.producedValue + 1;
    const std::uint32_t slot = SlotForFrame(value, kSharedSlots);
    if (s.vk.getFenceStatus(s.device, s.fences[slot]) != VK_SUCCESS) {
        ++s.skippedCopy;  // copie précédente de cet emplacement pas encore terminée : on ne bloque pas le jeu
        Report(s);
        return false;
    }
    // Synchronisation en retour : l'emplacement contient l'image value - kSharedSlots, que le processus DLSS doit avoir
    // fini de lire. Vérification sans attente : sinon l'image est sautée (le jeu n'attend jamais le processus DLSS).
    std::uint64_t consumed = 0;
    s.vk.getSemaphoreCounterValue(s.device, s.consumed.semaphore, &consumed);
    if (!SlotIsFree(value, consumed, kSharedSlots)) {
        ++s.skippedConsumer;
        Report(s);
        return false;
    }
    VkImage game[3]{};
    VkImageCreateInfo unused{};
    if (!GameImage(sceneColor, game[0], unused) || !GameImage(depth, game[1], unused) ||
        !GameImage(motion, game[2], unused)) {
        ++s.skippedCopy;
        Report(s);
        return false;
    }
    ID3D9VkInteropDevice* interop = Interop();
    // Le travail Direct3D 9 en attente doit être soumis avant le nôtre (documentation de l'interface DXVK).
    interop->FlushRenderingCommands();

    VkCommandBuffer commands = s.commands[slot];
    s.vk.resetCommandBuffer(commands, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    s.vk.beginCommandBuffer(commands, &begin);
    SharedImage* shared[3] = {&s.color[slot], &s.depth[slot], &s.motion[slot]};
    RecordCopy(s, commands, game, shared);
    s.vk.endCommandBuffer(commands);

    VkTimelineSemaphoreSubmitInfo timelineInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &value;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.pNext = &timelineInfo;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &s.produced.semaphore;
    if (s.info != nullptr) {
        // Entrée écrite avant la soumission, le numéro en dernier (voir dlss/FrameInfo.h).
        FrameInfo& info = s.info->frames[slot];
        const graphics::jitter::Offset jitter = graphics::jitter::Current();
        info.jitterX = GetConfig().dlssJitter > 0.0f ? jitter.x : 0.0f;
        info.jitterY = GetConfig().dlssJitter > 0.0f ? jitter.y : 0.0f;
        info.reset = value == 1 ? 1u : 0u;
        MemoryBarrier();
        info.value = value;
    }
    // Retour de l'image du DLSS : seulement si le processus DLSS a déjà rendu au moins une image et suit le rythme
    // (image v - 2 traitée), sinon la scène d'origine est gardée pour cette image. La carte graphique attend « traitées »
    // = v avant la recopie ; le processeur, lui, n'attend jamais (chien de garde en cas de blocage).
    VkSubmitInfo submits[2] = {submit, {VK_STRUCTURE_TYPE_SUBMIT_INFO}};
    std::uint32_t submitCount = 1;
    VkTimelineSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (s.returnEnabled) {
        std::uint64_t processed = 0;
        s.vk.getSemaphoreCounterValue(s.device, s.processed.semaphore, &processed);
        if (processed >= 1 && processed + 2 >= value) {
            VkCommandBuffer back = s.back[slot];
            s.vk.resetCommandBuffer(back, 0);
            s.vk.beginCommandBuffer(back, &begin);
            RecordReturn(s, back, game[0], s.output[slot]);
            s.vk.endCommandBuffer(back);
            waitInfo.waitSemaphoreValueCount = 1;
            waitInfo.pWaitSemaphoreValues = &value;
            submits[1].pNext = &waitInfo;
            submits[1].waitSemaphoreCount = 1;
            submits[1].pWaitSemaphores = &s.processed.semaphore;
            submits[1].pWaitDstStageMask = &waitStage;
            submits[1].commandBufferCount = 1;
            submits[1].pCommandBuffers = &back;
            submitCount = 2;
            s.waitingFor.store(value);
            ++s.returned;
        } else {
            ++s.notReturned;
        }
    }
    s.vk.resetFences(s.device, 1, &s.fences[slot]);
    // Pendant le verrou de la file, aucun appel Direct3D 9 depuis ce thread (documentation DXVK).
    interop->LockSubmissionQueue();
    const VkResult result = s.vk.queueSubmit(s.queue, submitCount, submits, s.fences[slot]);
    interop->ReleaseSubmissionQueue();
    if (result != VK_SUCCESS) {
        log::Error("DLSS : soumission de la copie refusée ({}), partage arrêté", static_cast<int>(result));
        s.ready = false;
        return false;
    }
    s.producedValue = value;
    ++s.shared;
    QueryPerformanceCounter(&end);
    QueryPerformanceFrequency(&frequency);
    s.cpuMs += static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
    Report(s);
    return true;
}

}  // namespace hca::dlss
