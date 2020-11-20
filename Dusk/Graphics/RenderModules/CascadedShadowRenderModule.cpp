/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "CascadedShadowRenderModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Graphics/Mesh.h>
#include <Graphics/Model.h>
#include <Graphics/Material.h>

#include <Core/ViewFormat.h>

#include "Core/Allocators/LinearAllocator.h"

#include "Graphics/WorldRenderer.h"
#include "Graphics/RenderWorld.h"
#include "Graphics/ShaderHeaders/Light.h"

#include "Generated/DepthPyramid.generated.h"
#include "Generated/ShadowSetup.generated.h"
#include "Generated/Culling.generated.h"
#include "Generated/ShadowRendering.generated.h"

#include "WorldRenderModule.h"

#include "Maths/Frustum.h"
#include "Maths/MatrixTransformations.h"

#include <numeric>

DUSK_ENV_VAR( ShadowMapResolution, 2048, u32 ); // Per-slice shadow map resolution.

enum eCsmLockType : i32
{
	CSM_LOCK_READY = 0,
    CSM_LOCK_UPLOAD = 1,
    CSM_LOCK_RENDERING = 2
};

struct PerPassData
{
    u32         SliceIndex;
	u32         __PADDING__[3];
};

static dkMat4x4f CreateGlobalShadowMatrix( const dkVec3f& lightDirNormalized, const dkMat4x4f& viewProjection )
{
    // Get the 8 points of the view frustum in world space
    dkVec3f frustumCorners[8] = {
        dkVec3f( -1.0f, +1.0f, +0.0f ),
        dkVec3f( +1.0f, +1.0f, +0.0f ),
        dkVec3f( +1.0f, -1.0f, +0.0f ),
        dkVec3f( -1.0f, -1.0f, +0.0f ),
        dkVec3f( -1.0f, +1.0f, +1.0f ),
        dkVec3f( +1.0f, +1.0f, +1.0f ),
        dkVec3f( +1.0f, -1.0f, +1.0f ),
        dkVec3f( -1.0f, -1.0f, +1.0f ),
    };

    dkMat4x4f invViewProjection = viewProjection.inverse();
    dkVec3f frustumCenter( 0.0f );
    for ( uint64_t i = 0; i < 8; ++i ) {
        frustumCorners[i] = dkVec4f( frustumCorners[i], 1.0f ) * invViewProjection;
        frustumCenter += frustumCorners[i];
    }

    frustumCenter /= 8.0f;

    // Pick the up vector to use for the light camera
    const dkVec3f upDir = dkVec3f( 0.0f, 1.0f, 0.0f );

    // Get position of the shadow camera
    dkVec3f shadowCameraPos = frustumCenter + lightDirNormalized * -0.5f;

    // Create a new orthographic camera for the shadow caster
    dkMat4x4f shadowCamera = dk::maths::MakeOrtho( -0.5f, +0.5f, -0.5f, +0.5f, +0.0f, +1.0f );
    dkMat4x4f shadowLookAt = dk::maths::MakeLookAtMat( shadowCameraPos, frustumCenter, upDir );
    dkMat4x4f shadowMatrix = shadowCamera * shadowLookAt;

    // Use a 4x4 bias matrix for texel sampling
    const dkMat4x4f texScaleBias = dkMat4x4f(
        +0.5f, +0.0f, +0.0f, +0.0f,
        +0.0f, -0.5f, +0.0f, +0.0f,
        +0.0f, +0.0f, +1.0f, +0.0f,
        +0.5f, +0.5f, +0.0f, +1.0f );

    return ( texScaleBias * shadowMatrix ).transpose();
}

