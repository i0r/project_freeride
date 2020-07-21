/*
    Dusk Source Code
    Copyright (C) 2020209 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class GraphicsAssetCache;
class BaseAllocator;
struct DrawCall;
struct GPUShadowBatchInfos;
struct Image;
struct Buffer;

using ResHandle_t = uint32_t;

#include <Maths/Matrix.h>

class CascadedShadowRenderModule
{
public:
    static constexpr dkStringHash_t SliceBufferHashcode = DUSK_STRING_HASH( "SliceInfos" );
    static constexpr dkStringHash_t SliceImageHashcode = DUSK_STRING_HASH( "SunShadowMap" );

public:
    DUSK_INLINE const dkMat4x4f& getGlobalShadowMatrix() const { return globalShadowMatrix; }

public:
                        CascadedShadowRenderModule( BaseAllocator* allocator );
                        CascadedShadowRenderModule( CascadedShadowRenderModule& ) = delete;
                        CascadedShadowRenderModule& operator = ( CascadedShadowRenderModule& ) = delete;
                        ~CascadedShadowRenderModule();

    void                destroy( RenderDevice& renderDevice );

	void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    void                captureShadowMap( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight, const RenderWorld* renderWorld );

    void                submitGPUShadowCullCmds( GPUShadowDrawCmd* drawCmds, const size_t drawCmdCount );

private:
    struct CullPassOutput
	{
		ResHandle_t DrawCallsBuffer;
		ResHandle_t CulledIndexesBuffer;
    };

private:
    // Clear indirect draw argument buffers (via a single compute call).
    void                clearIndirectArgsBuffer( FrameGraph& frameGraph );

    // Build matrices and parameters to capture CSM slices on GPU.
    void                setupParameters( FrameGraph& frameGraph, ResHandle_t depthMinMax, const DirectionalLightGPU& directionalLight );

    // Reduce and extract the min/max of a given depth buffer (assuming the input buffer is reversed).
    ResHandle_t         reduceDepthBuffer( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize );

    CullPassOutput      cullShadowCasters( FrameGraph& frameGraph );

    ResHandle_t         batchDrawCalls( FrameGraph& frameGraph, CullPassOutput& cullPassOutput, const u32 gpuShadowCastersCount, GPUShadowBatchInfos* gpuShadowCasters );

private:
    struct BatchChunk {
        // Output indice buffer filled with visible triangles.
        Buffer* FilteredIndiceBuffer;

        // Buffer holding batch data.
        Buffer* BatchDataBuffer;

        // Buffer holding draw arguments to render this chunk.
        Buffer* DrawArgsBuffer;
    };

private:
	BaseAllocator*      memoryAllocator;

    Buffer*             csmSliceInfosBuffer;

	Buffer*             drawArgsBuffer;

    LinearAllocator*    drawCallsAllocator;

    Image*              shadowSlices;

    dkMat4x4f           globalShadowMatrix;

    BatchChunk*         batchChunks;
};
