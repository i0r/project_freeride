/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <vulkan/vulkan.h>
#include <vector>

#include <Rendering/RenderDevice.h>

struct RenderContext
{
    VkInstance                          instance;
    VkPhysicalDevice                    physicalDevice;
    VkDevice                            device; 
    dkStringHash_t*                     instanceExtensionHashes;
    u32                                 instanceExtensionCount;
    std::vector<VkExtensionProperties>  deviceExtensionList;

#if DUSK_DEVBUILD
    PFN_vkDebugMarkerSetObjectNameEXT   debugObjectMarker;
    VkDebugUtilsMessengerEXT            debugCallback;
#endif


    PFN_vkCmdDebugMarkerBeginEXT        vkCmdDebugMarkerBegin;
    PFN_vkCmdDebugMarkerEndEXT          vkCmdDebugMarkerEnd;

    VkQueue                             graphicsQueue;
    VkQueue                             computeQueue;
    VkQueue                             presentQueue;
    
    VkSurfaceKHR                        displaySurface;

    u32                                 graphicsQueueIndex;
    u32                                 computeQueueIndex;
    u32                                 presentQueueIndex;

    VkExtent2D                          swapChainExtent;
    VkFormat                            swapChainFormat;

    VkSwapchainKHR              swapChain;
    VkPresentModeKHR             presentMode;

    VkFence                                 swapChainFence[RenderDevice::PENDING_FRAME_COUNT];

    VkDescriptorPool                      descriptorPool[RenderDevice::PENDING_FRAME_COUNT];

    VkCommandPool                       graphicsCommandPool[RenderDevice::PENDING_FRAME_COUNT];
    VkCommandPool                       computeCommandPool[RenderDevice::PENDING_FRAME_COUNT];

    VkSemaphore frameSemaphores[RenderDevice::PENDING_FRAME_COUNT][33];
    u32 frameSemaphoresCount[RenderDevice::PENDING_FRAME_COUNT];
};
#endif