CascadedShadowRenderModule::CascadedShadowRenderModule( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , csmSliceInfosBuffer( nullptr )
	, drawCallsAllocator( dk::core::allocate<LinearAllocator>( allocator, 4096 * sizeof( DrawCall ), allocator->allocate( 4096 * sizeof( DrawCall ) ) ) )
    , shadowSlices( nullptr )
    , globalShadowMatrix( dkMat4x4f::Identity )
	, batchChunks( dk::core::allocateArray<BatchChunk>( allocator, BATCH_CHUNK_COUNT ) )
	, renderingLock( 0 )
{

}

CascadedShadowRenderModule::~CascadedShadowRenderModule()
{
    dk::core::free( memoryAllocator, drawCallsAllocator );
    dk::core::freeArray( memoryAllocator, batchChunks );

    csmSliceInfosBuffer = nullptr;
}

void CascadedShadowRenderModule::destroy( RenderDevice& renderDevice )
{
    for ( i32 i = 0; i < BATCH_CHUNK_COUNT; ++i ) {
        BatchChunk& chunk = batchChunks[i];
        renderDevice.destroyBuffer( chunk.FilteredIndiceBuffer );
        renderDevice.destroyBuffer( chunk.BatchDataBuffer );
        renderDevice.destroyBuffer( chunk.DrawArgsBuffer );
        renderDevice.destroyBuffer( chunk.DrawCallArgsBuffer );
        renderDevice.destroyBuffer( chunk.InstanceIdBuffer );

        dk::core::freeArray( memoryAllocator, chunk.BatchData );
        dk::core::freeArray( memoryAllocator, chunk.DrawCallArgs );
    }

    if ( csmSliceInfosBuffer != nullptr ) {
        renderDevice.destroyBuffer( csmSliceInfosBuffer );
        csmSliceInfosBuffer = nullptr;
    }

    if ( shadowSlices != nullptr ) {
        renderDevice.destroyImage( shadowSlices );
        shadowSlices = nullptr;
    }
}

void CascadedShadowRenderModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    BufferDesc cascadeMatrixBuffer;
    cascadeMatrixBuffer.BindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
    cascadeMatrixBuffer.Usage = RESOURCE_USAGE_DEFAULT;
    cascadeMatrixBuffer.SizeInBytes = sizeof( CSMSliceInfos ) * 4;
    cascadeMatrixBuffer.StrideInBytes = sizeof( CSMSliceInfos );

    csmSliceInfosBuffer = renderDevice.createBuffer( cascadeMatrixBuffer );

    ImageDesc shadowSliceAtlas;
    shadowSliceAtlas.width = CSM_SLICE_DIMENSION * CSM_SLICE_COUNT;
    shadowSliceAtlas.height = CSM_SLICE_DIMENSION;
    shadowSliceAtlas.dimension = ImageDesc::DIMENSION_2D;
    shadowSliceAtlas.format = VIEW_FORMAT_R32_TYPELESS;
    shadowSliceAtlas.usage = RESOURCE_USAGE_DEFAULT;
    shadowSliceAtlas.bindFlags = RESOURCE_BIND_DEPTH_STENCIL | RESOURCE_BIND_SHADER_RESOURCE;
    shadowSliceAtlas.DefaultView.ViewFormat = VIEW_FORMAT_D32_FLOAT;

    shadowSlices = renderDevice.createImage( shadowSliceAtlas );

    std::vector<i32> ids( BATCH_COUNT );
    std::iota( ids.begin(), ids.end(), 0 );

    struct IndirectArguments {
        u32 IndexCountPerInstance;
        u32 InstanceCount;
        u32 StartIndexLocation;
        i32 BaseVertexLocation;
        u32 StartInstanceLocation;
    };

    std::vector<IndirectArguments> indirectArgs( BATCH_COUNT, IndirectArguments{ 0, 1, 0, 0, 0 } );

    for ( i32 i = 0; i < BATCH_CHUNK_COUNT; ++i ) {
        BufferDesc filteredIndiceBufferDesc;
        filteredIndiceBufferDesc.BindFlags = RESOURCE_BIND_INDICE_BUFFER | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
        filteredIndiceBufferDesc.SizeInBytes = BATCH_COUNT * BATCH_SIZE * ( sizeof( i32 ) * 3 );
        filteredIndiceBufferDesc.StrideInBytes = sizeof( i32 );
        filteredIndiceBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
        filteredIndiceBufferDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

        batchChunks[i].FilteredIndiceBuffer = renderDevice.createBuffer( filteredIndiceBufferDesc );

        BufferDesc batchDataBufferDesc;
        batchDataBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
        batchDataBufferDesc.SizeInBytes = BATCH_COUNT * sizeof( SmallBatchData );
        batchDataBufferDesc.StrideInBytes = sizeof( SmallBatchData );
        batchDataBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;

        batchChunks[i].BatchDataBuffer = renderDevice.createBuffer( batchDataBufferDesc );

        BufferDesc drawArgsBufferDesc;
        drawArgsBufferDesc.BindFlags = RESOURCE_BIND_INDIRECT_ARGUMENTS | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
        drawArgsBufferDesc.SizeInBytes = 5 * sizeof( u32 ) * BATCH_COUNT;
        drawArgsBufferDesc.StrideInBytes = 5 * sizeof( u32 );
        drawArgsBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
        drawArgsBufferDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

        batchChunks[i].DrawArgsBuffer = renderDevice.createBuffer( drawArgsBufferDesc, indirectArgs.data() );

        BufferDesc drawCallArgsBufferDesc;
        drawCallArgsBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
        drawCallArgsBufferDesc.SizeInBytes = BATCH_COUNT * sizeof( SmallBatchDrawConstants );
        drawCallArgsBufferDesc.StrideInBytes = sizeof( SmallBatchDrawConstants );
        drawCallArgsBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;

        batchChunks[i].DrawCallArgsBuffer = renderDevice.createBuffer( drawCallArgsBufferDesc );

        BufferDesc instanceIdBufferDesc;
        instanceIdBufferDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
        instanceIdBufferDesc.SizeInBytes = BATCH_COUNT * sizeof( i32 );
        instanceIdBufferDesc.StrideInBytes = sizeof( i32 );
        instanceIdBufferDesc.Usage = RESOURCE_USAGE_STATIC;

        batchChunks[i].InstanceIdBuffer = renderDevice.createBuffer( instanceIdBufferDesc, ids.data() );

        batchChunks[i].EnqueuedDrawCallCount = 0;
        batchChunks[i].BatchCount = 0;

        batchChunks[i].BatchData = dk::core::allocateArray<SmallBatchData>( memoryAllocator, BATCH_COUNT );
        batchChunks[i].DrawCallArgs = dk::core::allocateArray<SmallBatchDrawConstants>( memoryAllocator, BATCH_COUNT );
    }
}

