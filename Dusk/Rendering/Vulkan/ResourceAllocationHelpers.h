/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include "vulkan.h"

DUSK_INLINE static VkBufferUsageFlags GetResourceUsageFlags( const u32 bindFlags )
{
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if ( bindFlags == RESOURCE_BIND_UNBINDABLE ) {
        return usageFlags;
    }

    if ( bindFlags & RESOURCE_BIND_CONSTANT_BUFFER ) {
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if ( bindFlags & RESOURCE_BIND_VERTEX_BUFFER ) {
        usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if ( bindFlags & RESOURCE_BIND_INDICE_BUFFER ) {
        usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if ( bindFlags & RESOURCE_BIND_UNORDERED_ACCESS_VIEW ) {
        usageFlags |= ( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT );
    }

    if ( bindFlags & RESOURCE_BIND_STRUCTURED_BUFFER ) {
        usageFlags |= ( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT );
    }

    if ( bindFlags & RESOURCE_BIND_INDIRECT_ARGUMENTS ) {
        usageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }

    return usageFlags;
}

DUSK_INLINE u32 FindMemoryType( VkPhysicalDevice physicalDevice, const u32 typeFilter, const VkMemoryPropertyFlags properties )
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memProperties );

    for ( u32 i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( typeFilter & ( 1 << i ) ) && ( memProperties.memoryTypes[i].propertyFlags & properties ) == properties ) {
            return i;
        }
    }

    return ~0u;
}

DUSK_INLINE VkMemoryPropertyFlags GetMemoryPropertyFlags( const eResourceUsage usage ) {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    switch ( usage ) {
    case eResourceUsage::RESOURCE_USAGE_STAGING:
    case eResourceUsage::RESOURCE_USAGE_DYNAMIC:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    case eResourceUsage::RESOURCE_USAGE_STATIC:
    case eResourceUsage::RESOURCE_USAGE_DEFAULT:
        return VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
}
#endif
