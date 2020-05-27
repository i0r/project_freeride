/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct PipelineState;
class BaseAllocator;
class RenderDevice;
class VirtualFileSystem;
class ShaderCache;
class LinearAllocator;

#include <Core/Hashing/Helpers.h>
#include <Core/Types.h>
#include <Rendering/RenderDevice.h>

class PipelineStateCache
{
public:
    // Shader hashcodes table which should be provided when a PSO is requested
    // Note that we use explicitly const char array to avoid reallocation at runtime
    struct ShaderBinding {
        const dkChar_t*       VertexShader;
        const dkChar_t*       TesselationControlShader;
        const dkChar_t*       TesselationEvaluationShader;
        const dkChar_t*       PixelShader;
        const dkChar_t*       ComputeShader;

        ShaderBinding()
            : VertexShader( nullptr )
            , TesselationControlShader( nullptr )
            , TesselationEvaluationShader( nullptr )
            , PixelShader( nullptr )
            , ComputeShader( nullptr )
        {

        }
        
        explicit constexpr ShaderBinding( const dkChar_t* VS,
                                          const dkChar_t* TCS,
                                          const dkChar_t* TES,
                                          const dkChar_t* PS,
                                          const dkChar_t* CS )
            : VertexShader( VS )
            , TesselationControlShader( TCS )
            , TesselationEvaluationShader( TES )
            , PixelShader( PS )
            , ComputeShader( CS )
        {

        }
    };

public:
                                                        PipelineStateCache( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVFS, ShaderCache* activeShaderCache );
                                                        PipelineStateCache( PipelineStateCache& ) = delete;
                                                        PipelineStateCache& operator = ( PipelineStateCache& ) = delete;
                                                        ~PipelineStateCache();

    PipelineState*                                      getOrCreatePipelineState( const PipelineStateDesc& descriptor, const ShaderBinding& shaderBinding );

private:
    // The pipeline cache is local (one cache per thread); it shouldnt be too expensive 
    // to do a linear lookup when we need to retrieve a certain pipeline state
    static constexpr size_t MAX_CACHE_ELEMENT_COUNT = 32;

private:
    Hash128                 pipelineHashes[MAX_CACHE_ELEMENT_COUNT];
    PipelineState*          pipelineStates[MAX_CACHE_ELEMENT_COUNT];
    i32                     cachedPipelineStateCount;
    BaseAllocator*          memoryAllocator;
    RenderDevice*           renderDevice;
    VirtualFileSystem*      virtualFs;
    ShaderCache*            shaderCache;
    LinearAllocator*        pipelineStateCacheAllocator;

private:
    Hash128                                             computePipelineStateKey( const PipelineStateDesc& descriptor, const ShaderBinding& shaderBinding ) const;
};
