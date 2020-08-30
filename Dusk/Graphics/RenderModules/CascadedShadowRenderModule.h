/*
    Dusk Source Code
    Copyright (C) 2020209 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class GraphicsAssetCache;
class BaseAllocator;
class RenderWorld;
class LinearAllocator;

struct GPUShadowDrawCmd;
struct DrawCall;
struct GPUShadowBatchInfos;
struct Image;
struct Buffer;
struct DirectionalLightGPU;
struct CameraData;
struct MeshConstants;
struct MeshCluster;
struct SmallBatchData;
struct SmallBatchDrawConstants;

#include "Graphics/FrameGraph.h"
#include <Maths/Matrix.h>
#include <vector>

class CascadedShadowRenderModule
{
public:
    // TODO THIS SHOULDNT BE STORED HERE (those constants have nothing to do with CSM)
	static constexpr i32 BATCH_CHUNK_COUNT = 32; // Number of chunk for batch dispatch. Each chunk can be filled with multiple clusters.
    static constexpr i32 BATCH_SIZE = 4 * 64; // Should be a multiple of the wavefront size
    static constexpr i32 BATCH_COUNT = 1 * 384;

    static constexpr dkStringHash_t SliceBufferHashcode = DUSK_STRING_HASH( "SliceInfos" );
    static constexpr dkStringHash_t SliceImageHashcode = DUSK_STRING_HASH( "SunShadowMap" );

public:
    DUSK_INLINE const dkMat4x4f& getGlobalShadowMatrix() const { return globalShadowMatrix; }
    DUSK_INLINE Image* getShadowAtlas() const { return shadowSlices; }

public:
                        CascadedShadowRenderModule( BaseAllocator* allocator );
                        CascadedShadowRenderModule( CascadedShadowRenderModule& ) = delete;
                        CascadedShadowRenderModule& operator = ( CascadedShadowRenderModule& ) = delete;
                        ~CascadedShadowRenderModule();

    void                destroy( RenderDevice& renderDevice );

	void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    void                captureShadowMap( FrameGraph& frameGraph, FGHandle depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight, const RenderWorld* renderWorld );

    void                submitGPUShadowCullCmds( GPUShadowDrawCmd* drawCmds, const size_t drawCmdCount );

	void                lockForUpload();

	void                lockForRendering();

	void                unlock();

private:
    void                fillBatchChunks( const CameraData* cameraData, MeshConstants* gpuShadowCasters, const std::vector<MeshCluster>* meshClusters );

    // Clear indirect draw argument buffers (via a single compute call).
    void                clearIndirectArgsBuffer( FrameGraph& frameGraph );

    // Build matrices and parameters to capture CSM slices on GPU.
    void                setupParameters( FrameGraph& frameGraph, FGHandle depthMinMax, const DirectionalLightGPU& directionalLight );

    // Reduce and extract the min/max of a given depth buffer (assuming the input buffer is reversed).
    FGHandle            reduceDepthBuffer( FrameGraph& frameGraph, FGHandle depthBuffer, const dkVec2f& depthBufferSize );

	bool                acquireLock( const i32 nextState );

private:
    struct BatchChunk {
        // Output indice buffer filled with visible triangles.
        Buffer* FilteredIndiceBuffer;

        // Buffer holding batch data.
        Buffer* BatchDataBuffer;

        // Buffer holding draw arguments to render this chunk.
        Buffer* DrawArgsBuffer;

        Buffer* DrawCallArgsBuffer;

        Buffer* InstanceIdBuffer;

        // The number of draw call enqueued in this chunk (flushed during the frame rendering).
        u32     EnqueuedDrawCallCount;

        // Batch count included in this chunk.
        u32     BatchCount;

        u32     FaceCount;

        SmallBatchData* BatchData;

        SmallBatchDrawConstants* DrawCallArgs;
    };

private:
	BaseAllocator*      memoryAllocator;

    Buffer*             csmSliceInfosBuffer;

    LinearAllocator*    drawCallsAllocator;

    Image*              shadowSlices;

    dkMat4x4f           globalShadowMatrix;

    BatchChunk*         batchChunks;

	std::atomic<i32>    renderingLock;
};
