#include "dlss/SharedFrames.h"

#include <windows.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <d3d9_vk_interop.h>
#include <vulkan/vulkan_win32.h>

#include "core/Config.h"
#include "core/Log.h"
#include "dlss/DxvkInterop.h"

namespace hca::dlss {

namespace {

constexpr std::uint32_t kSlots = 3;
constexpr ULONGLONG kReportIntervalMs = 10000;

// Fonctions Vulkan chargées depuis le périphérique de DXVK (vulkan-1.dll est déjà chargé par DXVK).
struct VulkanFunctions {
    PFN_vkGetPhysicalDeviceMemoryProperties getMemoryProperties = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkGetMemoryWin32HandleKHR getMemoryWin32Handle = nullptr;
    PFN_vkCreateSemaphore createSemaphore = nullptr;
    PFN_vkGetSemaphoreWin32HandleKHR getSemaphoreWin32Handle = nullptr;
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
};

struct SharedImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    HANDLE handle = nullptr;  // handle NT exporté, gardé ouvert pour le processus DLSS
    VkExtent3D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool initialized = false;  // disposition passée de UNDEFINED à GENERAL
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
    SharedImage color;
    SharedImage depth;
    VkSemaphore timeline = VK_NULL_HANDLE;
    HANDLE timelineHandle = nullptr;
    std::uint64_t timelineValue = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer commands[kSlots]{};
    VkFence fences[kSlots]{};
    std::uint64_t frame = 0;
    // Statistiques journalisées toutes les 10 s.
    std::uint32_t shared = 0;
    std::uint32_t skipped = 0;
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
    if (getDeviceProc == nullptr || s.vk.getMemoryProperties == nullptr) {
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
    return ok;
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

bool CreateSharedImage(State& s, const VkImageCreateInfo& source, SharedImage& out, const char* name) {
    VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.pNext = &external;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = source.format;
    info.extent = {source.extent.width, source.extent.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
    log::Info("DLSS : image partagée {} {}x{}, format Vulkan {}, {} Ko, handle 0x{:X}", name, out.extent.width,
              out.extent.height, static_cast<int>(out.format), requirements.size / 1024,
              reinterpret_cast<std::uintptr_t>(out.handle));
    return true;
}

bool CreateSync(State& s) {
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkExportSemaphoreCreateInfo exportInfo{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
    exportInfo.pNext = &type;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphore.pNext = &exportInfo;
    if (s.vk.createSemaphore(s.device, &semaphore, nullptr, &s.timeline) == VK_SUCCESS) {
        VkSemaphoreGetWin32HandleInfoKHR handleInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR};
        handleInfo.semaphore = s.timeline;
        handleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        if (s.vk.getSemaphoreWin32Handle(s.device, &handleInfo, &s.timelineHandle) == VK_SUCCESS) {
            log::Info("DLSS : sémaphore timeline exporté, handle 0x{:X}",
                      reinterpret_cast<std::uintptr_t>(s.timelineHandle));
        }
    } else {
        log::Warn("DLSS : sémaphore timeline exportable impossible (synchronisation par le processus DLSS à revoir)");
        s.timeline = VK_NULL_HANDLE;
    }

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = s.queueFamily;
    if (s.vk.createCommandPool(s.device, &poolInfo, nullptr, &s.pool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = s.pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = kSlots;
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

bool Initialize(State& s, IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) {
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
    if (!LoadFunctions(s) || !GameImage(sceneColor, image, colorInfo) || !GameImage(depth, image, depthInfo)) {
        return false;
    }
    return CreateSharedImage(s, colorInfo, s.color, "scène") && CreateSharedImage(s, depthInfo, s.depth, "profondeur") &&
           CreateSync(s);
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

void RecordCopy(State& s, VkCommandBuffer commands, VkImage gameColor, VkImage gameDepth) {
    // Images du jeu : laissées en GENERAL (disposition rapportée par DXVK) ; dépendance sur tout le travail déjà soumis.
    const VkImageMemoryBarrier before[4] = {
        Barrier(gameColor, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT),
        Barrier(gameDepth, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT),
        Barrier(s.color.image, s.color.initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT),
        Barrier(s.depth.image, s.depth.initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT)};
    s.vk.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                            0, nullptr, 4, before);
    s.color.initialized = true;
    s.depth.initialized = true;

    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.extent = s.color.extent;
    s.vk.cmdCopyImage(commands, gameColor, VK_IMAGE_LAYOUT_GENERAL, s.color.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    region.extent = s.depth.extent;
    s.vk.cmdCopyImage(commands, gameDepth, VK_IMAGE_LAYOUT_GENERAL, s.depth.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);

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
    log::Info("DLSS : images partagées {}, sautées {}, temps processeur moyen {:.3f} ms, compteur {}", s.shared,
              s.skipped, s.shared > 0 ? s.cpuMs / s.shared : 0.0, s.timelineValue);
    s.shared = 0;
    s.skipped = 0;
    s.cpuMs = 0.0;
}

}  // namespace

void ShareFrame(IDirect3DBaseTexture9* sceneColor, IDirect3DBaseTexture9* depth) noexcept {
    State& s = g_state;
    if (!GetConfig().dlssShareFrames || sceneColor == nullptr || depth == nullptr) {
        return;
    }
    if (!s.attempted) {
        s.attempted = true;
        s.ready = Initialize(s, sceneColor, depth);
        log::Info("DLSS : partage des images {}", s.ready ? "prêt" : "indisponible");
    }
    if (!s.ready) {
        return;
    }
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    LARGE_INTEGER frequency{};
    QueryPerformanceCounter(&start);

    const std::uint32_t slot = SlotForFrame(s.frame++, kSlots);
    if (s.vk.getFenceStatus(s.device, s.fences[slot]) != VK_SUCCESS) {
        ++s.skipped;  // copie précédente de cet emplacement pas encore terminée : on ne bloque pas le jeu
        Report(s);
        return;
    }
    VkImage gameColor = VK_NULL_HANDLE;
    VkImage gameDepth = VK_NULL_HANDLE;
    VkImageCreateInfo unused{};
    if (!GameImage(sceneColor, gameColor, unused) || !GameImage(depth, gameDepth, unused)) {
        ++s.skipped;
        Report(s);
        return;
    }
    ID3D9VkInteropDevice* interop = Interop();
    // Le travail Direct3D 9 en attente doit être soumis avant le nôtre (documentation de l'interface DXVK).
    interop->FlushRenderingCommands();

    VkCommandBuffer commands = s.commands[slot];
    s.vk.resetCommandBuffer(commands, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    s.vk.beginCommandBuffer(commands, &begin);
    RecordCopy(s, commands, gameColor, gameDepth);
    s.vk.endCommandBuffer(commands);

    const std::uint64_t signalValue = s.timelineValue + 1;
    VkTimelineSemaphoreSubmitInfo timelineInfo{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    if (s.timeline != VK_NULL_HANDLE) {
        submit.pNext = &timelineInfo;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &s.timeline;
    }
    s.vk.resetFences(s.device, 1, &s.fences[slot]);
    // Pendant le verrou de la file, aucun appel Direct3D 9 depuis ce thread (documentation DXVK).
    interop->LockSubmissionQueue();
    const VkResult result = s.vk.queueSubmit(s.queue, 1, &submit, s.fences[slot]);
    interop->ReleaseSubmissionQueue();
    if (result != VK_SUCCESS) {
        log::Error("DLSS : soumission de la copie refusée ({}), partage arrêté", static_cast<int>(result));
        s.ready = false;
        return;
    }
    if (s.timeline != VK_NULL_HANDLE) {
        s.timelineValue = signalValue;
    }
    ++s.shared;
    QueryPerformanceCounter(&end);
    QueryPerformanceFrequency(&frequency);
    s.cpuMs += static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart);
    Report(s);
}

}  // namespace hca::dlss