void CascadedShadowRenderModule::captureShadowMap( FrameGraph& frameGraph, FGHandle depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight, const RenderWorld* renderWorld )
{   
    // Extract depth min/max.
    FGHandle depthMinMax = reduceDepthBuffer( frameGraph, depthBuffer, depthBufferSize );
    
    // Compute shadow matrices for each csm slice.
    setupParameters( frameGraph, depthMinMax, directionalLight );

    const CameraData* cameraData = frameGraph.getActiveCameraData();

    // Update this frame shadow casters (note that this function has an implicit
    // synchronization with rendering).
    fillBatchChunks( cameraData, renderWorld->getGpuShadowBatchesData(), renderWorld->getMeshClusters() );

    // Clear IndirectDraw arguments buffer.
    clearIndirectArgsBuffer( frameGraph );

    // Filter shadow geometry.
    struct DummyPassData {};
    frameGraph.addRenderPass<DummyPassData>(
        Culling::FilterShadowGeometry_Name,
        [&]( FrameGraphBuilder& builder, DummyPassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();
        },
        [=]( const DummyPassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            cmdList->pushEventMarker( Culling::FilterShadowGeometry_EventName );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, Culling::FilterShadowGeometry_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );

            for ( i32 i = 0; i < BATCH_CHUNK_COUNT; i++ ) {
                BatchChunk& batchChunk = batchChunks[i];

                // Since we linearly fill the chunks, we can early exit as soon as we reach an empty chunk.
                if ( batchChunk.BatchCount == 0 ) {
                    break;
                }

                // Clear indirect args buffer.
                cmdList->bindBuffer( Culling::FilterShadowGeometry_IndirectArgs_Hashcode, batchChunk.DrawArgsBuffer );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_FilteredIndiceBuffer_Hashcode, batchChunk.FilteredIndiceBuffer );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_VertexBuffer_Hashcode, renderWorld->getShadowVertexBuffer() );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_IndexBuffer_Hashcode, renderWorld->getShadowIndiceBuffer() );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_MeshConstantsBuffer_Hashcode, renderWorld->getMeshConstantsBuffer() );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_DrawConstants_Hashcode, batchChunk.DrawCallArgsBuffer );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_SmallBatchDataBuffer_Hashcode, batchChunk.BatchDataBuffer );
                cmdList->bindBuffer( Culling::FilterShadowGeometry_SliceInfos_Hashcode, csmSliceInfosBuffer );

                cmdList->prepareAndBindResourceList();

                cmdList->dispatchCompute( batchChunk.BatchCount, 1, 1 );
            }

            cmdList->popEventMarker();
        }
    );

	// Render each CSM slice.
    struct PassData
    {
		FGHandle VectorDataBuffer;
		FGHandle PerPassBuffer;
    };

    PassData& data = frameGraph.addRenderPass<PassData>(
        ShadowRendering::DirectionalShadowRendering_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
			BufferDesc perPassBuffer;
			perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;
			perPassBuffer.SizeInBytes = sizeof( PerPassData );

			passData.PerPassBuffer = builder.allocateBuffer( perPassBuffer, SHADER_STAGE_VERTEX );
        }, 
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            lockForRendering();

            Buffer* vectorBuffer = resources->getBuffer( passData.VectorDataBuffer );
            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

			cmdList->pushEventMarker( ShadowRendering::DirectionalShadowRendering_EventName );
			cmdList->clearDepthStencil( shadowSlices, 1.0f );

			PipelineStateDesc DefaultPipelineState( PipelineStateDesc::GRAPHICS );
			DefaultPipelineState.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			DefaultPipelineState.DepthStencilState.EnableDepthWrite = true;
			DefaultPipelineState.DepthStencilState.EnableDepthTest = true;
			DefaultPipelineState.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_LESS;

			DefaultPipelineState.RasterizerState.UseTriangleCCW = true;
			DefaultPipelineState.RasterizerState.CullMode = eCullMode::CULL_MODE_NONE;
			DefaultPipelineState.RasterizerState.FillMode = eFillMode::FILL_MODE_SOLID;

			DefaultPipelineState.FramebufferLayout.declareDSV( VIEW_FORMAT_D16_UNORM );

			DefaultPipelineState.samplerCount = 1u;
            DefaultPipelineState.InputLayout.Entry[0] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 0, 0, false, "POSITION" };
            DefaultPipelineState.InputLayout.Entry[1] = { 0, VIEW_FORMAT_R32_UINT, 1, 1, 0, false, "BLENDINDICES" };
			DefaultPipelineState.depthClearValue = 1.0f;

            PipelineState* pso = psoCache->getOrCreatePipelineState( DefaultPipelineState, ShadowRendering::DirectionalShadowRendering_ShaderBinding );

			cmdList->bindPipelineState( pso );

			cmdList->bindBuffer( InstanceVectorBufferHashcode, vectorBuffer );
            cmdList->bindBuffer( ShadowRendering::DirectionalShadowRendering_SliceInfos_Hashcode, csmSliceInfosBuffer );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
            
            cmdList->setupFramebuffer( nullptr, FramebufferAttachment( shadowSlices ) );

			const Buffer* bufferList[2] = {
				renderWorld->getShadowVertexBuffer(),
                nullptr
			};

			for ( i32 sliceIdx = 0; sliceIdx < CSM_SLICE_COUNT; sliceIdx++ ) {
				Viewport vp;
				vp.X = sliceIdx * CSM_SLICE_DIMENSION;
				vp.Y = 0;
				vp.Width = CSM_SLICE_DIMENSION;
				vp.Height = CSM_SLICE_DIMENSION;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;

				ScissorRegion sr;
				sr.Top = 0;
				sr.Left = sliceIdx * CSM_SLICE_DIMENSION;
				sr.Right = ( sliceIdx + 1 ) * CSM_SLICE_DIMENSION;
				sr.Bottom = CSM_SLICE_DIMENSION;

				cmdList->setViewport( vp );
				cmdList->setScissor( sr );

                PerPassData perPassData;
                perPassData.SliceIndex = sliceIdx;

                // Upload vector buffer offset
                cmdList->updateBuffer( *perPassBuffer, &perPassData, sizeof( PerPassData ) );

                for ( i32 i = 0; i < BATCH_CHUNK_COUNT; i++ ) {
                    BatchChunk& batchChunk = batchChunks[i];

                    // Since we linearly fill the chunks, we can early exit as soon as we reach an empty chunk.
                    if ( batchChunk.BatchCount == 0 ) {
                        break;
                    }

                    // Bind vertex buffers
                    bufferList[1] = batchChunk.InstanceIdBuffer;

                    cmdList->bindVertexBuffer( ( const Buffer** )bufferList, nullptr, 2, 0 );
                    cmdList->bindIndiceBuffer( batchChunk.FilteredIndiceBuffer, true );

                    cmdList->bindBuffer( ShadowRendering::DirectionalShadowRendering_DrawConstants_Hashcode, batchChunk.DrawCallArgsBuffer );
                    cmdList->prepareAndBindResourceList();

                    cmdList->multiDrawIndexedInstancedIndirect( batchChunk.EnqueuedDrawCallCount, batchChunk.DrawArgsBuffer, 0u, 5 * sizeof( u32 ) );
                }
			}

            cmdList->popEventMarker();

            unlock();
        } 
    );

    frameGraph.importPersistentBuffer( SliceBufferHashcode, csmSliceInfosBuffer );
    frameGraph.importPersistentImage( SliceImageHashcode, shadowSlices );
}

