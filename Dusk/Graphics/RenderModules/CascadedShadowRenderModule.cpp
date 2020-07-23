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

#include "Graphics/RenderWorld.h"

#include "Generated/DepthPyramid.generated.h"
#include "Generated/ShadowSetup.generated.h"
#include "Generated/SceneCulling.generated.h"
#include "Generated/ShadowRendering.generated.h"

#include "WorldRenderModule.h"

#include "Graphics/RenderPasses/Headers/Light.h"

#include "Maths/Frustum.h"

#include <numeric>

DUSK_ENV_VAR( ShadowMapResolution, 2048, u32 ); // Per-slice shadow map resolution.

constexpr i32 BATCH_CHUNK_COUNT = 16; // Number of chunk for batch dispatch. Each chunk can be filled with multiple clusters.
constexpr i32 BATCH_SIZE = 4 * 64; // Should be a multiple of the wavefront size
constexpr i32 BATCH_COUNT = 1 * 384;

struct PerPassData
{
	f32         StartVector;
	f32         VectorPerInstance;
    u32         SliceIndex;
	u32         __PADDING__;
};

static u32 DispatchSize( const u32 tgSize, const u32 numElements )
{
	u32 dispatchSize = numElements / tgSize;
    dispatchSize += numElements % tgSize > 0 ? 1 : 0;
    return dispatchSize;
}

