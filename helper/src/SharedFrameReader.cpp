#include "SharedFrameReader.h"

#include <windows.h>

#include <vulkan/vulkan_win32.h>

#include <algorithm>
#include <cstring>
#include <format>

namespace hca::dlss {

struct SharedFrameReader::Impl {
    Arguments args;
    Logger log;
    HMODULE library = nullptr;
    PFN_vkGetInstanceProcAddr getInstanceProc = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkImage color[kSlots]{};
    VkImage depth[kSlots]{};
    VkDeviceMemory colorMemory[kSlots]{};
    VkDeviceMemory depthMemory[kSlots]{};
    VkImage motion[kSlots]{};
    VkDeviceMemory motionMemory[kSlots]{};
    VkImage output[kSlots]{};  // sorties du DLSS rendues au jeu (facultatives)
    VkDeviceMemory outputMemory[kSlots]{};
    VkSemaphore processed = VK_NULL_HANDLE;  // « traitées » (signalé par ce processus)
    VkSemaphore timeline = VK_NULL_HANDLE;  // « produites »
    VkSemaphore consumed = VK_NULL_HANDLE;  // « consommées »
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<std::string> instanceExtensions;  // demandées par le DLSS
    std::vector<std::string> deviceExtensions;

    template <typename Fn>
    Fn Instance(const char* name) {
        return reinterpret_cast<Fn>(getInstanceProc(instance, name));
    }
    template <typename Fn>
    Fn Device(const char* name) {
        return reinterpret_cast<Fn>(getDeviceProc(device, name));
    }

    bool CreateInstance();
    bool CreateDevice();
    bool ImportImage(const SharedImageArgs& shared, std::uint64_t handle, VkImage& image, VkDeviceMemory& memory,
                     const std::string& name, VkImageUsageFlags extraUsage = 0);
    bool ImportSemaphore(std::uint64_t handle, VkSemaphore& semaphore, const char* name);
    std::uint32_t FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags);
};

bool SharedFrameReader::Impl::CreateInstance() {
    library = LoadLibraryW(L"vulkan-1.dll");
    getInstanceProc = library != nullptr
                          ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library, "vkGetInstanceProcAddr"))
                          : nullptr;
    if (getInstanceProc == nullptr) {
        log("vulkan-1.dll introuvable");
        return false;
    }
    const auto create = Instance<PFN_vkCreateInstance>("vkCreateInstance");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "HomecomingAgain.Dlss";
    app.apiVersion = VK_API_VERSION_1_2;
    std::vector<const char*> extensions;
    for (const std::string& name : instanceExtensions) {
        extensions.push_back(name.c_str());
    }
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    if (create == nullptr || create(&info, nullptr, &instance) != VK_SUCCESS) {
        log("création de l'instance Vulkan impossible");
        return false;
    }
    getDeviceProc = Instance<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    return getDeviceProc != nullptr;
}

