/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "CommandList.h"
#include "Image.h"
#include "ImageHelpers.h"
#include "ResourceAllocationHelpers.h"

#include "Core/StringHelpers.h"

#include "vulkan.h"

static VkImageCreateFlags GetTextureCreateFlags( const ImageDesc& description )
{
    VkImageCreateFlags flagset = 0;

    if ( description.miscFlags & ImageDesc::IS_CUBE_MAP ) {
        flagset |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    } else if ( description.arraySize > 1 || description.dimension == ImageDesc::DIMENSION_3D ) {
        // NOTE Do not set VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT when VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT is set (not allowed by the specs.)
        flagset |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    return flagset;
}

static DUSK_INLINE bool IsUsingCompressedFormat( const eViewFormat format )
{
    return format == VIEW_FORMAT_BC1_TYPELESS
        || format == VIEW_FORMAT_BC1_UNORM
        || format == VIEW_FORMAT_BC1_UNORM_SRGB
        || format == VIEW_FORMAT_BC2_TYPELESS
        || format == VIEW_FORMAT_BC2_UNORM
        || format == VIEW_FORMAT_BC2_UNORM_SRGB
        || format == VIEW_FORMAT_BC3_TYPELESS
        || format == VIEW_FORMAT_BC3_UNORM
        || format == VIEW_FORMAT_BC3_UNORM_SRGB
        || format == VIEW_FORMAT_BC4_TYPELESS
        || format == VIEW_FORMAT_BC4_UNORM
        || format == VIEW_FORMAT_BC4_SNORM
        || format == VIEW_FORMAT_BC5_TYPELESS
        || format == VIEW_FORMAT_BC5_UNORM
        || format == VIEW_FORMAT_BC5_SNORM;
}

static constexpr VkImageType VK_IMAGE_TYPE_LUT[4] = {
    VkImageType::VK_IMAGE_TYPE_2D,
    VkImageType::VK_IMAGE_TYPE_1D,
    VkImageType::VK_IMAGE_TYPE_2D,
    VkImageType::VK_IMAGE_TYPE_3D,
};

static DUSK_INLINE VkImageUsageFlags GetImageUsageFlags( const ImageDesc& description )
{
    VkImageUsageFlags usageFlags = 0;

    if ( description.bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if ( description.bindFlags & RESOURCE_BIND_SHADER_RESOURCE ) {
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if ( description.bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) {
        usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if ( description.bindFlags & RESOURCE_BIND_RENDER_TARGET_VIEW ){
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    if ( description.usage == RESOURCE_USAGE_STAGING  ) {
        usageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }

    return usageFlags;
}

Image* RenderDevice::createImage( const ImageDesc& description, const void* initialData, const size_t initialDataSize )
{
    // Create image descriptor object
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.flags = GetTextureCreateFlags( description );

    VkImageType imageType = VK_IMAGE_TYPE_LUT[description.dimension];
    
    const bool isCubemap = ( description.miscFlags & ImageDesc::IS_CUBE_MAP );
    const bool isArray = ( description.arraySize > 1 );

    if ( imageType == VK_IMAGE_TYPE_2D && isArray && !isCubemap ) {
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.arrayLayers = description.arraySize * description.depth;
        imageInfo.extent.depth = 1u;
    } else {
        imageInfo.imageType = imageType;

        if ( !isCubemap ) {
            imageInfo.arrayLayers = ( imageType == VK_IMAGE_TYPE_3D ) ? 1u : description.arraySize; // if pCreateInfo->imageType is VK_IMAGE_TYPE_3D, pCreateInfo->arrayLayers must be 1.
        } else {
            imageInfo.arrayLayers = description.arraySize * description.depth; // if pCreateInfo->imageType is VK_IMAGE_TYPE_3D, pCreateInfo->arrayLayers must be 1.
        }

        imageInfo.extent.depth = ( imageType == VK_IMAGE_TYPE_2D ) ? 1u : description.depth; // if pCreateInfo->imageType is VK_IMAGE_TYPE_2D, pCreateInfo->extent.depth must be 1.
    }

    imageInfo.extent.width = static_cast<uint32_t>( description.width );
    imageInfo.extent.height = static_cast<uint32_t>( description.height );
    imageInfo.mipLevels = description.mipCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0u;
    imageInfo.pQueueFamilyIndices = VK_NULL_HANDLE;
    imageInfo.samples = GetVkSampleCount( description.samplerCount );
    imageInfo.format = VK_IMAGE_FORMAT[description.format];
    imageInfo.usage = GetImageUsageFlags( description );

    if ( initialData != nullptr ) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    Image* image = dk::core::allocate<Image>( memoryAllocator );
    image->viewType = GetViewType( description.dimension, isCubemap, isArray );
    image->defaultFormat = VK_IMAGE_FORMAT[description.format];
    image->aspectFlag = ( ( description.bindFlags & RESOURCE_BIND_DEPTH_STENCIL ) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT );
    image->mipCount = description.mipCount;
    image->arraySize = description.arraySize;
    image->resourceUsage = description.usage;
    
    size_t memorySize = initialDataSize;

    switch ( description.usage ) {
    case eResourceUsage::RESOURCE_USAGE_STATIC: {
        for ( i32 i = 0; i < 1; i ++) {
            VkImage imageObject;
            VkResult operationResult = vkCreateImage( renderContext->device, &imageInfo, nullptr, &imageObject );
            DUSK_ASSERT( ( operationResult == VK_SUCCESS ), "Failed to create image! (error code: 0x%x)\n", operationResult )

            // Allocate memory
            VkDeviceMemory deviceMemory;

            VkMemoryRequirements imageMemoryRequirements;
            vkGetImageMemoryRequirements( renderContext->device, imageObject, &imageMemoryRequirements );

            // Update memory size with proper size on the GPU side.
            memorySize = imageMemoryRequirements.size;

            VkMemoryAllocateInfo allocInfo;
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.allocationSize = imageMemoryRequirements.size;
            allocInfo.memoryTypeIndex = FindMemoryType(
                        renderContext->physicalDevice,
                        imageMemoryRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            VkResult allocationResult = vkAllocateMemory( renderContext->device, &allocInfo, nullptr, &deviceMemory );
            DUSK_ASSERT( ( allocationResult == VK_SUCCESS ), "Failed to allocate VkBuffer memory (error code: 0x%x)\n", allocationResult )

            vkBindImageMemory( renderContext->device, imageObject, deviceMemory, 0ull );

            image->resource[i] = imageObject;
            image->currentStage[i]= VK_PIPELINE_STAGE_HOST_BIT;
            image->deviceMemory[i] = deviceMemory;
        }

        // Create default view for static resources (since it'll most likely be bind as a SRV at some point).
        createImageView( *image, RenderingHelpers::IV_WholeArrayAndMipchain, 0u );

        for ( i32 i = 1; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
            image->currentStage[i]= VK_PIPELINE_STAGE_HOST_BIT;
            image->resource[i] = image->resource[0];
            image->deviceMemory[i] = image->deviceMemory[0];
        }
    } break;

    default: {
        for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
            VkImage imageObject;
            VkResult operationResult = vkCreateImage( renderContext->device, &imageInfo, nullptr, &imageObject );
            DUSK_ASSERT( ( operationResult == VK_SUCCESS ), "Failed to create image! (error code: 0x%x)\n", operationResult )

            // Allocate memory
            VkDeviceMemory deviceMemory;

            VkMemoryRequirements imageMemoryRequirements;
            vkGetImageMemoryRequirements( renderContext->device, imageObject, &imageMemoryRequirements );

            VkMemoryAllocateInfo allocInfo;
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.allocationSize = imageMemoryRequirements.size;
            allocInfo.memoryTypeIndex = FindMemoryType(
                        renderContext->physicalDevice,
                        imageMemoryRequirements.memoryTypeBits,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            VkResult allocationResult = vkAllocateMemory( renderContext->device, &allocInfo, nullptr, &deviceMemory );
            DUSK_ASSERT( ( allocationResult == VK_SUCCESS ), "Failed to allocate VkBuffer memory (error code: 0x%x)\n", allocationResult )

            vkBindImageMemory( renderContext->device, imageObject, deviceMemory, 0ull );

            image->currentStage[i]= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            image->resource[i] = imageObject;
            image->deviceMemory[i] = deviceMemory;
        }

        // Create default view for static resources (since it'll most likely be bind as a SRV at some point).
        createImageView( *image, RenderingHelpers::IV_WholeArrayAndMipchain, 0u );
    } break;
    }

    if ( initialData != nullptr ) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo stagingBufferInfo = {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.flags = 0;
        stagingBufferInfo.pNext = VK_NULL_HANDLE;
        stagingBufferInfo.size = memorySize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        DUSK_RAISE_FATAL_ERROR( vkCreateBuffer( renderContext->device, &stagingBufferInfo, nullptr, &stagingBuffer ) == VK_SUCCESS, "Staging Buffer creation FAILED!" );

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements( renderContext->device, stagingBuffer, &memRequirements );

        VkMemoryAllocateInfo allocInfo;
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = VK_NULL_HANDLE;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType( renderContext->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

        VkResult allocationResult = vkAllocateMemory( renderContext->device, &allocInfo, nullptr, &stagingBufferMemory );
        DUSK_ASSERT( ( allocationResult == VK_SUCCESS ), "Failed to allocate VkBuffer memory (error code: 0x%x)\n", allocationResult )

        vkBindBufferMemory( renderContext->device, stagingBuffer, stagingBufferMemory, 0);

        void* data;
        vkMapMemory( renderContext->device, stagingBufferMemory, 0, initialDataSize, 0, &data);
        memcpy( data, initialData, initialDataSize );
        vkUnmapMemory(renderContext->device, stagingBufferMemory);

        CommandList& cmdList = allocateCopyCommandList();
        cmdList.begin();

        NativeCommandList* nativeCmdList = cmdList.getNativeCommandList();
        if ( description.usage == eResourceUsage::RESOURCE_USAGE_STATIC ) {
            cmdList.transitionImage( *image, eResourceState::RESOURCE_STATE_COPY_DESTINATION );

            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = Max( description.arraySize, 1u );
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {
                description.width,
                description.height,
                Max( description.depth, 1u )
            };

            vkCmdCopyBufferToImage( nativeCmdList->cmdList, stagingBuffer, image->resource[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

            cmdList.transitionImage( *image, eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE );
        }
        
        cmdList.end();
        submitCommandList( cmdList );

        // TODO Defer memory release to avoid cmd queue stalling
        vkQueueWaitIdle( renderContext->graphicsQueue );

        vkDestroyBuffer( renderContext->device, stagingBuffer, nullptr);
        vkFreeMemory( renderContext->device, stagingBufferMemory, nullptr);
    }

    return image;
}

void RenderDevice::destroyImage( Image* image )
{

}

void RenderDevice::setDebugMarker( Image& image, const dkChar_t* objectName )
{
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    const i32 resCount = ( image.resourceUsage == eResourceUsage::RESOURCE_USAGE_STATIC ) ? 1 : PENDING_FRAME_COUNT;

    dkString_t str( objectName );
    std::string stdStr = DUSK_NARROW_STRING( str );

    for ( i32 i = 0; i < resCount; i++ ) {
        VkDebugMarkerObjectNameInfoEXT nameInfo;
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.pNext = VK_NULL_HANDLE;
        nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
        nameInfo.object = reinterpret_cast<uint64_t>( image.resource[i] );
        nameInfo.pObjectName = stdStr.c_str();

        renderContext->debugObjectMarker( renderContext->device, &nameInfo);
    }
#endif
}

static VkImageLayout GetLayout( const eResourceState state )
{
    switch ( state ) {
    case eResourceState::RESOURCE_STATE_UNKNOWN:
        return VK_IMAGE_LAYOUT_UNDEFINED;

    case eResourceState::RESOURCE_STATE_COPY_DESTINATION:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    case eResourceState::RESOURCE_STATE_COPY_SOURCE:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    case eResourceState::RESOURCE_STATE_UAV:
        return VK_IMAGE_LAYOUT_GENERAL;

    case eResourceState::RESOURCE_STATE_PIXEL_BINDED_RESOURCE:
    case eResourceState::RESOURCE_STATE_ALL_BINDED_RESOURCE:
    case eResourceState::RESOURCE_STATE_RESOLVE_SOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    case eResourceState::RESOURCE_STATE_RESOLVE_DESTINATION:
    case eResourceState::RESOURCE_STATE_RENDER_TARGET:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case eResourceState::RESOURCE_STATE_DEPTH_WRITE:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    case eResourceState::RESOURCE_STATE_SWAPCHAIN_BUFFER:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    case eResourceState::RESOURCE_STATE_DEPTH_READ:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    return VK_IMAGE_LAYOUT_GENERAL;
}

static DUSK_INLINE VkAccessFlags GetAccessMask( const eResourceState state )
{
    switch( state ) {
    case RESOURCE_STATE_COPY_SOURCE:
        return VK_ACCESS_TRANSFER_READ_BIT;

    case RESOURCE_STATE_COPY_DESTINATION:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

    case RESOURCE_STATE_VERTEX_BUFFER:
    case RESOURCE_STATE_CBUFFER:
        return VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

    case RESOURCE_STATE_INDEX_BUFFER:
        return VK_ACCESS_INDEX_READ_BIT;

    case RESOURCE_STATE_UAV:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    case RESOURCE_STATE_INDIRECT_ARGS:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    case RESOURCE_STATE_RENDER_TARGET:
    case RESOURCE_STATE_RESOLVE_DESTINATION:
    case RESOURCE_STATE_RESOLVE_SOURCE:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case RESOURCE_STATE_DEPTH_WRITE:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    case RESOURCE_STATE_DEPTH_READ:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    case RESOURCE_STATE_ALL_BINDED_RESOURCE:
    case RESOURCE_STATE_PIXEL_BINDED_RESOURCE:
        return VK_ACCESS_SHADER_READ_BIT;

    case RESOURCE_STATE_SWAPCHAIN_BUFFER:
        return VK_ACCESS_MEMORY_READ_BIT;
    }

    return 0;
}

static DUSK_INLINE VkPipelineStageFlags GetStageFlags( const VkAccessFlags accessFlags, const VkPipelineStageFlags psoStageFlags )
{
    if ( accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT ) {
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }

    if ( accessFlags & VK_ACCESS_INDEX_READ_BIT
      || accessFlags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT ) {
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }

    if ( accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) {
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if ( accessFlags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
      || accessFlags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) {
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    if ( accessFlags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
      || accessFlags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) {
        return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    if ( accessFlags & VK_ACCESS_TRANSFER_READ_BIT
      || accessFlags & VK_ACCESS_TRANSFER_WRITE_BIT ) {
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if ( accessFlags & VK_ACCESS_HOST_READ_BIT
      || accessFlags & VK_ACCESS_HOST_WRITE_BIT ) {
        return VK_PIPELINE_STAGE_HOST_BIT;
    }

    if ( accessFlags & VK_ACCESS_UNIFORM_READ_BIT
    ||  accessFlags & VK_ACCESS_SHADER_READ_BIT
    ||  accessFlags & VK_ACCESS_SHADER_WRITE_BIT ) {
        return psoStageFlags;
    }

    return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}

void CommandList::transitionImage( Image& image, const eResourceState state, const u32 mipIndex, const TransitionType transitionType )
{
    const eResourceState previousState = image.currentState[resourceFrameIndex];

    VkImageLayout oldLayout = GetLayout( previousState );
    VkImageLayout newLayout = GetLayout( state );

    if  ( oldLayout == newLayout && transitionType == TRANSITION_SAME_QUEUE ) {
        return;
    }

    VkAccessFlags srcAccess = GetAccessMask( previousState );
    VkAccessFlags dstAccess = GetAccessMask( state );

    VkPipelineStageFlags stageFlags = ( commandListType == CommandList::Type::GRAPHICS ) ? VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkPipelineStageFlags srcStage =  image.currentStage[resourceFrameIndex];
    VkPipelineStageFlags dstStage = GetStageFlags( dstAccess, stageFlags );

    u32 srcQueueIdx = VK_QUEUE_FAMILY_IGNORED;
    u32 dstQueueIdx = VK_QUEUE_FAMILY_IGNORED;

    if ( transitionType == CommandList::TRANSITION_GRAPHICS_TO_COMPUTE ) {
        srcQueueIdx = nativeCommandList->graphicsQueueIndex;
        dstQueueIdx = nativeCommandList->computeQueueIndex;

        nativeCommandList->waitForPreviousCmdList = true;
    } else  if ( transitionType == CommandList::TRANSITION_COMPUTE_TO_GRAPHICS ) {
        srcQueueIdx = nativeCommandList->computeQueueIndex;
        dstQueueIdx = nativeCommandList->graphicsQueueIndex;

        nativeCommandList->waitForPreviousCmdList = true;
    } else  if ( transitionType == CommandList::TRANSITION_GRAPHICS_TO_PRESENT ) {
        srcQueueIdx = nativeCommandList->graphicsQueueIndex;
        dstQueueIdx = nativeCommandList->presentQueueIndex;

        nativeCommandList->waitForPreviousCmdList = true;
    } else  if ( transitionType == CommandList::TRANSITION_COMPUTE_TO_PRESENT ) {
        srcQueueIdx = nativeCommandList->computeQueueIndex;
        dstQueueIdx = nativeCommandList->presentQueueIndex;

        nativeCommandList->waitForPreviousCmdList = true;
    }

    VkImageMemoryBarrier imgBarrier;
    imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgBarrier.pNext = VK_NULL_HANDLE;
    imgBarrier.srcAccessMask = srcAccess;
    imgBarrier.dstAccessMask = dstAccess;
    imgBarrier.oldLayout = oldLayout;
    imgBarrier.newLayout = newLayout;
    imgBarrier.srcQueueFamilyIndex = srcQueueIdx;
    imgBarrier.dstQueueFamilyIndex = dstQueueIdx;
    imgBarrier.image = image.resource[resourceFrameIndex];
    imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgBarrier.subresourceRange.baseMipLevel = mipIndex;
    imgBarrier.subresourceRange.levelCount = 1;
    imgBarrier.subresourceRange.baseArrayLayer = 0;
    imgBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier( nativeCommandList->cmdList, srcStage, dstStage, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &imgBarrier );

    image.currentLayout[resourceFrameIndex] = newLayout;
    image.currentState[resourceFrameIndex] = state;
    image.currentStage[resourceFrameIndex] = dstStage;
}

void CommandList::insertComputeBarrier( Image& image )
{

}

void CreateImageView( VkDevice device, const ImageViewDesc& viewDescription, Image& image )
{
    const bool isArrayView = ( viewDescription.ImageCount > 1 );
    const u32 mipCount = ( viewDescription.MipCount <= 0 ) ? image.mipCount : viewDescription.MipCount;
    const u32 imgCount = ( viewDescription.ImageCount <= 0 ) ? image.arraySize : viewDescription.ImageCount;
    const VkFormat viewFormat = ( viewDescription.ViewFormat == eViewFormat::VIEW_FORMAT_UNKNOWN ) ? image.defaultFormat : VK_IMAGE_FORMAT[viewDescription.ViewFormat];
    const VkImageViewType viewType = ( isArrayView ) ? image.viewType : GetPerSliceViewType( image.viewType );
    const bool isCubemap = ( viewType == VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE || viewType == VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY );

    VkImageViewCreateInfo imageViewDesc;
    imageViewDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewDesc.pNext = nullptr;
    imageViewDesc.flags = 0u;
    imageViewDesc.viewType = viewType;
    imageViewDesc.format = viewFormat;

    imageViewDesc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewDesc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewDesc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewDesc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewDesc.subresourceRange.baseMipLevel = viewDescription.StartMipIndex;
    imageViewDesc.subresourceRange.levelCount = mipCount;
    imageViewDesc.subresourceRange.baseArrayLayer = viewDescription.StartImageIndex;
    imageViewDesc.subresourceRange.layerCount = ( isCubemap && ( ( imgCount % 6 ) == 0 ) ) ? 6 : imgCount; // The view must cover all the faces of the cubemap.
    imageViewDesc.subresourceRange.aspectMask = image.aspectFlag;

    // Create the view for each internally buffered resource.
    const i32 resCount = ( image.resourceUsage == eResourceUsage::RESOURCE_USAGE_STATIC ) ? 1 : RenderDevice::PENDING_FRAME_COUNT;
    for ( i32 i = 0; i < resCount; i++ ) {
        imageViewDesc.image = image.resource[i];

        VkImageView imageView;
        VkResult creationResult = vkCreateImageView( device, &imageViewDesc, VK_NULL_HANDLE, &imageView );
        DUSK_RAISE_FATAL_ERROR( creationResult == VK_SUCCESS, "Image view creation failed! (error code %i)", creationResult );

        image.renderTargetView[i][viewDescription.SortKey] = imageView;
    }
}

void RenderDevice::createImageView( Image& image, const ImageViewDesc& viewDescription, const u32 creationFlags )
{
    // NOTE We do not need to take in account IMAGE_VIEW_CREATE_* flags (this is only required for older gen. API like
    // D3D11, GNM, etc.).
    if ( creationFlags & IMAGE_VIEW_COVER_WHOLE_MIPCHAIN ) {
        ImageViewDesc perMipViewDescription = viewDescription;
        for ( u32 i = 0; i < image.mipCount; i++ ) {
            perMipViewDescription.StartMipIndex = i;
            perMipViewDescription.MipCount = 1;

            CreateImageView( renderContext->device, perMipViewDescription, image );
        }
    } else {
        CreateImageView( renderContext->device, viewDescription, image );
    }
}

void CommandList::setupFramebuffer( FramebufferAttachment* renderTargetViews, FramebufferAttachment depthStencilView )
{
    VkImageView imageViews[24];
    VkClearValue rtClear[24];
    u32 clearCount = 0;

    PipelineState* pipelineState = nativeCommandList->BindedPipelineState;
    DUSK_DEV_ASSERT( ( pipelineState != nullptr ), "Trying to setup a Framebuffer without a PipelineState bind!" )

    const FramebufferLayoutDesc& fboLayout = pipelineState->fboLayout;

    u32 attachmentCount = fboLayout.getAttachmentCount();
    for ( u32 i = 0; i < attachmentCount; i++ ) {
        const FramebufferLayoutDesc::AttachmentDesc& attachment = fboLayout.Attachments[i];

        Image* imageResource = renderTargetViews[i].ImageAttachment;
        imageViews[i] = imageResource->renderTargetView[attachment.viewFormat][resourceFrameIndex];

        // The cmdlist will write to the swapchain; add an implicit sync.
        if ( attachment.bindMode == FramebufferLayoutDesc::WRITE_SWAPCHAIN ) {
            nativeCommandList->waitForSwapchainRetrival = true;
        }

        if ( fboLayout.Attachments[i].targetState == FramebufferLayoutDesc::InitialState::CLEAR ) {
            rtClear[clearCount++] = nativeCommandList->BindedPipelineState->defaultClearValue;
        }
    }

    if ( fboLayout.depthStencilAttachment.bindMode != FramebufferLayoutDesc::BindMode::UNUSED ) {
        Image* depthStencilImage = depthStencilView.ImageAttachment;

        imageViews[attachmentCount++] = depthStencilImage->renderTargetView[fboLayout.depthStencilAttachment.viewFormat][resourceFrameIndex];
        rtClear[clearCount++] = nativeCommandList->BindedPipelineState->defaultDepthClearValue;
    }

    if ( pipelineState->framebuffer[resourceFrameIndex] == VK_NULL_HANDLE ) {
        VkFramebufferCreateInfo framebufferCreateInfos;
        framebufferCreateInfos.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfos.pNext = nullptr;
        framebufferCreateInfos.flags = 0u;
        framebufferCreateInfos.renderPass = pipelineState->renderPass;
        framebufferCreateInfos.attachmentCount = attachmentCount;
        framebufferCreateInfos.pAttachments = imageViews;
        framebufferCreateInfos.width = static_cast<uint32_t>( nativeCommandList->activeViewport.Width );
        framebufferCreateInfos.height = static_cast<uint32_t>( nativeCommandList->activeViewport.Height );
        framebufferCreateInfos.layers = 1u;

        DUSK_ASSERT( vkCreateFramebuffer( nativeCommandList->device, &framebufferCreateInfos, nullptr, &pipelineState->framebuffer[resourceFrameIndex] ) == VK_SUCCESS, "Failed to created FB" )
   }

    VkRenderPassBeginInfo renderPassInfo;
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.renderPass = pipelineState->renderPass;
    renderPassInfo.framebuffer = pipelineState->framebuffer[resourceFrameIndex];
    renderPassInfo.renderArea.extent.width = static_cast<uint32_t>( nativeCommandList->activeViewport.Width );
    renderPassInfo.renderArea.extent.height = static_cast<uint32_t>( nativeCommandList->activeViewport.Height );
    renderPassInfo.renderArea.offset.x = nativeCommandList->activeViewport.X;
    renderPassInfo.renderArea.offset.y = nativeCommandList->activeViewport.Y;
    renderPassInfo.clearValueCount = clearCount;
    renderPassInfo.pClearValues = rtClear;

    vkCmdBeginRenderPass( nativeCommandList->cmdList, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    nativeCommandList->isInRenderPass =  true;
}

void CommandList::clearRenderTargets( Image** renderTargetViews, const u32 renderTargetCount, const f32 clearValues[4] )
{

}

void CommandList::clearDepthStencil( Image* depthStencilView, const f32 clearValue, const bool clearStencil, const u8 clearStencilValue )
{

}

void CommandList::resolveImage( Image& src, Image& dst )
{

}
#endif