void CascadedShadowRenderModule::fillBatchChunks( const CameraData* cameraData, MeshConstants* gpuShadowCasters, const std::vector<MeshCluster>* meshClusters )
{
	lockForUpload();

    u32 drawCallsCount = static_cast< u32 >( drawCallsAllocator->getAllocationCount() );
    DrawCall* drawCalls = static_cast< DrawCall* >( drawCallsAllocator->getBaseAddress() );

    // Reset batch chunk infos from previous frame.
    for ( i32 i = 0; i < BATCH_CHUNK_COUNT; i++ ) {
        BatchChunk& chunk = batchChunks[i];

        chunk.BatchCount = 0u;
        chunk.EnqueuedDrawCallCount = 0u;
        chunk.FaceCount = 0u;
    }

    for ( u32 k = 0; k < drawCallsCount; k++ ) {
        const DrawCall& drawCall = drawCalls[k];
        const MeshConstants& meshInfos = gpuShadowCasters[drawCall.MeshEntryIndex];
        const std::vector<MeshCluster>& clusters = meshClusters[drawCall.MeshEntryIndex];

        i32 triangleCount = meshInfos.FaceCount;
        i32 firstTriangleIndex = 0;

        const dkVec3f eyePositionOS = dkVec4f( cameraData->worldPosition, 0.0f ) * drawCall.ModelMatrix.inverse();
       
        // Submit draw call triangles to the chunked batches.
        for ( i32 i = 0; i < BATCH_CHUNK_COUNT; i++ ) {
            BatchChunk& chunk = batchChunks[i];
            if ( chunk.EnqueuedDrawCallCount == BATCH_COUNT ) {
                continue;
            }

            i32 firstTriangle = firstTriangleIndex;
            const i32 firstCluster = firstTriangle / BATCH_SIZE;
            i32 currentCluster = firstCluster;
            i32 lastTriangle = firstTriangle;

            const i32 filteredIndexBufferStartOffset = chunk.BatchCount * BATCH_SIZE * 3 * sizeof( i32 );
            const u32 firstBatch = chunk.BatchCount;

            for ( i32 j = chunk.BatchCount; j < BATCH_COUNT; j++ ) {
                lastTriangle = Min( firstTriangle + BATCH_SIZE, static_cast<i32>( meshInfos.FaceCount ) );

                const MeshCluster& clusterInfo = clusters[currentCluster];
                ++currentCluster;

                bool cullCluster = false;

                const dkVec3f testVec = ( eyePositionOS - clusterInfo.ConeCenter ).normalize();
                const f32 testAngle = dkVec3f::dot( testVec, clusterInfo.ConeAxis );

                // Check if we're inside the cone
                if ( testAngle > clusterInfo.ConeAngleCosine ) {
                    cullCluster = true;
                }

                if ( !cullCluster ) {
                    SmallBatchData& smallBatchData = chunk.BatchData[chunk.BatchCount];
                    smallBatchData.DrawIndex = chunk.EnqueuedDrawCallCount;
                    smallBatchData.FaceCount = lastTriangle - firstTriangle;

                    // Offset relative to the start of the mesh
                    smallBatchData.IndexOffset = firstTriangle * 3 * sizeof( i32 );
                    smallBatchData.OutputIndexOffset = filteredIndexBufferStartOffset;
                    smallBatchData.MeshIndex = drawCall.MeshEntryIndex;
                    smallBatchData.DrawBatchStart = firstBatch;

                    chunk.FaceCount += smallBatchData.FaceCount;
                    ++chunk.BatchCount;
                }

                firstTriangle += BATCH_SIZE;

                if ( lastTriangle == meshInfos.FaceCount ) {
                    break;
                }
            }

            if ( chunk.BatchCount > firstBatch ) {
                chunk.DrawCallArgs[chunk.EnqueuedDrawCallCount].MeshEntryIndex = drawCall.MeshEntryIndex;
                chunk.DrawCallArgs[chunk.EnqueuedDrawCallCount].ModelMatrix = drawCall.ModelMatrix;
                ++chunk.EnqueuedDrawCallCount;
            }

            // Check if the draw command fit into this call, if not, create a remainder
            if ( lastTriangle < triangleCount ) {
                firstTriangleIndex = lastTriangle;
            } else {
                break;
            }
        }
    }

    unlock();
}