bool SharedFrameReader::Impl::CreateDevice() {
    const auto enumerate = Instance<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
    const auto properties = Instance<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
    const auto queueProperties =
        Instance<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
    const auto createDevice = Instance<PFN_vkCreateDevice>("vkCreateDevice");
    std::uint32_t count = 0;
    enumerate(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    enumerate(instance, &count, devices.data());
    // Même carte que DXVK (fabricant et modèle transmis par le mod) : la mémoire exportée n'est valable que là.
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties p{};
        properties(candidate, &p);
        if (p.vendorID == args.vendorId && p.deviceID == args.deviceId) {
            physicalDevice = candidate;
            log(std::format("carte : {}", p.deviceName));
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        log(std::format("carte 0x{:04X}:0x{:04X} introuvable", args.vendorId, args.deviceId));
        return false;
    }
    queueProperties(physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    queueProperties(physicalDevice, &count, families.data());
    for (std::uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            queueFamily = i;
            break;
        }
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    // Extensions du partage avec le mod, plus celles du DLSS, sans doublon.
    std::vector<std::string> names = {VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
                                      VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME};
    for (const std::string& name : deviceExtensions) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }
    std::vector<const char*> extensions;
    for (const std::string& name : names) {
        extensions.push_back(name.c_str());
    }
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.timelineSemaphore = VK_TRUE;
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.pNext = &features12;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    const VkResult created = createDevice(physicalDevice, &info, nullptr, &device);
    if (created != VK_SUCCESS) {
        log(std::format("création du périphérique Vulkan impossible ({}, {} extensions)", static_cast<int>(created),
                        extensions.size()));
        return false;
    }
    Device<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device, queueFamily, 0, &queue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    return Device<PFN_vkCreateCommandPool>("vkCreateCommandPool")(device, &poolInfo, nullptr, &pool) == VK_SUCCESS;
}

std::uint32_t SharedFrameReader::Impl::FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties memory{};
    Instance<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physicalDevice, &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool SharedFrameReader::Impl::ImportImage(const SharedImageArgs& shared, std::uint64_t handle, VkImage& image,
                                          VkDeviceMemory& memory, const std::string& name,
                                          VkImageUsageFlags extraUsage) {
    // Paramètres identiques à ceux du mod (dlss/SharedFrames.cpp, CreateSharedImage) : obligatoire pour l'import.
    VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.pNext = &external;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = static_cast<VkFormat>(shared.format);
    info.extent = {args.width, args.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extraUsage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (Device<PFN_vkCreateImage>("vkCreateImage")(device, &info, nullptr, &image) != VK_SUCCESS) {
        log(std::format("image {} : création impossible", name));
        return false;
    }
    VkMemoryRequirements requirements{};
    Device<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(device, image, &requirements);
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    dedicated.image = image;
    VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};
    import.pNext = &dedicated;
    import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    import.handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(handle));
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.pNext = &import;
    allocate.allocationSize = shared.allocationSize;
    allocate.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    const VkResult result = Device<PFN_vkAllocateMemory>("vkAllocateMemory")(device, &allocate, nullptr, &memory);
    if (result != VK_SUCCESS ||
        Device<PFN_vkBindImageMemory>("vkBindImageMemory")(device, image, memory, 0) != VK_SUCCESS) {
        log(std::format("image {} : import de la mémoire impossible ({})", name, static_cast<int>(result)));
        return false;
    }
    log(std::format("image {} importée ({}x{}, format {}, {} Ko)", name, args.width, args.height, shared.format,
                    shared.allocationSize / 1024));
    return true;
}

bool SharedFrameReader::Impl::ImportSemaphore(std::uint64_t handle, VkSemaphore& semaphore, const char* name) {
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    info.pNext = &type;
    if (Device<PFN_vkCreateSemaphore>("vkCreateSemaphore")(device, &info, nullptr, &semaphore) != VK_SUCCESS) {
        return false;
    }
    VkImportSemaphoreWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};
    import.semaphore = semaphore;
    import.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    import.handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(handle));
    const VkResult result =
        Device<PFN_vkImportSemaphoreWin32HandleKHR>("vkImportSemaphoreWin32HandleKHR")(device, &import);
    log(std::format("compteur « {} » importé : {}", name,
                    result == VK_SUCCESS ? "oui" : std::format("non ({})", static_cast<int>(result))));
    return result == VK_SUCCESS;
}

SharedFrameReader::SharedFrameReader(const Arguments& args, Logger log) : impl_(std::make_unique<Impl>()) {
    impl_->args = args;
    impl_->log = std::move(log);
}

