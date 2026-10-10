// Interfaces d'interopérabilité Direct3D 9 / Vulkan de DXVK (v3.1.1, fichier src/d3d9/d3d9_interfaces.h), réduites
// à ce que HomecomingAgain utilise. Les types Vulkan sont déclarés de façon opaque pour ne pas dépendre des en-têtes
// Vulkan ; la disposition des vtables (ordre et signature des méthodes) est celle d'origine.
//
// Copyright (c) 2017 Philip Rebohle, (c) 2019 Joshua Ashton, (c) 2019 Robin Kertels. Licence zlib : voir LICENSE.txt
// dans ce dossier. Modifié : extrait et types Vulkan opaques.

#pragma once

#include <d3d9.h>

#include <cstdint>

// Handles Vulkan : « dispatchables » (pointeurs) et non dispatchables (entiers 64 bits en 32 bits).
typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef std::uint64_t VkImage;
typedef std::int32_t VkImageLayout;      // énumération Vulkan
typedef std::uint32_t VkImageUsageFlags;
struct VkImageCreateInfo;                // non utilisé ici (on passe nullptr)
struct VkImageSubresourceRange;

struct D3D9VkExtImageDesc;

MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(VkImage* pHandle, VkImageLayout* pLayout,
                                                         VkImageCreateInfo* pInfo) = 0;
};

MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(VkInstance* pInstance, VkPhysicalDevice* pPhysDev,
                                                    VkDevice* pDevice) = 0;
    virtual void STDMETHODCALLTYPE GetSubmissionQueue(VkQueue* pQueue, std::uint32_t* pQueueIndex,
                                                      std::uint32_t* pQueueFamilyIndex) = 0;
    virtual void STDMETHODCALLTYPE TransitionTextureLayout(ID3D9VkInteropTexture* pTexture,
                                                           const VkImageSubresourceRange* pSubresources,
                                                           VkImageLayout OldLayout, VkImageLayout NewLayout) = 0;
    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;
    virtual void STDMETHODCALLTYPE LockDevice() = 0;
    virtual void STDMETHODCALLTYPE UnlockDevice() = 0;
    virtual bool STDMETHODCALLTYPE WaitForResource(IDirect3DResource9* pResource, DWORD MapFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateImage(const D3D9VkExtImageDesc* desc, IDirect3DResource9** ppResult) = 0;
};
