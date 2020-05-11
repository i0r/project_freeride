/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <vulkan/vulkan.h>
#include <unordered_map>

#include <Rendering/RenderDevice.h>

struct Sampler;

struct PipelineState
{
    VkPipeline                     pipelineObject;
    VkPipelineLayout          layout;
    VkRenderPass                renderPass; // Graphics PSO only

    VkPipelineStageFlags    stageFlags;

    VkPipelineCache            pipelineCache;
    FramebufferLayoutDesc        fboLayout;
    VkFramebuffer               framebuffer[RenderDevice::PENDING_FRAME_COUNT];

    VkDescriptorSetLayout  descriptorSetLayouts[8];
    u32                                   descriptorSetCount;

    VkClearValue                    defaultClearValue;
    VkClearValue                    defaultDepthClearValue;

    VkSampler                        immutableSamplers[8];
    i32                                     immutableSamplerCount;

    struct ResourceBinding {
        u32 descriptorSet;
        u32 bindingIndex;
        VkDescriptorType type;
        size_t offset;
        size_t range;
    };
    std::unordered_map<dkStringHash_t, ResourceBinding>  bindingSet;
};
#endif
