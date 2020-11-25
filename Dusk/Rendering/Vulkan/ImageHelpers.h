/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include "vulkan.h"

#include "Core/ViewFormat.h"

#include "Rendering/RenderDevice.h"
#include "RenderDevice.h"

// Look Up Table to retrieve the number of bytes used for a single pixel with a given format.
// (ChannelCount * PerChannelPrecisionSize). Will return 0 if the format is invalid/unavailable.
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

// Look up table to convert an abstract eViewFormat to a VkFormat.
// If there is no matching VkFormat, VK_FORMAT_UNDEFINED will be used.
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

// Return the matching single-layered view type for an array view type.
// If the given imageViewType is a single layered view type (e.g. 2D) the same view type will be returned by the function.
static VkImageViewType GetPerSliceViewType( const VkImageViewType imageViewType, const u32 sliceCount = 1u )
{
    switch ( imageViewType ) {
    case VkImageViewType::VK_IMAGE_VIEW_TYPE_1D:
    case VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;

    case VkImageViewType::VK_IMAGE_VIEW_TYPE_2D:
    case VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY:
    case VkImageViewType::VK_IMAGE_VIEW_TYPE_3D:
    case VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;

    case VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        return ( ( sliceCount % 6 ) == 0 ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;

    default:
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    }
}

// Return the matching view type for a given abstract image dimension. If isArrayImage is true, the function will return
// the array view type matching the dimension provided (e.g. 2D_ARRAY for a 2D dimension with isArrayImage=true).
static VkImageViewType GetViewType( const decltype( ImageDesc::dimension ) dimension, const bool isArrayImage = false, const bool isCubemap = false )
{
    switch ( dimension ) {
    case ImageDesc::DIMENSION_1D:
    {
        return ( ( isArrayImage ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_1D );
    } break;

    case ImageDesc::DIMENSION_2D:
    {
        if ( isCubemap ) {
            return ( ( isArrayImage ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE );
        } else {
            return ( ( isArrayImage ) ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D );
        }
    } break;

    case ImageDesc::DIMENSION_3D:
    {
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
    } break;

    default:
    {
        return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    } break;
    };
}

// Return the matching VkSampleCountFlagBits for a given integer sample count. If the given sampleCount is invalid
// (e.g. exceed the maximum sample count defined by the standard), the function will return VK_SAMPLE_COUNT_1_BIT.
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
#endif