void CascadedShadowRenderModule::submitGPUShadowCullCmds( GPUShadowDrawCmd* drawCmds, const size_t drawCmdCount )
{
    drawCallsAllocator->clear();

    // Flatten batches informations. We want to assign one draw call per thread.
    for ( u32 i = 0; i < drawCmdCount; i++ ) {
        const GPUShadowDrawCmd& shadowDrawCmd = drawCmds[i];
        const u32 batchIdx = shadowDrawCmd.ShadowMeshBatchIndex;

        for ( u32 j = 0; j < shadowDrawCmd.InstanceCount; j++ ) {
            const GPUBatchData& batchData = shadowDrawCmd.InstancesData[j];

            DrawCall& singleDrawCall = *dk::core::allocate<DrawCall>( drawCallsAllocator );
            singleDrawCall.MeshEntryIndex = batchIdx;
            singleDrawCall.ModelMatrix = batchData.ModelMatrix;
            singleDrawCall.SphereCenter = batchData.BoundingSphereCenter;
            singleDrawCall.SphereRadius = batchData.BoundingSphereRadius;
        }
    }
}

void CascadedShadowRenderModule::clearIndirectArgsBuffer( FrameGraph& frameGraph )
{
    struct DummyPassData {};
    frameGraph.addRenderPass<DummyPassData>(
        Culling::ClearArgsBuffer_Name,
        [&]( FrameGraphBuilder& builder, DummyPassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();
        },
        [=]( const DummyPassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            cmdList->pushEventMarker( Culling::ClearArgsBuffer_EventName );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, Culling::ClearArgsBuffer_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );

            for ( i32 i = 0; i < BATCH_CHUNK_COUNT; i++ ) {
                BatchChunk& batchChunk = batchChunks[i];

                // Since we linearly fill the chunks, we can early exit as soon as we reach an empty chunk.
                if ( batchChunk.BatchCount == 0 ) {
                    break;
                }

                // Clear indirect args buffer.
                cmdList->bindBuffer( Culling::ClearArgsBuffer_IndirectArgs_Hashcode, batchChunk.DrawArgsBuffer );
                cmdList->prepareAndBindResourceList();

                cmdList->dispatchCompute( batchChunk.BatchCount, 1, 1 );

                // Upload batch data.
                // TODO We might want to do this early/in parallel using a copy command list.
                cmdList->updateBuffer( *batchChunk.BatchDataBuffer, batchChunk.BatchData, sizeof( SmallBatchData ) * batchChunk.BatchCount );
                cmdList->updateBuffer( *batchChunk.DrawCallArgsBuffer, batchChunk.DrawCallArgs, sizeof( SmallBatchDrawConstants ) * batchChunk.EnqueuedDrawCallCount );
            }

            cmdList->popEventMarker();
        }
    );
}

