/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include "vulkan.h"

static constexpr VkSamplerAddressMode VK_SAMPLER_ADDRESS[eSamplerAddress::SAMPLER_ADDRESS_COUNT] = {
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
};

static constexpr VkFilter VK_MIN_FILTER[eSamplerFilter::SAMPLER_FILTER_COUNT] = {
    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,

    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
};

static constexpr VkFilter VK_MAG_FILTER[eSamplerFilter::SAMPLER_FILTER_COUNT] = {
    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,

    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
};

static constexpr VkSamplerMipmapMode VK_MIP_MAP_MODE[eSamplerFilter::SAMPLER_FILTER_COUNT] = {
    VK_SAMPLER_MIPMAP_MODE_NEAREST,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,

    VK_SAMPLER_MIPMAP_MODE_NEAREST,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
};

struct Sampler
{
    VkSampler_T* samplerState;
};
#endif
