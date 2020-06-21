/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"

#include <vulkan/vulkan.h>

struct QueryPool
{
    VkQueryPool   queryPool;
    unsigned int    currentAllocableIndex;
    unsigned int    capacity;
};

QueryPool* RenderDevice::createQueryPool( const eQueryType type, const u32 poolCapacity )
{
    QueryPool* queryPool = dk::core::allocate<QueryPool>( memoryAllocator );

    VkQueryPoolCreateInfo queryPoolCreateInfos = {};
    queryPoolCreateInfos.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfos.pNext = nullptr;
    queryPoolCreateInfos.flags = 0;

    if ( type == eQueryType::QUERY_TYPE_TIMESTAMP ) {
        queryPoolCreateInfos.queryType = VkQueryType::VK_QUERY_TYPE_TIMESTAMP;
    }

    queryPoolCreateInfos.queryCount = poolCapacity;
    queryPoolCreateInfos.pipelineStatistics = 0;

    VkQueryPool nativeQueryPool = nullptr;
    vkCreateQueryPool( renderContext->device, &queryPoolCreateInfos, nullptr, &nativeQueryPool );

    queryPool->queryPool = nativeQueryPool;
    queryPool->currentAllocableIndex = 0;
    queryPool->capacity = poolCapacity;

    return queryPool;
}

void RenderDevice::destroyQueryPool( QueryPool* queryPool )
{
    vkDestroyQueryPool( renderContext->device, queryPool->queryPool, nullptr );
}

f64 RenderDevice::convertTimestampToMs( const u64 timestamp ) const
{
    return static_cast<f64>( timestamp ) / 1000000.0;
}

void CommandList::retrieveQueryResults( QueryPool& queryPool, const u32 startQueryIndex, const u32 queryCount )
{

}

void CommandList::getQueryResult( QueryPool& queryPool, u64* resultsArray, const u32 startQueryIndex, const u32 queryCount )
{

}

u32 CommandList::allocateQuery( QueryPool& queryPool )
{
    u32 queryIdx = queryPool.currentAllocableIndex;
    queryPool.currentAllocableIndex = ++queryPool.currentAllocableIndex % queryPool.capacity;

    return queryIdx;
}

void CommandList::beginQuery( QueryPool& queryPool, const u32 queryIndex )
{
    vkCmdBeginQuery( nativeCommandList->cmdList, queryPool.queryPool, queryIndex, 0 );
}

void CommandList::endQuery( QueryPool& queryPool, const u32 queryIndex )
{
    vkCmdEndQuery( nativeCommandList->cmdList, queryPool.queryPool, queryIndex );
}

void CommandList::writeTimestamp( QueryPool& queryPool, const u32 queryIndex )
{
    vkCmdWriteTimestamp( nativeCommandList->cmdList, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, queryPool.queryPool, queryIndex );
}
#endif
