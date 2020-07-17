/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "PipelineStateCache.h"

#include "ShaderCache.h"

#include <Core/Hashing/MurmurHash3.h>
#include <Core/BitsManipulation.h>
#include <Core/Allocators/LinearAllocator.h>
#include <Rendering/RenderDevice.h>
#include <FileSystem/VirtualFileSystem.h>

static constexpr size_t PSO_BLOB_CACHE_SIZE = ( 4 << 20 ); // Size (in bytes) of the memory block allocated for cached PSO blob read

DUSK_DEV_VAR_PERSISTENT( DisablePipelineCache, true, bool ); // "Disables Pipeline State (+Root signature on D3D12) caching." [false/true]

PipelineStateCache::PipelineStateCache( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVFS )
    : memoryAllocator( allocator )
    , renderDevice( activeRenderDevice )
    , virtualFs( activeVFS )
    , shaderCache( dk::core::allocate<ShaderCache>( allocator, allocator, activeRenderDevice, activeVFS ) )
    , cachedPipelineStateCount( 0 )
{
    memset( pipelineHashes, 0, sizeof( Hash128 ) * MAX_CACHE_ELEMENT_COUNT );
    memset( pipelineStates, 0, sizeof( PipelineState* ) * MAX_CACHE_ELEMENT_COUNT );

    pipelineStateCacheAllocator = dk::core::allocate<LinearAllocator>( memoryAllocator, 4 << 20, memoryAllocator->allocate( 4 << 20 ) );
}

PipelineStateCache::~PipelineStateCache()
{
    for ( i32 i = 0; i < cachedPipelineStateCount; i++ ) {
        renderDevice->destroyPipelineState( pipelineStates[i] );
    }

    dk::core::free( memoryAllocator, shaderCache );

    cachedPipelineStateCount = 0;
    memset( pipelineHashes, 0, sizeof( Hash128 ) * MAX_CACHE_ELEMENT_COUNT );
    memset( pipelineStates, 0, sizeof( PipelineState* ) * MAX_CACHE_ELEMENT_COUNT );
}

PipelineState* PipelineStateCache::getOrCreatePipelineState( const PipelineStateDesc& descriptor, const ShaderBinding& shaderBinding, const bool forceRebuild )
{
    i32 cachedPsoIndex = -1;

    // (crappy) linear hashcode lookup (TODO profile this to make sure this isn't bottleneck)
    Hash128 psoDescHashcode = computePipelineStateKey( descriptor, shaderBinding );
    for ( i32 i = 0; i < cachedPipelineStateCount; i++ ) {
        if ( pipelineHashes[i] == psoDescHashcode ) {
            if ( forceRebuild ) {
                cachedPsoIndex = i;
                break;
            }

            return pipelineStates[i];
        }
    }

    // TODO In the future this should be done asynchronously to avoid this blocking behavior
    PipelineStateDesc filledDescriptor = descriptor;
    if ( filledDescriptor.PipelineType == PipelineStateDesc::COMPUTE ) {
        filledDescriptor.computeShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_COMPUTE>( shaderBinding.ComputeShader, forceRebuild );
    } else {
        if ( shaderBinding.VertexShader != nullptr && shaderBinding.VertexShader[0] != DUSK_STRING( '\0' ) ) {
            filledDescriptor.vertexShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_VERTEX>( shaderBinding.VertexShader, forceRebuild );
        }
       /* if ( shaderBinding.TesselationControlShader != 0 ) {
            filledDescriptor.tesselationControlShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_TESSELATION_CONTROL>( shaderBinding.TesselationControlShader, forceRebuild );
        }
        if ( shaderBinding.TesselationEvaluationShader != 0 ) {
            filledDescriptor.tesselationEvalShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION>( shaderBinding.TesselationEvaluationShader, forceRebuild );
        }*/
        if ( shaderBinding.PixelShader != nullptr && shaderBinding.PixelShader[0] != DUSK_STRING( '\0' ) ) {
            filledDescriptor.pixelShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_PIXEL>( shaderBinding.PixelShader, forceRebuild );
        }
    }

    // Retrieve cache from disk (if available)
    dkString_t psoDescHashcodeDigest = dk::core::GetHashcodeDigest128( psoDescHashcode );

    dkString_t cachedPsoName = dkString_t( DUSK_STRING( "GameData/cache/" ) ) + psoDescHashcodeDigest + DUSK_STRING( ".bin" );

    bool isPsoCached = DisablePipelineCache;
    if ( !DisablePipelineCache ) {
        FileSystemObject* cachedPSO = virtualFs->openFile( cachedPsoName, eFileOpenMode::FILE_OPEN_MODE_READ | eFileOpenMode::FILE_OPEN_MODE_BINARY );
        if ( cachedPSO != nullptr ) {
            u32 psoBinarySize = 0;
            cachedPSO->read( psoBinarySize );

            u8* psoBinary = dk::core::allocateArray<u8>( pipelineStateCacheAllocator, psoBinarySize );
            cachedPSO->read( psoBinary, psoBinarySize );

            filledDescriptor.cachedPsoData = psoBinary;
            filledDescriptor.cachedPsoSize = psoBinarySize;
        } else {
            isPsoCached = false;
        }
    }

    PipelineState* pipelineState = renderDevice->createPipelineState( filledDescriptor );
    DUSK_DEV_ASSERT( pipelineState, "Failed to create pipelinestate (TODO return a generic/error pso)" )

    if ( pipelineState != nullptr && !isPsoCached ) {
        DUSK_LOG_INFO( "Missing PSO cache '%s'! Building cache...\n", psoDescHashcodeDigest.c_str() );

        void* psoBinary = nullptr;
        size_t psoBinarySize = 0;
        renderDevice->getPipelineStateCache( pipelineState, &psoBinary, psoBinarySize );

        FileSystemObject* cachedPSO = virtualFs->openFile( cachedPsoName, eFileOpenMode::FILE_OPEN_MODE_WRITE | eFileOpenMode::FILE_OPEN_MODE_BINARY );
        cachedPSO->write( static_cast< u32 >( psoBinarySize ) );
        cachedPSO->write( static_cast< u8* >( psoBinary ), psoBinarySize );
        cachedPSO->close();

        renderDevice->destroyPipelineStateCache( pipelineState );
    }

    // Free cached data read from disk (if any)
    if ( filledDescriptor.cachedPsoData != nullptr ) {
        dk::core::freeArray( pipelineStateCacheAllocator, static_cast< u8* >( filledDescriptor.cachedPsoData ) );

        // Since allocators are thread local, we can safely reset its pointer at the end of the creation of the pso
        pipelineStateCacheAllocator->clear();
    }

    // TODO Critical section / synchronized section
    i32& psoCacheIndex = ( cachedPsoIndex == -1 ) ? cachedPipelineStateCount : cachedPsoIndex;

    pipelineHashes[psoCacheIndex] = psoDescHashcode;
    pipelineStates[psoCacheIndex] = pipelineState;

    PipelineState* cachedPipelineState = pipelineStates[psoCacheIndex];
    psoCacheIndex++;

    return cachedPipelineState;
}