SharedFrameReader::~SharedFrameReader() {
    Impl& s = *impl_;
    if (s.device != VK_NULL_HANDLE) {
        s.Device<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(s.device);
        s.Device<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(s.device, s.pool, nullptr);
        s.Device<PFN_vkDestroySemaphore>("vkDestroySemaphore")(s.device, s.timeline, nullptr);
        s.Device<PFN_vkDestroySemaphore>("vkDestroySemaphore")(s.device, s.consumed, nullptr);
        s.Device<PFN_vkDestroySemaphore>("vkDestroySemaphore")(s.device, s.processed, nullptr);
        for (std::size_t i = 0; i < kSlots; ++i) {
            s.Device<PFN_vkDestroyImage>("vkDestroyImage")(s.device, s.color[i], nullptr);
            s.Device<PFN_vkDestroyImage>("vkDestroyImage")(s.device, s.depth[i], nullptr);
            s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.device, s.colorMemory[i], nullptr);
            s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.device, s.depthMemory[i], nullptr);
            s.Device<PFN_vkDestroyImage>("vkDestroyImage")(s.device, s.motion[i], nullptr);
            s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.device, s.motionMemory[i], nullptr);
            s.Device<PFN_vkDestroyImage>("vkDestroyImage")(s.device, s.output[i], nullptr);
            s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.device, s.outputMemory[i], nullptr);
        }
        s.Device<PFN_vkDestroyDevice>("vkDestroyDevice")(s.device, nullptr);
    }
    if (s.instance != VK_NULL_HANDLE) {
        s.Instance<PFN_vkDestroyInstance>("vkDestroyInstance")(s.instance, nullptr);
    }
}

void SharedFrameReader::AddExtensions(const std::vector<std::string>& instanceExtensions,
                                      const std::vector<std::string>& deviceExtensions) {
    impl_->instanceExtensions.insert(impl_->instanceExtensions.end(), instanceExtensions.begin(),
                                     instanceExtensions.end());
    impl_->deviceExtensions.insert(impl_->deviceExtensions.end(), deviceExtensions.begin(), deviceExtensions.end());
}

VulkanContext SharedFrameReader::Context() const {
    const Impl& s = *impl_;
    return {s.instance,  s.physicalDevice,  s.device,        s.queue,    s.queueFamily,
            s.pool,      s.getInstanceProc, s.getDeviceProc, s.processed};
}

SlotImages SharedFrameReader::Images(std::size_t slot) const {
    const Impl& s = *impl_;
    return {s.color[slot % kSlots], s.depth[slot % kSlots], s.motion[slot % kSlots], s.output[slot % kSlots]};
}

bool SharedFrameReader::Open() {
    Impl& s = *impl_;
    if (!s.CreateInstance() || !s.CreateDevice()) {
        return false;
    }
    for (std::size_t i = 0; i < kSlots; ++i) {
        if (!s.ImportImage(s.args.color, s.args.color.handles[i], s.color[i], s.colorMemory[i], std::format("scène {}", i)) ||
            !s.ImportImage(s.args.depth, s.args.depth.handles[i], s.depth[i], s.depthMemory[i],
                           std::format("profondeur {}", i)) ||
            !s.ImportImage(s.args.motion, s.args.motion.handles[i], s.motion[i], s.motionMemory[i],
                           std::format("mouvement {}", i))) {
            return false;
        }
    }
    if (!s.ImportSemaphore(s.args.semaphore, s.timeline, "produites") ||
        !s.ImportSemaphore(s.args.consumed, s.consumed, "consommées")) {
        return false;
    }
    // Retour vers le jeu (facultatif) : sorties écrites par le DLSS (STORAGE), même création que dans le mod.
    if (s.args.processed != 0) {
        for (std::size_t i = 0; i < kSlots; ++i) {
            if (!s.ImportImage(s.args.output, s.args.output.handles[i], s.output[i], s.outputMemory[i],
                               std::format("sortie {}", i), VK_IMAGE_USAGE_STORAGE_BIT)) {
                return false;
            }
        }
        if (!s.ImportSemaphore(s.args.processed, s.processed, "traitées")) {
            return false;
        }
    }
    return true;
}

std::uint64_t SharedFrameReader::WaitNext(std::uint64_t lastValue, std::uint32_t timeoutMs) {
    Impl& s = *impl_;
    const std::uint64_t target = lastValue + 1;
    VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &s.timeline;
    wait.pValues = &target;
    if (s.Device<PFN_vkWaitSemaphores>("vkWaitSemaphores")(s.device, &wait,
                                                            static_cast<std::uint64_t>(timeoutMs) * 1000000) !=
        VK_SUCCESS) {
        return lastValue;
    }
    std::uint64_t value = lastValue;
    s.Device<PFN_vkGetSemaphoreCounterValue>("vkGetSemaphoreCounterValue")(s.device, s.timeline, &value);
    return value;
}

