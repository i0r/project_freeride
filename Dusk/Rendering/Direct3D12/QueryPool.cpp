/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "QueryPool.h"
#include "Buffer.h"

#include <d3d12.h>

static constexpr D3D12_QUERY_HEAP_TYPE QUERY_HEAP_TYPE_LUT[eQueryType::QUERY_TYPE_COUNT] = {
    D3D12_QUERY_HEAP_TYPE_TIMESTAMP
};

static constexpr D3D12_QUERY_TYPE QUERY_TYPE_LUT[eQueryType::QUERY_TYPE_COUNT] = {
    D3D12_QUERY_TYPE_TIMESTAMP
};

QueryPool* RenderDevice::createQueryPool( const eQueryType type, const u32 poolCapacity )
{
    QueryPool* pool = dk::core::allocate<QueryPool>( memoryAllocator );
    pool->queryType = QUERY_TYPE_LUT[type];
    pool->heapCapacity = poolCapacity;

    D3D12_QUERY_HEAP_DESC heapDesc;
    heapDesc.Type = QUERY_HEAP_TYPE_LUT[type];
    heapDesc.NodeMask = 0;
    heapDesc.Count = poolCapacity;

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        renderContext->device->CreateQueryHeap( &heapDesc, IID_PPV_ARGS( &pool->queryHeap[i] ) );
    }

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        pool->heapUsage[i] = 0;
        pool->previousHeapUsage[i] = 0;
        pool->queryRetrievalMark[i] = 0;
    }
    
    // Create a staging buffer to read back query results
    BufferDesc readbackBuffer;
    readbackBuffer.Usage = RESOURCE_USAGE_STAGING;
    readbackBuffer.SizeInBytes = sizeof( u64 ) * PENDING_FRAME_COUNT * poolCapacity;
    readbackBuffer.BindFlags = RESOURCE_BIND_UNBINDABLE;

    pool->readbackBuffer = createBuffer( readbackBuffer );

    return pool;
}

void RenderDevice::destroyQueryPool( QueryPool* queryPool )
{
    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        queryPool->queryHeap[i]->Release();
    }

    destroyBuffer( queryPool->readbackBuffer );

    dk::core::free( memoryAllocator, queryPool );
}

f64 RenderDevice::convertTimestampToMs( const u64 timestamp ) const
{
    return static_cast<f64>( timestamp * 1000.0 ) / renderContext->directQueueTimestampFreq;
}

void CommandList::retrieveQueryResults( QueryPool& queryPool, const u32 startQueryIndex, const u32 queryCount )
{
    u32 queryStartIndex = ( startQueryIndex == QUERY_START_AT_RETRIEVAL_MARK ) ? queryPool.queryRetrievalMark[resourceFrameIndex] : startQueryIndex;
    u32 queryToRetrieveCount = ( queryCount == QUERY_COUNT_WHOLE_POOL ) ? queryPool.heapCapacity : queryCount;

    // Read back query heap data
    nativeCommandList->graphicsCmdList->ResolveQueryData( 
        queryPool.queryHeap[resourceFrameIndex], 
        queryPool.queryType,
        queryStartIndex,
        queryToRetrieveCount,
        queryPool.readbackBuffer->resource[0],
        static_cast<UINT64>( resourceFrameIndex * queryPool.heapCapacity + queryStartIndex ) * sizeof( u64 )
    );

    // If we retrieve result for the current frame, assume those results will be used and retrieved to the CPU later
    // We need to keep track of the query start index since the query allocator is circular
    queryPool.queryRetrievalMark[resourceFrameIndex] = queryPool.heapUsage[resourceFrameIndex];
}

void CommandList::getQueryResult( QueryPool& queryPool, u64* resultsArray, const u32 startQueryIndex, const u32 queryCount )
{
    DUSK_DEV_ASSERT( resultsArray, "Query result array 'resultsArray' is null, expect a crash soon..." );

    u32 queryToRetrieveCount = ( queryCount == QUERY_COUNT_WHOLE_POOL ) ? queryPool.heapCapacity : queryCount;
    u32 availableFrameResIdx = static_cast< u32 >( resourceFrameIndex + 2 ) % RenderDevice::PENDING_FRAME_COUNT;
    u32 queryStartIndex = ( startQueryIndex == QUERY_START_AT_RETRIEVAL_MARK ) ? queryPool.previousHeapUsage[availableFrameResIdx] : startQueryIndex;

    // NOTE Range has no effects on the accessibility of the data; it is only used as a hint
    D3D12_RANGE bindRange;
    bindRange.Begin = ( availableFrameResIdx * queryPool.heapCapacity ) + queryStartIndex * sizeof( u64 );
    bindRange.End = bindRange.Begin + queryToRetrieveCount * sizeof( u64 );

    u64* queryResultPtr = nullptr;
    if ( FAILED( queryPool.readbackBuffer->resource[0]->Map( 0, &bindRange, reinterpret_cast<void**>( &queryResultPtr ) ) )
        || queryResultPtr == nullptr ) {
        DUSK_LOG_ERROR( "Failed to map QueryPool staging buffer!\n" );
        DUSK_TRIGGER_BREAKPOINT;
        return;
    }

    // Advance the buffer pointer to the available data
    queryResultPtr += ( availableFrameResIdx * queryPool.heapCapacity ) + queryStartIndex;
    memcpy( resultsArray, queryResultPtr, queryToRetrieveCount * sizeof( u64 ) );
    queryPool.readbackBuffer->resource[0]->Unmap( 0, nullptr );

    // Update previous heap usage to figure out the start of the allocation done during the current frame for the current frame + 1 frame
    queryPool.previousHeapUsage[availableFrameResIdx] = queryPool.heapUsage[availableFrameResIdx];
}

u32 CommandList::allocateQuery( QueryPool& queryPool )
{
    const u32 queryIdx = queryPool.heapUsage[resourceFrameIndex];
    queryPool.heapUsage[resourceFrameIndex] = ( ++queryPool.heapUsage[resourceFrameIndex] % queryPool.heapCapacity );
    return queryIdx;
}

void CommandList::beginQuery( QueryPool& queryPool, const u32 queryIndex )
{
    nativeCommandList->graphicsCmdList->BeginQuery( queryPool.queryHeap[resourceFrameIndex], queryPool.queryType, queryIndex );
}

void CommandList::endQuery( QueryPool& queryPool, const u32 queryIndex )
{
    nativeCommandList->graphicsCmdList->EndQuery( queryPool.queryHeap[resourceFrameIndex], queryPool.queryType, queryIndex );
}

void CommandList::writeTimestamp( QueryPool& queryPool, const u32 queryIndex )
{
    nativeCommandList->graphicsCmdList->EndQuery( queryPool.queryHeap[resourceFrameIndex], D3D12_QUERY_TYPE_TIMESTAMP, queryIndex );
}
#endif
