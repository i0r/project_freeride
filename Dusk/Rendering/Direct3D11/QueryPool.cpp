/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "QueryPool.h"
#include "RenderDevice.h"
#include "CommandList.h"

#include <d3d11.h>

static constexpr D3D11_QUERY D3D11_QUERY_TYPE[eQueryType::QUERY_TYPE_COUNT] =
{
    D3D11_QUERY_TIMESTAMP,
};

QueryPool* RenderDevice::createQueryPool( const eQueryType type, const u32 poolCapacity )
{
    QueryPool* queryPool = dk::core::allocate<QueryPool>( memoryAllocator );
    queryPool->targetType = D3D11_QUERY_TYPE[type];
    queryPool->queryHandles = dk::core::allocateArray<ID3D11Query*>( memoryAllocator, poolCapacity );

    queryPool->capacity = poolCapacity;
    queryPool->currentAllocableIndex = 0;

    // Create a pool of disjoint queries if the main pool is a timestamp one
    if ( queryPool->targetType == D3D11_QUERY_TIMESTAMP ) {
        queryPool->currentDisjointAllocableIndex = 0;
        queryPool->isRecordingDisjointQuery = false;
        queryPool->disjointQueryHandles = dk::core::allocateArray<ID3D11Query*>( memoryAllocator, poolCapacity );
        queryPool->disjointQueriesResult.resize( poolCapacity );
        queryPool->queryDisjointTable.resize( poolCapacity );

        std::fill_n( queryPool->disjointQueriesResult.begin(), poolCapacity, 0ull );

        D3D11_QUERY_DESC disjointQueryDesc;
        disjointQueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        disjointQueryDesc.MiscFlags = 0;

        for ( u32 queryIdx = 0; queryIdx < poolCapacity; queryIdx++ ) {
            renderContext->PhysicalDevice->CreateQuery( &disjointQueryDesc, &queryPool->disjointQueryHandles[queryIdx] );
        }
    }

    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = queryPool->targetType;
    queryDesc.MiscFlags = 0;

    for ( u32 queryIdx = 0; queryIdx < poolCapacity; queryIdx++ ) {
        renderContext->PhysicalDevice->CreateQuery( &queryDesc, &queryPool->queryHandles[queryIdx] );
    }

    return queryPool;
}

void RenderDevice::destroyQueryPool( QueryPool* queryPool )
{
    for ( u32 queryIdx = 0; queryIdx < queryPool->capacity; queryIdx++ ) {
        queryPool->queryHandles[queryIdx]->Release();
    }
    dk::core::free( memoryAllocator, queryPool->queryHandles );

    if ( queryPool->targetType == D3D11_QUERY_TIMESTAMP ) {
        for ( u32 queryIdx = 0; queryIdx < queryPool->capacity; queryIdx++ ) {
            queryPool->disjointQueryHandles[queryIdx]->Release();
        }

        dk::core::free( memoryAllocator, queryPool->disjointQueryHandles );
    }
}

static u64 DEVICE_CLOCK_FREQUENCY = 1;

f64 RenderDevice::convertTimestampToMs( const u64 timestamp ) const
{
    return static_cast< f64 >( timestamp ) / DEVICE_CLOCK_FREQUENCY * 1000.0;
}

void CommandList::retrieveQueryResults( QueryPool& queryPool, const u32 startQueryIndex, const u32 queryCount )
{
    // Retrieval is done at driver level in D3D11
}

void CommandList::getQueryResult( QueryPool& queryPool, u64* resultsArray, const u32 startQueryIndex, const u32 queryCount )
{
    ID3D11DeviceContext* deviceContext = nativeCommandList->ImmediateContext;

    i32 retrievedResultCount = 0;
    for ( u32 i = startQueryIndex; i < ( startQueryIndex + queryCount ); i++ ) {
        ID3D11Query* queryHandle = queryPool.queryHandles[i];
        
        u64& queryResult = resultsArray[retrievedResultCount++];
        
        HRESULT dataRetrieveResult = deviceContext->GetData( queryHandle, &queryResult, sizeof( uint64_t ), 0 );
        if ( FAILED( dataRetrieveResult ) ) {
            return;
        }

        if ( queryPool.targetType == D3D11_QUERY_TIMESTAMP ) {
            u32 disjointQueryIndex = queryPool.queryDisjointTable[i];

            if ( queryPool.disjointQueriesResult[disjointQueryIndex] == 0 ) {
                ID3D11Query* disjointQueryHandle = queryPool.disjointQueryHandles[queryPool.queryDisjointTable[i]];

                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointQueryResult;
                HRESULT dataDisjointRetrieveResult = deviceContext->GetData( disjointQueryHandle, &disjointQueryResult, sizeof( D3D11_QUERY_DATA_TIMESTAMP_DISJOINT ), 0 );

                // Disjoint query has failed, meaning that the results will be unreliable
                if ( FAILED( dataDisjointRetrieveResult ) || disjointQueryResult.Disjoint ) {
                    return;
                }

                DEVICE_CLOCK_FREQUENCY = disjointQueryResult.Frequency;
            }
        }
    }
}

u32 CommandList::allocateQuery( QueryPool& queryPool )
{
    queryPool.currentAllocableIndex = ( ++queryPool.currentAllocableIndex % queryPool.capacity );
    return queryPool.currentAllocableIndex;
}

void CommandList::beginQuery( QueryPool& queryPool, const u32 queryIndex )
{
    nativeCommandList->ImmediateContext->Begin( queryPool.queryHandles[queryIndex] );
}

void CommandList::endQuery( QueryPool& queryPool, const u32 queryIndex )
{
    nativeCommandList->ImmediateContext->End( queryPool.queryHandles[queryIndex] );
}

void CommandList::writeTimestamp( QueryPool& queryPool, const u32 queryIndex )
{
    // Update disjoint queries
    if ( !queryPool.isRecordingDisjointQuery ) {
        nativeCommandList->ImmediateContext->Begin( queryPool.disjointQueryHandles[queryIndex] );
        queryPool.disjointQueriesResult[queryIndex] = 0;
        queryPool.currentDisjointAllocableIndex = queryIndex;
    } else {
        nativeCommandList->ImmediateContext->End( queryPool.disjointQueryHandles[queryPool.currentDisjointAllocableIndex] );
    }

    queryPool.queryDisjointTable[queryIndex] = queryPool.currentDisjointAllocableIndex;
    queryPool.isRecordingDisjointQuery = !queryPool.isRecordingDisjointQuery;

    // Write timestamp
    nativeCommandList->ImmediateContext->End( queryPool.queryHandles[queryIndex] );
}
#endif
