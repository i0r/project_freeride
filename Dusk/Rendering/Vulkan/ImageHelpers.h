/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/ViewFormat.h>

#if DUSK_VULKAN
#include <vulkan/vulkan.h>

#include <Rendering/RenderDevice.h>

#include "RenderDevice.h"

// Channel * PerChannelSize
static constexpr size_t VK_VIEW_FORMAT_SIZE[VIEW_FORMAT_COUNT] =
{
    0ull,
    0ull,
    16ull,
    16ull,
    16ull,
    0ull,
    12ull,
    12ull,
    12ull,
    0ull,
    8ull,
    8ull,
    8ull,
    8ull,
    8ull,
    0ull,
    8ull,
    8ull,
    8ull,
    0ull,
    8ull,
    0ull,
    0ull,
    0ull,
    4ull,
    4ull,
    4ull,
    0ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    0ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,
    4ull,

    4ull,
    0ull,
    0ull,
    0ull,

    0ull,
    2ull,
    2ull,
    2ull,
    2ull,

    0ull,
    2ull,

    2ull,
    2ull,
    2ull,
    2ull,
    2ull,

    0ull,
    1ull,
    1ull,
    1ull,
    1ull,

    0ull,
    0ull,
    0ull,
    0ull,
    0ull,

    8ull,
    8ull,
    8ull,

    16ull,
    16ull,
    16ull,

    16ull,
    16ull,
    16ull,

    8ull,
    8ull,
    8ull,

    16ull,
    16ull,
    16ull,

    2ull,
    2ull,

    4ull,
    4ull,

    0ull,
    0ull,

    4ull,
    0ull,
    0ull,

    0ull,
    16ull,
    16ull,

    0ull,
    16ull,
    16ull
};

static constexpr VkFormat VK_IMAGE_FORMAT[VIEW_FORMAT_COUNT] =
{
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R32G32B32A32_UINT,
    VK_FORMAT_R32G32B32A32_SINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32B32_UINT,
    VK_FORMAT_R32G32B32_SINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R16G16B16A16_UINT,
    VK_FORMAT_R16G16B16A16_SNORM,
    VK_FORMAT_R16G16B16A16_SINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32_UINT,
    VK_FORMAT_R32G32_SINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_A2R10G10B10_UNORM_PACK32,
    VK_FORMAT_A2R10G10B10_UINT_PACK32,
    VK_FORMAT_B10G11R11_UFLOAT_PACK32,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_R8G8B8A8_UINT,
    VK_FORMAT_R8G8B8A8_SNORM,
    VK_FORMAT_R8G8B8A8_SINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R16G16_SFLOAT,
    VK_FORMAT_R16G16_UNORM,
    VK_FORMAT_R16G16_UINT,
    VK_FORMAT_R16G16_SNORM,
    VK_FORMAT_R16G16_SINT,
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_R32_SFLOAT,
    VK_FORMAT_R32_UINT,
    VK_FORMAT_R32_SINT,

    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R8G8_UNORM,
    VK_FORMAT_R8G8_UINT,
    VK_FORMAT_R8G8_SNORM,
    VK_FORMAT_R8G8_SINT,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R16_SFLOAT,

    VK_FORMAT_D16_UNORM,
    VK_FORMAT_R16_UNORM,
    VK_FORMAT_R16_UINT,
    VK_FORMAT_R16_SNORM,
    VK_FORMAT_R16_SINT,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_R8_UNORM,
    VK_FORMAT_R8_UINT,
    VK_FORMAT_R8_SNORM,
    VK_FORMAT_R8_SINT,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,

    VK_FORMAT_BC1_RGB_UNORM_BLOCK,
    VK_FORMAT_BC1_RGB_UNORM_BLOCK,
    VK_FORMAT_BC1_RGB_SRGB_BLOCK,

    VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC2_UNORM_BLOCK,
    VK_FORMAT_BC2_SRGB_BLOCK,

    VK_FORMAT_BC3_UNORM_BLOCK,
    VK_FORMAT_BC3_UNORM_BLOCK,
    VK_FORMAT_BC3_SRGB_BLOCK,

    VK_FORMAT_BC4_UNORM_BLOCK,
    VK_FORMAT_BC4_UNORM_BLOCK,
    VK_FORMAT_BC4_SNORM_BLOCK,

    VK_FORMAT_BC5_UNORM_BLOCK,
    VK_FORMAT_BC5_UNORM_BLOCK,
    VK_FORMAT_BC5_SNORM_BLOCK,

    VK_FORMAT_B5G6R5_UNORM_PACK16,
    VK_FORMAT_B5G5R5A1_UNORM_PACK16,

    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_B8G8R8A8_UNORM,
        
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,

    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_UNDEFINED,
    VK_FORMAT_UNDEFINED,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_BC6H_UFLOAT_BLOCK,
    VK_FORMAT_BC6H_SFLOAT_BLOCK,

    VK_FORMAT_UNDEFINED,
    VK_FORMAT_BC7_UNORM_BLOCK,
    VK_FORMAT_BC7_SRGB_BLOCK
};

static VkImageViewType GetViewType( const ImageDesc& description, const bool perSliceView = false )
{
    switch ( description.dimension ) {
    case ImageDesc::DIMENSION_1D:
        return ( ( description.arraySize > 1 && !perSliceView ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_1D );

    case ImageDesc::DIMENSION_2D:
    {
        if ( description.miscFlags & ImageDesc::IS_CUBE_MAP && !perSliceView ) {
            return ( ( description.arraySize > 1 ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE );
        } else {
            return ( ( description.arraySize > 1 && !perSliceView ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D );
        }
    } break;

    case ImageDesc::DIMENSION_3D:
    {
        return  VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
    }

    default:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    }
}

static VkSampleCountFlagBits GetVkSampleCount( const u32 sampleCount )
{
    switch ( sampleCount ) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;
    default:
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

static VkImageView CreateImageView( VkDevice device, const VkImage image, const VkImageViewType viewType, const VkFormat format, const VkImageAspectFlagBits aspect, const u32 sliceIndex = 0u, const u32 sliceCount = ~0u, const u32 mipIndex = 0u, const u32 mipCount = ~0u )
{
    const bool isPerSliceView = ( sliceIndex != 0 );

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0u;
    createInfo.image = image;
    createInfo.viewType = viewType;
    createInfo.format = format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.baseMipLevel = mipIndex;
    createInfo.subresourceRange.levelCount = mipCount;
    createInfo.subresourceRange.baseArrayLayer = sliceIndex;
    createInfo.subresourceRange.layerCount = sliceCount;

    VkImageView imageView;
    vkCreateImageView( device, &createInfo, nullptr, &imageView );

    return imageView;
}
#endif
