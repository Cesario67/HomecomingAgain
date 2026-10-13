#include "DlssUpscaler.h"

#include <windows.h>

#include <cstring>
#include <format>

// En-têtes du SDK NVIDIA : avertissements hors de notre contrôle.
#pragma warning(push, 0)
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_vk.h>
#pragma warning(pop)

namespace hca::dlss {

namespace {

// Identifiant du projet pour NGX (NVSDK_NGX_ENGINE_TYPE_CUSTOM sans identifiant NVIDIA) : GUID propre à HomecomingAgain.
constexpr const char* kProjectId = "7c1f3a52-9b6e-4d08-a5e1-3f2c8d74b915";

std::string ResultText(NVSDK_NGX_Result result) {
    return std::format("0x{:08X}", static_cast<std::uint32_t>(result));
}

}  // namespace

struct DlssUpscaler::Impl {
    Logger log;
    VulkanContext vk;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool ngxReady = false;
    NVSDK_NGX_Parameter* parameters = nullptr;
    NVSDK_NGX_Handle* feature = nullptr;
    struct Inputs {
        VkImage color = VK_NULL_HANDLE;
        VkImage depth = VK_NULL_HANDLE;
        VkImage motion = VK_NULL_HANDLE;
        VkImageView colorView = VK_NULL_HANDLE;
        VkImageView depthView = VK_NULL_HANDLE;
        VkImageView motionView = VK_NULL_HANDLE;
    };
    std::vector<Inputs> inputs;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat motionFormat = VK_FORMAT_UNDEFINED;
    static constexpr VkFormat kOutputFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImage output = VK_NULL_HANDLE;
    VkDeviceMemory outputMemory = VK_NULL_HANDLE;
    VkImageView outputView = VK_NULL_HANDLE;
    VkImageLayout outputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    template <typename Fn>
    Fn Device(const char* name) const {
        return reinterpret_cast<Fn>(vk.getDeviceProc(vk.device, name));
    }
    template <typename Fn>
    Fn Instance(const char* name) const {
        return reinterpret_cast<Fn>(vk.getInstanceProc(vk.instance, name));
    }