void CascadedShadowRenderModule::setupParameters( FrameGraph& frameGraph, FGHandle depthMinMax, const DirectionalLightGPU& directionalLight )
{
    struct PassData {
        FGHandle PerPassBuffer;
        FGHandle ReducedDepth;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        ShadowSetup::SetupCSMParameters_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            // PerPassBuffer
            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.SizeInBytes = sizeof( ShadowSetup::SetupCSMParametersRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );

            passData.ReducedDepth = builder.readReadOnlyImage( depthMinMax );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            cmdList->pushEventMarker( ShadowSetup::SetupCSMParameters_EventName );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, ShadowSetup::SetupCSMParameters_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );

            const CameraData* cameraData = resources->getMainCamera();

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Image* depthMinMax = resources->getImage( passData.ReducedDepth );

            dkVec3f lightDirection = directionalLight.NormalizedDirection;
            globalShadowMatrix = CreateGlobalShadowMatrix( lightDirection, cameraData->depthViewProjectionMatrix );

            // Bind resources
            ShadowSetup::SetupCSMParametersProperties.GlobalShadowMatrix = globalShadowMatrix;
            ShadowSetup::SetupCSMParametersProperties.ViewProjInv = cameraData->depthViewProjectionMatrix.inverse();
            ShadowSetup::SetupCSMParametersProperties.CameraNearClip = cameraData->depthNearPlane;
            ShadowSetup::SetupCSMParametersProperties.CameraFarClip = cameraData->depthFarPlane;
            ShadowSetup::SetupCSMParametersProperties.PSSMLambda = 0.50f;
            ShadowSetup::SetupCSMParametersProperties.LightDirection = lightDirection;
           
            cmdList->updateBuffer( *perPassBuffer, &ShadowSetup::SetupCSMParametersProperties, sizeof( ShadowSetup::SetupCSMParametersRuntimeProperties ) );

            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
            cmdList->bindImage( ShadowSetup::SetupCSMParameters_ReducedDepth_Hashcode, depthMinMax );
            cmdList->bindBuffer( ShadowSetup::SetupCSMParameters_SliceInfos_Hashcode, csmSliceInfosBuffer );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, 1, 1 );
            cmdList->popEventMarker();
        }
    );
}