static dkVec3f TransformVec3( const dkVec3f& vector, const dkMat4x4f& matrix )
{
    float x = ( vector.x * matrix[0].x ) + ( vector.y * matrix[1].x ) + ( vector.z * matrix[2].x ) + matrix[3].x;
    float y = ( vector.x * matrix[0].y ) + ( vector.y * matrix[1].y ) + ( vector.z * matrix[2].y ) + matrix[3].y;
    float z = ( vector.x * matrix[0].z ) + ( vector.y * matrix[1].z ) + ( vector.z * matrix[2].z ) + matrix[3].z;
    float w = ( vector.x * matrix[0].w ) + ( vector.y * matrix[1].w ) + ( vector.z * matrix[2].w ) + matrix[3].w;

    return dkVec3f( x, y, z ) / w;
}

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

    dkMat4x4f invViewProjection = viewProjection.inverse(); // .transpose();
	/*dkMat4x4f( 3.21438E-8f, 0.00f, 0.53928f, 0.00f, 0.00f, 0.30335f, 0.00f, 0.00f, -159.83661f, -19.97958f, -19.97958f, -3.99592f,
			   158.99663f, 19.99958f, 19.99958f, 3.99992f );*/
    dkVec3f frustumCenter( 0.0f );
    for ( uint64_t i = 0; i < 8; ++i ) {
        frustumCorners[i] = TransformVec3( frustumCorners[i], invViewProjection );
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
    , drawArgsBuffer( nullptr)
	, drawCallsAllocator( dk::core::allocate<LinearAllocator>( allocator, 4096 * sizeof( DrawCall ), allocator->allocate( 4096 * sizeof( DrawCall ) ) ) )
    , shadowSlices( nullptr )
    , globalShadowMatrix( dkMat4x4f::Identity )
    , batchChunks( dk::core::allocateArray<BatchChunk>( allocator, BATCH_CHUNK_COUNT ) )
{

}

CascadedShadowRenderModule::~CascadedShadowRenderModule()
{
    dk::core::free( memoryAllocator, drawCallsAllocator );

    csmSliceInfosBuffer = nullptr;
}

void CascadedShadowRenderModule::destroy( RenderDevice& renderDevice )
{
    if ( csmSliceInfosBuffer != nullptr ) {
        renderDevice.destroyBuffer( csmSliceInfosBuffer );
        csmSliceInfosBuffer = nullptr;
    }

	if ( drawArgsBuffer != nullptr ) {
		renderDevice.destroyBuffer( drawArgsBuffer );
        drawArgsBuffer = nullptr;
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

	BufferDesc drawArgsBufferDesc;
	drawArgsBufferDesc.BindFlags = RESOURCE_BIND_RAW | RESOURCE_BIND_INDIRECT_ARGUMENTS | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
	drawArgsBufferDesc.SizeInBytes = 5 * sizeof( u32 ) * ( RenderWorld::MAX_MODEL_COUNT * 4 );
	drawArgsBufferDesc.StrideInBytes = 5 * sizeof( u32 );
	drawArgsBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
	drawArgsBufferDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32_TYPELESS;

	drawArgsBuffer = renderDevice.createBuffer( drawArgsBufferDesc );

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

        batchChunks[i].DrawArgsBuffer = renderDevice.createBuffer( drawArgsBufferDesc );

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
    }
}

void CascadedShadowRenderModule::captureShadowMap( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight, const RenderWorld* renderWorld )
{   
    // Extract depth min/max.
    ResHandle_t depthMinMax = reduceDepthBuffer( frameGraph, depthBuffer, depthBufferSize );
    
    // Compute shadow matrices for each csm slice.
    setupParameters( frameGraph, depthMinMax, directionalLight );

    // Clear IndirectDraw arguments buffer.
    clearIndirectArgsBuffer( frameGraph );
    
    // Do Frustum culling on each shadow caster.
    CullPassOutput cullPassOutput = cullShadowCasters( frameGraph );

    // Batch Culled casters and build draw arguments buffer.
    ResHandle_t batchesMatricesBuffer = batchDrawCalls( frameGraph, cullPassOutput, renderWorld->getGpuShadowBatchCount(), renderWorld->getGpuShadowBatchesData() );

	// Render each CSM slice.
    struct PassData
    {
		ResHandle_t VectorDataBuffer;
		ResHandle_t PerPassBuffer;
    };

    PassData& data = frameGraph.addRenderPass<PassData>(
        ShadowRendering::DirectionalShadowRendering_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.VectorDataBuffer = builder.readReadOnlyBuffer( batchesMatricesBuffer );

			BufferDesc perPassBuffer;
			perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;
			perPassBuffer.SizeInBytes = sizeof( PerPassData );

			passData.PerPassBuffer = builder.allocateBuffer( perPassBuffer, SHADER_STAGE_VERTEX );
        }, 
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
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
			DefaultPipelineState.RasterizerState.CullMode = eCullMode::CULL_MODE_FRONT;
			DefaultPipelineState.RasterizerState.FillMode = eFillMode::FILL_MODE_SOLID;

			DefaultPipelineState.FramebufferLayout.declareDSV( VIEW_FORMAT_D16_UNORM );

			DefaultPipelineState.samplerCount = 1u;
			DefaultPipelineState.InputLayout.Entry[0] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 0, 0, false, "POSITION" };
			DefaultPipelineState.depthClearValue = 1.0f;

            PipelineState* pso = psoCache->getOrCreatePipelineState( DefaultPipelineState, ShadowRendering::DirectionalShadowRendering_ShaderBinding );

			cmdList->bindPipelineState( pso );

			cmdList->bindBuffer( InstanceVectorBufferHashcode, vectorBuffer );
            cmdList->bindBuffer( ShadowRendering::DirectionalShadowRendering_SliceInfos_Hashcode, csmSliceInfosBuffer );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

			cmdList->setupFramebuffer( nullptr, FramebufferAttachment( shadowSlices ) );
			cmdList->prepareAndBindResourceList();

			const Buffer* bufferList[1] = {
				renderWorld->getShadowVertexBuffer()
			};

			// Bind vertex buffers
			cmdList->bindVertexBuffer( ( const Buffer** )bufferList, 1, 0 );
			cmdList->bindIndiceBuffer( renderWorld->getShadowIndiceBuffer(), true );

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
				perPassData.StartVector = static_cast<f32>( sliceIdx * 4096 );
				perPassData.VectorPerInstance = 4;
                perPassData.SliceIndex = sliceIdx;

				// Upload vector buffer offset
				cmdList->updateBuffer( *perPassBuffer, &perPassData, sizeof( PerPassData ) );

                cmdList->multiDrawIndexedInstancedIndirect( 1u, drawArgsBuffer, static_cast<u32>( perPassData.StartVector ), 5 * sizeof( u32 ) );
			}

            cmdList->popEventMarker();
        } 
    );

    frameGraph.importPersistentBuffer( SliceBufferHashcode, csmSliceInfosBuffer );
    frameGraph.importPersistentImage( SliceImageHashcode, shadowSlices );
}