    std::uint32_t FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const;
    VkImageView CreateView(VkImage image, VkFormat format) const;
    bool CreateOutput();
    bool Begin() const;
    bool SubmitAndWait() const;
    void Barrier(VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                 VkAccessFlags dstAccess) const;
};

std::uint32_t DlssUpscaler::Impl::FindMemoryType(std::uint32_t bits, VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties memory{};
    Instance<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(vk.physicalDevice,
                                                                                               &memory);
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((bits & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkImageView DlssUpscaler::Impl::CreateView(VkImage image, VkFormat format) const {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (Device<PFN_vkCreateImageView>("vkCreateImageView")(vk.device, &info, nullptr, &view) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return view;
}

bool DlssUpscaler::Impl::CreateOutput() {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = kOutputFormat;
    info.extent = {width, height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (Device<PFN_vkCreateImage>("vkCreateImage")(vk.device, &info, nullptr, &output) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements requirements{};
    Device<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(vk.device, output, &requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (Device<PFN_vkAllocateMemory>("vkAllocateMemory")(vk.device, &allocate, nullptr, &outputMemory) != VK_SUCCESS ||
        Device<PFN_vkBindImageMemory>("vkBindImageMemory")(vk.device, output, outputMemory, 0) != VK_SUCCESS) {
        return false;
    }
    outputView = CreateView(output, kOutputFormat);
    return outputView != VK_NULL_HANDLE;
}

bool DlssUpscaler::Impl::Begin() const {
    Device<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return Device<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin) == VK_SUCCESS;
}

bool DlssUpscaler::Impl::SubmitAndWait() const {
    if (Device<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands) != VK_SUCCESS) {
        return false;
    }
    Device<PFN_vkResetFences>("vkResetFences")(vk.device, 1, &fence);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    if (Device<PFN_vkQueueSubmit>("vkQueueSubmit")(vk.queue, 1, &submit, fence) != VK_SUCCESS) {
        return false;
    }
    return Device<PFN_vkWaitForFences>("vkWaitForFences")(vk.device, 1, &fence, VK_TRUE, 1'000'000'000) == VK_SUCCESS;
}

void DlssUpscaler::Impl::Barrier(VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                                 VkAccessFlags dstAccess) const {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    Device<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                                                             nullptr, 1, &barrier);
}

DlssUpscaler::DlssUpscaler(Logger log) : impl_(std::make_unique<Impl>()) {
    impl_->log = std::move(log);
}

DlssUpscaler::~DlssUpscaler() {
    Impl& s = *impl_;
    if (s.vk.device == VK_NULL_HANDLE) {
        return;
    }
    s.Device<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(s.vk.device);
    if (s.feature != nullptr) {
        NVSDK_NGX_VULKAN_ReleaseFeature(s.feature);
    }
    if (s.parameters != nullptr) {
        NVSDK_NGX_VULKAN_DestroyParameters(s.parameters);
    }
    if (s.ngxReady) {
        NVSDK_NGX_VULKAN_Shutdown1(s.vk.device);
    }
    const auto destroyView = s.Device<PFN_vkDestroyImageView>("vkDestroyImageView");
    for (const Impl::Inputs& input : s.inputs) {
        destroyView(s.vk.device, input.colorView, nullptr);
        destroyView(s.vk.device, input.depthView, nullptr);
        destroyView(s.vk.device, input.motionView, nullptr);
    }
    destroyView(s.vk.device, s.outputView, nullptr);
    s.Device<PFN_vkDestroyImage>("vkDestroyImage")(s.vk.device, s.output, nullptr);
    s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.vk.device, s.outputMemory, nullptr);
    s.Device<PFN_vkDestroyFence>("vkDestroyFence")(s.vk.device, s.fence, nullptr);
    if (s.commands != VK_NULL_HANDLE) {
        s.Device<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(s.vk.device, s.vk.pool, 1, &s.commands);
    }
}

bool DlssUpscaler::RequiredExtensions(std::vector<std::string>& instanceExtensions,
                                      std::vector<std::string>& deviceExtensions) {
    unsigned int instanceCount = 0;
    const char** instanceNames = nullptr;
    unsigned int deviceCount = 0;
    const char** deviceNames = nullptr;
    if (NVSDK_NGX_FAILED(
            NVSDK_NGX_VULKAN_RequiredExtensions(&instanceCount, &instanceNames, &deviceCount, &deviceNames))) {
        return false;
    }
    for (unsigned int i = 0; i < instanceCount; ++i) {
        instanceExtensions.emplace_back(instanceNames[i]);
    }
    for (unsigned int i = 0; i < deviceCount; ++i) {
        deviceExtensions.emplace_back(deviceNames[i]);
    }
    return true;
}

bool DlssUpscaler::Initialize(const VulkanContext& context, const Arguments& args,
                              const std::vector<SlotImages>& slots, const std::wstring& dllDirectory,
                              const std::wstring& dataDirectory) {
    Impl& s = *impl_;
    s.vk = context;
    s.width = args.width;
    s.height = args.height;
    s.colorFormat = static_cast<VkFormat>(args.color.format);
    s.depthFormat = static_cast<VkFormat>(args.depth.format);
    s.motionFormat = static_cast<VkFormat>(args.motion.format);

    // nvngx_dlss.dll est cherchée dans le dossier de ce programme (à côté de SilentHill.exe).
    const wchar_t* paths[] = {dllDirectory.c_str()};
    NVSDK_NGX_FeatureCommonInfo common{};
    common.PathListInfo.Path = paths;
    common.PathListInfo.Length = 1;
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", dataDirectory.c_str(), s.vk.instance, s.vk.physicalDevice,
        s.vk.device, s.vk.getInstanceProc, s.vk.getDeviceProc, &common);
    if (NVSDK_NGX_FAILED(result)) {
        s.log(std::format("DLSS : initialisation de NGX impossible ({})", ResultText(result)));
        return false;
    }
    s.ngxReady = true;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&s.parameters);
    if (NVSDK_NGX_FAILED(result) || s.parameters == nullptr) {
        s.log(std::format("DLSS : paramètres de NGX indisponibles ({})", ResultText(result)));
        return false;
    }
    int available = 0;
    int needsDriver = 0;
    NVSDK_NGX_Parameter_GetI(s.parameters, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    NVSDK_NGX_Parameter_GetI(s.parameters, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
    if (available == 0) {
        s.log(std::format("DLSS : non disponible sur cette carte ou ce pilote (pilote à mettre à jour : {})",
                          needsDriver != 0 ? "oui" : "non"));
        return false;
    }

    for (const SlotImages& slot : slots) {
        Impl::Inputs input;
        input.color = slot.color;
        input.depth = slot.depth;
        input.motion = slot.motion;
        input.colorView = s.CreateView(slot.color, s.colorFormat);
        input.depthView = s.CreateView(slot.depth, s.depthFormat);
        input.motionView = s.CreateView(slot.motion, s.motionFormat);
        s.inputs.push_back(input);
        if (input.colorView == VK_NULL_HANDLE || input.depthView == VK_NULL_HANDLE ||
            input.motionView == VK_NULL_HANDLE) {
            s.log("DLSS : vues des images partagées impossibles");
            return false;
        }
    }
    if (!s.CreateOutput()) {
        s.log("DLSS : image de sortie impossible");
        return false;
    }
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = s.vk.pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (s.Device<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(s.vk.device, &allocate, &s.commands) !=
            VK_SUCCESS ||
        s.Device<PFN_vkCreateFence>("vkCreateFence")(s.vk.device, &fenceInfo, nullptr, &s.fence) != VK_SUCCESS) {
        return false;
    }

    // DLAA : entrée et sortie à la même taille. Vecteurs à la résolution de rendu, image LDR (non HDR), profondeur
    // croissante avec la distance (z/w codé par le moteur, voir src/graphics/DepthEncoding.h).
    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth = s.width;
    create.Feature.InHeight = s.height;
    create.Feature.InTargetWidth = s.width;
    create.Feature.InTargetHeight = s.height;
    create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
    create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    if (!s.Begin()) {
        return false;
    }
    result = NGX_VULKAN_CREATE_DLSS_EXT1(s.vk.device, s.commands, 1, 1, &s.feature, s.parameters, &create);
    if (!s.SubmitAndWait() || NVSDK_NGX_FAILED(result) || s.feature == nullptr) {
        s.log(std::format("DLSS : création de la fonction DLSS impossible ({})", ResultText(result)));
        return false;
    }
    s.log(std::format("DLSS : prêt en mode DLAA ({}x{})", s.width, s.height));
    return true;
}

bool DlssUpscaler::Evaluate(std::size_t slot, float jitterX, float jitterY, bool reset) {
    Impl& s = *impl_;
    if (s.feature == nullptr || s.inputs.empty() || !s.Begin()) {
        return false;
    }
    const Impl::Inputs& input = s.inputs[slot % s.inputs.size()];
    // Les images partagées restent en GENERAL entre deux images (disposition attendue par le mod) : lecture par le DLSS
    // dans cette même disposition, sans transition.
    s.Barrier(input.color, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
              VK_ACCESS_SHADER_READ_BIT);
    s.Barrier(input.depth, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
              VK_ACCESS_SHADER_READ_BIT);
    s.Barrier(input.motion, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_WRITE_BIT,
              VK_ACCESS_SHADER_READ_BIT);
    s.Barrier(s.output, s.outputLayout, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_MEMORY_READ_BIT,
              VK_ACCESS_SHADER_WRITE_BIT);
    s.outputLayout = VK_IMAGE_LAYOUT_GENERAL;

    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    NVSDK_NGX_Resource_VK color = NVSDK_NGX_Create_ImageView_Resource_VK(input.colorView, input.color, range,
                                                                          s.colorFormat, s.width, s.height, false);
    NVSDK_NGX_Resource_VK depth = NVSDK_NGX_Create_ImageView_Resource_VK(input.depthView, input.depth, range,
                                                                          s.depthFormat, s.width, s.height, false);
    NVSDK_NGX_Resource_VK motion = NVSDK_NGX_Create_ImageView_Resource_VK(input.motionView, input.motion, range,
                                                                           s.motionFormat, s.width, s.height, false);
    NVSDK_NGX_Resource_VK output = NVSDK_NGX_Create_ImageView_Resource_VK(s.outputView, s.output, range,
                                                                           Impl::kOutputFormat, s.width, s.height, true);
    NVSDK_NGX_VK_DLSS_Eval_Params eval{};
    eval.Feature.pInColor = &color;
    eval.Feature.pInOutput = &output;
    eval.pInDepth = &depth;
    eval.pInMotionVectors = &motion;
    eval.InJitterOffsetX = jitterX;
    eval.InJitterOffsetY = jitterY;
    eval.InRenderSubrectDimensions = {s.width, s.height};
    eval.InReset = reset ? 1 : 0;
    eval.InMVScaleX = 1.0f;  // vecteurs déjà en pixels (position précédente - position actuelle)
    eval.InMVScaleY = 1.0f;
    const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(s.commands, s.feature, s.parameters, &eval);
    const bool submitted = s.SubmitAndWait();
    if (NVSDK_NGX_FAILED(result) || !submitted) {
        s.log(std::format("DLSS : exécution impossible ({}, soumission {})", ResultText(result),
                          submitted ? "ok" : "refusée"));
        return false;
    }
    return true;
}

bool DlssUpscaler::ReadOutput(std::vector<std::uint8_t>& bgra) {
    Impl& s = *impl_;
    if (s.output == VK_NULL_HANDLE || s.outputLayout != VK_IMAGE_LAYOUT_GENERAL) {
        return false;
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(s.width) * s.height * 4;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    s.Device<PFN_vkCreateBuffer>("vkCreateBuffer")(s.vk.device, &info, nullptr, &buffer);
    VkMemoryRequirements requirements{};
    s.Device<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(s.vk.device, buffer, &requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = s.FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    s.Device<PFN_vkAllocateMemory>("vkAllocateMemory")(s.vk.device, &allocate, nullptr, &memory);
    s.Device<PFN_vkBindBufferMemory>("vkBindBufferMemory")(s.vk.device, buffer, memory, 0);
    bool ok = s.Begin();
    if (ok) {
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {s.width, s.height, 1};
        s.Device<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(s.commands, s.output, VK_IMAGE_LAYOUT_GENERAL,
                                                                       buffer, 1, &region);
        ok = s.SubmitAndWait();
    }
    if (ok) {
        void* mapped = nullptr;
        s.Device<PFN_vkMapMemory>("vkMapMemory")(s.vk.device, memory, 0, size, 0, &mapped);
        bgra.resize(static_cast<std::size_t>(size));
        const auto* rgba = static_cast<const std::uint8_t*>(mapped);
        // R8G8B8A8 vers B8G8R8A8.
        for (std::size_t i = 0; i < bgra.size(); i += 4) {
            bgra[i] = rgba[i + 2];
            bgra[i + 1] = rgba[i + 1];
            bgra[i + 2] = rgba[i];
            bgra[i + 3] = rgba[i + 3];
        }
        s.Device<PFN_vkUnmapMemory>("vkUnmapMemory")(s.vk.device, memory);
    }
    s.Device<PFN_vkDestroyBuffer>("vkDestroyBuffer")(s.vk.device, buffer, nullptr);
    s.Device<PFN_vkFreeMemory>("vkFreeMemory")(s.vk.device, memory, nullptr);
    return ok;
}

}  // namespace hca::dlss
