/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <vulkan/vulkan.h>

#include <Core/ViewFormat.h>
#include <Rendering/Shared.h>

struct Image
{
    VkImage         resource[RenderDevice::PENDING_FRAME_COUNT];
    VkDeviceMemory     deviceMemory[RenderDevice::PENDING_FRAME_COUNT];
    VkImageView     renderTargetView[eViewFormat::VIEW_FORMAT_COUNT][RenderDevice::PENDING_FRAME_COUNT];
    VkImageViewType viewType;
    VkFormat                defaultFormat;
    VkImageAspectFlagBits aspectFlag;
    u32 arraySize;
    u32 mipCount;
    eResourceUsage resourceUsage;
    eResourceState currentState[RenderDevice::PENDING_FRAME_COUNT];
    VkImageLayout currentLayout[RenderDevice::PENDING_FRAME_COUNT];
    VkPipelineStageFlags currentStage[RenderDevice::PENDING_FRAME_COUNT];

    Image() {
        memset( resource, 0, sizeof( VkImage ) * RenderDevice::PENDING_FRAME_COUNT );
        memset( deviceMemory, 0, sizeof( VkDeviceMemory ) * RenderDevice::PENDING_FRAME_COUNT );
        memset( renderTargetView, 0, sizeof( VkImageView ) * eViewFormat::VIEW_FORMAT_COUNT * RenderDevice::PENDING_FRAME_COUNT );
        memset( currentState, 0, sizeof( eResourceState ) * RenderDevice::PENDING_FRAME_COUNT );
        memset( currentLayout, 0, sizeof( VkImageLayout ) * RenderDevice::PENDING_FRAME_COUNT );
        memset( currentStage, 0, sizeof( VkPipelineStageFlagBits ) * RenderDevice::PENDING_FRAME_COUNT );

        resourceUsage = eResourceUsage::RESOURCE_USAGE_STATIC;
    }
};
#endif