ResHandle_t CascadedShadowRenderModule::batchDrawCalls( FrameGraph& frameGraph, CullPassOutput& cullPassOutput, const u32 gpuShadowCastersCount, MeshConstants* gpuShadowCasters )
{
	struct PassData
	{
		ResHandle_t DrawCallsBuffer;
		ResHandle_t CulledIndexesBuffer;
		ResHandle_t PerPassData;
		ResHandle_t BatchedMatricesBuffer;
		ResHandle_t ShadowCastersBuffer;
	};

	u32 drawCallsCount = static_cast< u32 >( drawCallsAllocator->getAllocationCount() );

	PassData& passData = frameGraph.addRenderPass<PassData>(
		SceneCulling::BatchCulledCommands_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
		    builder.useAsyncCompute();
		    builder.setUncullablePass();

		    passData.DrawCallsBuffer = builder.readBuffer( cullPassOutput.DrawCallsBuffer );
		    passData.CulledIndexesBuffer = builder.readBuffer( cullPassOutput.CulledIndexesBuffer );

		    BufferDesc perPassDesc;
		    perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
		    perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
		    perPassDesc.SizeInBytes = sizeof( SceneCulling::BatchCulledCommandsRuntimeProperties );

		    passData.PerPassData = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );

		    BufferDesc batchMatrixBufferDesc;
		    batchMatrixBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
		    batchMatrixBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
		    batchMatrixBufferDesc.SizeInBytes = sizeof( dkMat4x4f ) * 4096 * CSM_SLICE_COUNT;
		    batchMatrixBufferDesc.StrideInBytes = sizeof( dkMat4x4f );
		    batchMatrixBufferDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32G32B32A32_FLOAT;

		    passData.BatchedMatricesBuffer = builder.allocateBuffer( batchMatrixBufferDesc, SHADER_STAGE_COMPUTE );

		    BufferDesc shadowCasterBufferDesc;
		    shadowCasterBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
		    shadowCasterBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
		    shadowCasterBufferDesc.SizeInBytes = sizeof( GPUShadowBatchInfos ) * RenderWorld::MAX_MODEL_COUNT;
		    shadowCasterBufferDesc.StrideInBytes = sizeof( GPUShadowBatchInfos );

		    passData.ShadowCastersBuffer = builder.allocateBuffer( shadowCasterBufferDesc, SHADER_STAGE_COMPUTE );
	    },
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
		    cmdList->pushEventMarker( SceneCulling::BatchCulledCommands_EventName );

		    PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, SceneCulling::BatchCulledCommands_ShaderBinding );
		    cmdList->bindPipelineState( pipelineState );

		    Buffer* drawCallsBuffer = resources->getBuffer( passData.DrawCallsBuffer );
		    Buffer* culledIndexesBuffer = resources->getBuffer( passData.CulledIndexesBuffer );
		    Buffer* perPassBuffer = resources->getBuffer( passData.PerPassData );
		    Buffer* matrixBuffer = resources->getBuffer( passData.BatchedMatricesBuffer );
		    Buffer* shadowCastersBuffer = resources->getBuffer( passData.ShadowCastersBuffer );

		    SceneCulling::BatchCulledCommandsProperties.CullNearZ = false;
		    SceneCulling::BatchCulledCommandsProperties.NumDrawCalls = drawCallsCount;
		    SceneCulling::BatchCulledCommandsProperties.MaxMeshEntryIndex = gpuShadowCastersCount;

		    cmdList->updateBuffer( *perPassBuffer, &SceneCulling::BatchCulledCommandsProperties, sizeof( SceneCulling::BatchCulledCommandsRuntimeProperties ) );
		    cmdList->updateBuffer( *shadowCastersBuffer, gpuShadowCasters, sizeof( GPUShadowBatchInfos ) * gpuShadowCastersCount );

		    cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
		    cmdList->bindBuffer( SceneCulling::BatchCulledCommands_DrawCalls_Hashcode, drawCallsBuffer );
		    cmdList->bindBuffer( SceneCulling::BatchCulledCommands_CulledIndexes_Hashcode, culledIndexesBuffer );
		    cmdList->bindBuffer( SceneCulling::BatchCulledCommands_BatchedModelMatrices_Hashcode, matrixBuffer );
		    cmdList->bindBuffer( SceneCulling::BatchCulledCommands_ShadowCasters_Hashcode, shadowCastersBuffer );
		    cmdList->bindBuffer( SceneCulling::BatchCulledCommands_DrawArgsBuffer_Hashcode, drawArgsBuffer );

		    cmdList->prepareAndBindResourceList();

		    u32 threadGroupX = DispatchSize( SceneCulling::BatchCulledCommands_DispatchX, drawCallsCount );
		    cmdList->dispatchCompute( threadGroupX, 1, 1 );
		    cmdList->popEventMarker();
	    }
	);

    return passData.BatchedMatricesBuffer;
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
        SceneCulling::ClearArgsBuffer_Name,
        [&]( FrameGraphBuilder& builder, DummyPassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();
        },
        [=]( const DummyPassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            cmdList->pushEventMarker( SceneCulling::ClearArgsBuffer_EventName );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, SceneCulling::ClearArgsBuffer_ShaderBinding );
            cmdList->bindPipelineState( pipelineState );

            cmdList->bindBuffer( SceneCulling::ClearArgsBuffer_DrawArgsBuffer_Hashcode, drawArgsBuffer );

            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( 1, 1, 1 );
            cmdList->popEventMarker();
        }
    );
}