FGHandle CascadedShadowRenderModule::reduceDepthBuffer( FrameGraph& frameGraph, FGHandle depthBuffer, const dkVec2f& depthBufferSize )
{
    struct PassData {
        FGHandle	PerPassBuffer;
        FGHandle	DepthBuffer;
        FGHandle	DepthChain[16];
        i32			DepthLevelCount;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        DepthPyramid::DepthReduction_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            // PerPassBuffer
            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.SizeInBytes = sizeof( DepthPyramid::DepthReductionRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );

            // DepthBuffer
            passData.DepthBuffer = builder.readReadOnlyImage( depthBuffer );

            // DepthChain
            ImageDesc levelDesc;
            levelDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            levelDesc.usage = RESOURCE_USAGE_DEFAULT;
            levelDesc.dimension = ImageDesc::DIMENSION_2D;
            levelDesc.format = eViewFormat::VIEW_FORMAT_R16G16_UNORM;
            levelDesc.width = static_cast< u32 >( depthBufferSize.x );
            levelDesc.height = static_cast< u32 >( depthBufferSize.y );

            passData.DepthLevelCount = 0;
            while ( levelDesc.width > 1 || levelDesc.height > 1 ) {
                levelDesc.width = DispatchSize( DepthPyramid::DepthReduction_DispatchX, levelDesc.width );
                levelDesc.height = DispatchSize( DepthPyramid::DepthReduction_DispatchY, levelDesc.height );

                passData.DepthChain[passData.DepthLevelCount++] = builder.allocateImage( levelDesc );
            }
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );

            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
            Image* depthBuffer = resources->getImage( passData.DepthBuffer );
            Image* reducedDepthTarget = resources->getImage( passData.DepthChain[0] );

            // The initial reduction invoke is different since we need to linearize the input depth buffer.
            cmdList->pushEventMarker( DepthPyramid::DepthReductionMip0_EventName );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, DepthPyramid::DepthReductionMip0_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );

            // Bind resources
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );
            cmdList->bindImage( DepthPyramid::DepthReduction_DepthBuffer_Hashcode, depthBuffer );
            cmdList->bindImage( DepthPyramid::DepthReduction_ReducedDepthMip_Hashcode, reducedDepthTarget );
            cmdList->prepareAndBindResourceList();

            const CameraData* cameraData = resources->getMainCamera();

            u32 width = static_cast< u32 >( depthBufferSize.x );
            u32 height = static_cast< u32 >( depthBufferSize.y );
            DepthPyramid::DepthReductionProperties.CameraPlanes = dkVec2f( cameraData->nearPlane, cameraData->farPlane );
            DepthPyramid::DepthReductionProperties.Projection = cameraData->projectionMatrix;
            DepthPyramid::DepthReductionProperties.TextureSize = dkVec2u( width, height );
            cmdList->updateBuffer( *passBuffer, &DepthPyramid::DepthReductionProperties, sizeof( DepthPyramid::DepthReductionRuntimeProperties ) );

            width = DispatchSize( DepthPyramid::DepthReduction_DispatchX, width );
            height = DispatchSize( DepthPyramid::DepthReduction_DispatchY, height );

            cmdList->dispatchCompute( width, height, DepthPyramid::DepthReduction_DispatchZ );
            cmdList->popEventMarker();

            // Do the regular reduction until we reach dimension 1,1
            cmdList->pushEventMarker( DepthPyramid::DepthReduction_EventName );

            pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, DepthPyramid::DepthReduction_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );
            for ( i32 i = 1; i < passData.DepthLevelCount; i++ ) {
                Image* previousLevelTarget = resources->getImage( passData.DepthChain[i - 1] );
                Image* currentLevelTarget = resources->getImage( passData.DepthChain[i] );

                cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );
                cmdList->bindImage( DepthPyramid::DepthReduction_CurrentDepthMip_Hashcode, previousLevelTarget );
                cmdList->bindImage( DepthPyramid::DepthReduction_ReducedDepthMip_Hashcode, currentLevelTarget );

                cmdList->prepareAndBindResourceList();

                DepthPyramid::DepthReductionProperties.TextureSize = dkVec2u( width, height );
                cmdList->updateBuffer( *passBuffer, &DepthPyramid::DepthReductionProperties, sizeof( DepthPyramid::DepthReductionRuntimeProperties ) );

                width = DispatchSize( DepthPyramid::DepthReduction_DispatchX, width );
                height = DispatchSize( DepthPyramid::DepthReduction_DispatchY, height );
                cmdList->dispatchCompute( width, height, DepthPyramid::DepthReduction_DispatchZ );
            }

            cmdList->popEventMarker();
        }
    );

    return passData.DepthChain[passData.DepthLevelCount - 1];
}

bool CascadedShadowRenderModule::acquireLock( const i32 nextState )
{
	i32 lockState = CSM_LOCK_READY;
	return renderingLock.compare_exchange_weak( lockState, nextState, std::memory_order_acquire );
}

void CascadedShadowRenderModule::unlock()
{
	renderingLock.store( CSM_LOCK_READY, std::memory_order_release );
}

void CascadedShadowRenderModule::lockForUpload()
{
	while ( !acquireLock( CSM_LOCK_UPLOAD ) );
}

void CascadedShadowRenderModule::lockForRendering()
{
	while ( !acquireLock( CSM_LOCK_RENDERING ) );
}