Hash128 PipelineStateCache::computePipelineStateKey( const PipelineStateDesc& descriptor, const ShaderBinding& shaderBinding ) const
{
    struct PipelineStateKey {
        PipelineStateDesc::Type            PipelineType : 2;
        u8 : 0;
        dkStringHash_t                     vsStage;

        dkStringHash_t                     gsStage;
        dkStringHash_t                     teStage;

        dkStringHash_t                     tcStage;
        dkStringHash_t                     psOrCsStage;

        u64                                RStateSortKey[2];
        u64                                DSSStateSortKey[2];

        u64                                BSSortKey;
    } Key;

    memset( &Key, 0, sizeof( PipelineStateKey ) );

    // Fill Pipeline State key descriptor
    Key.PipelineType = descriptor.PipelineType;

    Key.vsStage = shaderBinding.VertexShader != nullptr ? dk::core::CRC32( shaderBinding.VertexShader ) : 0;
    Key.gsStage = 0;
    Key.teStage = shaderBinding.TesselationEvaluationShader != nullptr ? dk::core::CRC32( shaderBinding.TesselationEvaluationShader ) : 0;
    Key.tcStage = shaderBinding.TesselationControlShader != nullptr ? dk::core::CRC32( shaderBinding.TesselationControlShader ) : 0;

	if ( descriptor.PipelineType == PipelineStateDesc::GRAPHICS ) {
		Key.psOrCsStage = shaderBinding.PixelShader != nullptr ? dk::core::CRC32( shaderBinding.PixelShader ) : 0;
	} else if ( descriptor.PipelineType == PipelineStateDesc::COMPUTE ) {
		Key.psOrCsStage = shaderBinding.ComputeShader != nullptr ? dk::core::CRC32( shaderBinding.ComputeShader ) : 0;
	}
	
    memcpy( Key.RStateSortKey, descriptor.RasterizerState.SortKey, sizeof( RasterizerStateDesc::SortKey ) );
    memcpy( Key.DSSStateSortKey, descriptor.DepthStencilState.SortKey, sizeof( DepthStencilStateDesc::SortKey ) );

    Key.BSSortKey = descriptor.BlendState.SortKey;

    Hash128 psoKeyHashcode;
    MurmurHash3_x64_128( &Key, sizeof( PipelineStateKey ), 234823489, &psoKeyHashcode );
    return psoKeyHashcode;
}
