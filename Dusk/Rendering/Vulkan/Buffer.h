/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <vulkan/vulkan.h>
#include <Rendering/RenderDevice.h>

struct Buffer
{
    VkBuffer                    resource[RenderDevice::PENDING_FRAME_COUNT];
    VkDeviceMemory     deviceMemory[RenderDevice::PENDING_FRAME_COUNT];
    VkBufferView            bufferView;
    size_t                         Stride;
};
#endif