void SharedFrameReader::MarkConsumed(std::uint64_t value) {
    Impl& s = *impl_;
    VkSemaphoreSignalInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
    signal.semaphore = s.consumed;
    signal.value = value;
    s.Device<PFN_vkSignalSemaphore>("vkSignalSemaphore")(s.device, &signal);
}

bool SharedFrameReader::ReadBack(std::size_t slot, std::vector<std::uint8_t>& color, std::vector<float>& depth,
                                 std::vector<std::uint16_t>& motion) {
    Impl& s = *impl_;
    const VkDeviceSize pixels = static_cast<VkDeviceSize>(s.args.width) * s.args.height;
    const VkDeviceSize size = pixels * 4;  // 4 octets par pixel pour les trois formats (BGRA8, R32F, R16G16F)
    VkBuffer buffers[3]{};
    VkDeviceMemory memories[3]{};
    for (int i = 0; i < 3; ++i) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        s.Device<PFN_vkCreateBuffer>("vkCreateBuffer")(s.device, &info, nullptr, &buffers[i]);
        VkMemoryRequirements requirements{};
        s.Device<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(s.device, buffers[i],
                                                                                       &requirements);
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = s.FindMemoryType(
            requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        s.Device<PFN_vkAllocateMemory>("vkAllocateMemory")(s.device, &allocate, nullptr, &memories[i]);
        s.Device<PFN_vkBindBufferMemory>("vkBindBufferMemory")(s.device, buffers[i], memories[i], 0);
    }
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = s.pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    s.Device<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(s.device, &allocate, &commands);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    s.Device<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin);
    // Les images partagées restent en GENERAL (dispositions posées par le mod) : pas de passage par UNDEFINED, qui
    // autoriserait le pilote à jeter leur contenu.
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {s.args.width, s.args.height, 1};
    const auto copy = s.Device<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer");
    copy(commands, s.color[slot % kSlots], VK_IMAGE_LAYOUT_GENERAL, buffers[0], 1, &region);
    copy(commands, s.depth[slot % kSlots], VK_IMAGE_LAYOUT_GENERAL, buffers[1], 1, &region);
    copy(commands, s.motion[slot % kSlots], VK_IMAGE_LAYOUT_GENERAL, buffers[2], 1, &region);
    s.Device<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    const bool ok = s.Device<PFN_vkQueueSubmit>("vkQueueSubmit")(s.queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                    s.Device<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(s.queue) == VK_SUCCESS;
    if (ok) {
        void* mapped = nullptr;
        color.resize(static_cast<std::size_t>(size));
        depth.resize(static_cast<std::size_t>(pixels));
        s.Device<PFN_vkMapMemory>("vkMapMemory")(s.device, memories[0], 0, size, 0, &mapped);
        std::memcpy(color.data(), mapped, static_cast<std::size_t>(size));
        s.Device<PFN_vkUnmapMemory>("vkUnmapMemory")(s.device, memories[0]);
        s.Device<PFN_vkMapMemory>("vkMapMemory")(s.device, memories[1], 0, size, 0, &mapped);
        std::memcpy(depth.data(), mapped, static_cast<std::size_t>(size));
        s.Device<PFN_vkUnmapMemory>("vkUnmapMemory")(s.device, memories[1]);
        motion.resize(static_cast<std::size_t>(pixels) * 2);
        s.Device<PFN_vkMapMemory>("vkMapMemory")(s.device, memories[2], 0, size, 0, &mapped);
        std::memcpy(motion.data(), mapped, static_cast<std::size_t>(size));
        s.Device<PFN_vkUnmapMemory>("vkUnmapMemory")(s.device, memories[2]);
    }
    s.Device<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(s.device, s.pool, 1, &commands);
    for (int i = 0; i < 3; ++i) {
        s.Device<PFN_vkDestroyBuffer>("vkDestroyBuffer")(s.device, buffers[i], nullptr);
        s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.device, memories[i], nullptr);
    }
    return ok;
}

}  // namespace hca::dlss
