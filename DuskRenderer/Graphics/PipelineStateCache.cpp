/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "PipelineStateCache.h"

#include <Rendering/Shared.h>
#include "ShaderCache.h"

#include <Core/Hashing/MurmurHash3.h>
#include <Core/BitsManipulation.h>
#include <Core/Allocators/LinearAllocator.h>

#include <FileSystem/VirtualFileSystem.h>

static constexpr size_t PSO_BLOB_CACHE_SIZE = ( 4 << 20 ); // Size (in bytes) of the memory block allocated for cached PSO blob read

DUSK_DEV_VAR_PERSISTENT( DisablePipelineCache, true, bool ); // "Disables Pipeline State (+Root signature on D3D12) caching." [false/true]

PipelineStateCache::PipelineStateCache( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVFS, ShaderCache* activeShaderCache )
    : memoryAllocator( allocator )
    , renderDevice( activeRenderDevice )
    , virtualFs( activeVFS )
    , shaderCache( activeShaderCache )
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

    cachedPipelineStateCount = 0;
    memset( pipelineHashes, 0, sizeof( Hash128 ) * MAX_CACHE_ELEMENT_COUNT );
    memset( pipelineStates, 0, sizeof( PipelineState* ) * MAX_CACHE_ELEMENT_COUNT );
}

PipelineState* PipelineStateCache::getOrCreatePipelineState( const PipelineStateDesc& descriptor, const ShaderBinding& shaderBinding )
{
    // (crappy) linear hashcode lookup (TODO profile this to make sure this isn't bottleneck)
    Hash128 psoDescHashcode = computePipelineStateKey( descriptor, shaderBinding );
    for ( i32 i = 0; i < cachedPipelineStateCount; i++ ) {
        if ( pipelineHashes[i] == psoDescHashcode ) {
            return pipelineStates[i];
        }
    }

    // TODO In the future this should be done asynchronously to avoid this blocking behavior
    PipelineStateDesc filledDescriptor = descriptor;
    if ( filledDescriptor.PipelineType == PipelineStateDesc::COMPUTE ) {
        filledDescriptor.computeShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_COMPUTE>( shaderBinding.ComputeShader );
    } else {
        if ( shaderBinding.VertexShader != 0 ) {
            filledDescriptor.vertexShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_VERTEX>( shaderBinding.VertexShader );
        }
        if ( shaderBinding.TesselationControlShader != 0 ) {
            filledDescriptor.tesselationControlShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_TESSELATION_CONTROL>( shaderBinding.TesselationControlShader );
        }
        if ( shaderBinding.TesselationEvaluationShader != 0 ) {
            filledDescriptor.tesselationEvalShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION>( shaderBinding.TesselationEvaluationShader );
        }
        if ( shaderBinding.PixelShader != 0 ) {
            filledDescriptor.pixelShader = shaderCache->getOrUploadStage<eShaderStage::SHADER_STAGE_PIXEL>( shaderBinding.PixelShader );
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
    pipelineHashes[cachedPipelineStateCount] = psoDescHashcode;
    pipelineStates[cachedPipelineStateCount] = pipelineState;

    PipelineState* cachedPipelineState = pipelineStates[cachedPipelineStateCount];
    cachedPipelineStateCount++;

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
    Key.psOrCsStage = shaderBinding.PixelShader != nullptr ? dk::core::CRC32( shaderBinding.PixelShader ) : dk::core::CRC32( shaderBinding.ComputeShader );

    memcpy( Key.RStateSortKey, descriptor.RasterizerState.SortKey, sizeof( RasterizerStateDesc::SortKey ) );
    memcpy( Key.DSSStateSortKey, descriptor.DepthStencilState.SortKey, sizeof( DepthStencilStateDesc::SortKey ) );

    Key.BSSortKey = descriptor.BlendState.SortKey;

    Hash128 psoKeyHashcode;
    MurmurHash3_x64_128( &Key, sizeof( PipelineStateKey ), 234823489, &psoKeyHashcode );
    return psoKeyHashcode;
}
