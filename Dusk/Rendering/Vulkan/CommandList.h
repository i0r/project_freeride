/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include "vulkan.h"

#include <unordered_map>
#include "PipelineState.h"

#include <Rendering/CommandList.h>

struct NativeCommandList
{
                                                                NativeCommandList();
                                                                ~NativeCommandList();

    VkCommandBuffer                             cmdList;
    PFN_vkCmdDebugMarkerBeginEXT   vkCmdDebugMarkerBegin;
    PFN_vkCmdDebugMarkerEndEXT      vkCmdDebugMarkerEnd;

    VkDevice                                              device; // Required to do descriptorSet allocation from descriptorSetPool (sadly)
    VkDescriptorPool                                descriptorPool; // renderDevice->descriptorPool[currentFrameResIdx]

    Viewport                                               activeViewport;
    PipelineState*                                      BindedPipelineState;

    i32                                     bufferInfosCount;
    i32                                     imageInfosCount;
    u32                                    writeDescriptorSetsCount;
    u32                                    cmdQueueIdx;

    // Required for explicit ownership release/acquire barriers
    u32                                 graphicsQueueIndex;
    u32                                 computeQueueIndex;
    u32                                 presentQueueIndex;

    // If true, the renderdevice will add a semaphore to sync this cmdlist with the swapchain retrival
    bool                                    waitForSwapchainRetrival;

    // Crappy cmd queue sync test
    bool                                    waitForPreviousCmdList;

    // TODO For debug purposes, should be removed later
    bool                                    isInRenderPass;

    VkDescriptorBufferInfo  bufferInfos[256];
    VkDescriptorImageInfo imageInfos[256];
    VkDescriptorSet             activeDescriptorSets[8];
    VkWriteDescriptorSet    writeDescriptorSets[256];
};
#endif