void CascadedShadowRenderModule::setupParameters( FrameGraph& frameGraph, ResHandle_t depthMinMax, const DirectionalLightGPU& directionalLight )
{
    struct PassData {
        ResHandle_t PerPassBuffer;
        ResHandle_t ReducedDepth;
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
                /*
                dkMat4x4f( 3.21438E-8f, 0.00f, 0.53928f, 0.00f, 0.00f, 0.30335f, 0.00f, 0.00f, -159.83661f, -19.97958f, -19.97958f, -3.99592f,
																			   158.99663f, 19.99958f, 19.99958f, 3.99992f ).transpose(); 
            */
            //cameraData->depthViewProjectionMatrix.inverse();
            ShadowSetup::SetupCSMParametersProperties.CameraNearClip = cameraData->depthNearPlane;
            ShadowSetup::SetupCSMParametersProperties.CameraFarClip = cameraData->depthFarPlane;
            ShadowSetup::SetupCSMParametersProperties.ShadowMapSize = static_cast< f32 >( ShadowMapResolution );
            ShadowSetup::SetupCSMParametersProperties.PSSMLambda = 1.0f;
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

ResHandle_t CascadedShadowRenderModule::reduceDepthBuffer( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize )
{
    struct PassData {
        ResHandle_t	PerPassBuffer;
        ResHandle_t	DepthBuffer;
        ResHandle_t	DepthChain[16];
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

CascadedShadowRenderModule::CullPassOutput CascadedShadowRenderModule::cullShadowCasters( FrameGraph& frameGraph )
{
	// Cull this frame shadow casters.   
	struct PassData
	{
		ResHandle_t DrawCallsBuffer;
		ResHandle_t CulledIndexesBuffer;
		ResHandle_t PerPassData;
	};

	PassData& passData = frameGraph.addRenderPass<PassData>(
		SceneCulling::FrustumCullDrawCalls_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
		    builder.useAsyncCompute();
		    builder.setUncullablePass();

		    BufferDesc drawCallsBufferDesc;
		    drawCallsBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
		    drawCallsBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
		    drawCallsBufferDesc.SizeInBytes = sizeof( DrawCall ) * 4096;
		    drawCallsBufferDesc.StrideInBytes = sizeof( DrawCall );

		    passData.DrawCallsBuffer = builder.allocateBuffer( drawCallsBufferDesc, SHADER_STAGE_COMPUTE );

		    BufferDesc visibleIndexesDesc;
		    visibleIndexesDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
		    visibleIndexesDesc.Usage = RESOURCE_USAGE_DEFAULT;
		    visibleIndexesDesc.SizeInBytes = sizeof( u16 ) * 4096 * CSM_SLICE_COUNT;
		    visibleIndexesDesc.StrideInBytes = sizeof( u16 );
		    visibleIndexesDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R16_UINT;

		    passData.CulledIndexesBuffer = builder.allocateBuffer( visibleIndexesDesc, SHADER_STAGE_COMPUTE );

		    BufferDesc perPassDesc;
		    perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
		    perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;
		    perPassDesc.SizeInBytes = sizeof( SceneCulling::FrustumCullDrawCallsRuntimeProperties );

		    passData.PerPassData = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
	    },
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
		    cmdList->pushEventMarker( SceneCulling::FrustumCullDrawCalls_EventName );

		    PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, SceneCulling::FrustumCullDrawCalls_ShaderBinding );
		    cmdList->bindPipelineState( pipelineState );

		    Buffer* drawCallsBuffer = resources->getBuffer( passData.DrawCallsBuffer );
		    Buffer* culledIndexesBuffer = resources->getBuffer( passData.CulledIndexesBuffer );
		    Buffer* perPassBuffer = resources->getBuffer( passData.PerPassData );

		    void* drawCallsAddress = drawCallsAllocator->getBaseAddress();
		    u32 drawCallsCount = static_cast< u32 >( drawCallsAllocator->getAllocationCount() );
		    cmdList->updateBuffer( *drawCallsBuffer, drawCallsAddress, sizeof( DrawCall ) * drawCallsCount );

		    SceneCulling::FrustumCullDrawCallsProperties.CullNearZ = false;
		    SceneCulling::FrustumCullDrawCallsProperties.NumDrawCalls = drawCallsCount;
		    cmdList->updateBuffer( *perPassBuffer, &SceneCulling::FrustumCullDrawCallsProperties, sizeof( SceneCulling::FrustumCullDrawCallsRuntimeProperties ) );

		    cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
		    cmdList->bindBuffer( SceneCulling::FrustumCullDrawCalls_DrawCalls_Hashcode, drawCallsBuffer );
		    cmdList->bindBuffer( SceneCulling::FrustumCullDrawCalls_VisibleIndexes_Hashcode, culledIndexesBuffer );
		    cmdList->bindBuffer( SceneCulling::FrustumCullDrawCalls_SliceInfos_Hashcode, csmSliceInfosBuffer );

		    cmdList->prepareAndBindResourceList();

		    u32 threadGroupX = DispatchSize( SceneCulling::FrustumCullDrawCalls_DispatchX, drawCallsCount );
		    cmdList->dispatchCompute( threadGroupX, 1, 1 );
		    cmdList->popEventMarker();
	    }
	);

    CullPassOutput output;
    output.CulledIndexesBuffer = passData.CulledIndexesBuffer;
    output.DrawCallsBuffer = passData.DrawCallsBuffer;

	return output;
}
