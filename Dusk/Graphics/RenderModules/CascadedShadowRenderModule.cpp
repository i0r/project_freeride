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

#include "Generated/DepthPyramid.generated.h"
#include "Generated/ShadowSetup.generated.h"
#include "Generated/SceneCulling.generated.h"

DUSK_ENV_VAR( ShadowMapResolution, 2048, u32 ); // Per-slice shadow map resolution.

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

    dkMat4x4f invViewProjection = viewProjection.inverse().transpose();

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

    return ( texScaleBias * shadowMatrix );
}

CascadedShadowRenderModule::CascadedShadowRenderModule()
    : csmSliceInfosBuffer( nullptr )
{

}

CascadedShadowRenderModule::~CascadedShadowRenderModule()
{
    csmSliceInfosBuffer = nullptr;
}

void CascadedShadowRenderModule::destroy( RenderDevice& renderDevice )
{
    if ( csmSliceInfosBuffer != nullptr ) {
        renderDevice.destroyBuffer( csmSliceInfosBuffer );
        csmSliceInfosBuffer = nullptr;
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
    drawArgsBufferDesc.SizeInBytes = 5 * sizeof( u32 );
    drawArgsBufferDesc.StrideInBytes = sizeof( u32 );
    drawArgsBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
    drawArgsBufferDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32_TYPELESS;

    u32 drawArgsInit[5] = { 0, 1, 0, 0, 0 };
    drawArgsBuffer = renderDevice.createBuffer( drawArgsBufferDesc, drawArgsInit );
}

void CascadedShadowRenderModule::captureShadowMap( FrameGraph& frameGraph, ResHandle_t depthBuffer, const dkVec2f& depthBufferSize, const DirectionalLightGPU& directionalLight )
{   
    // Extract depth min/max.
    ResHandle_t depthMinMax = reduceDepthBuffer( frameGraph, depthBuffer, depthBufferSize );
    
    // Compute shadow matrices for each csm slice.
    setupParameters( frameGraph, depthMinMax, directionalLight );

    // Clear IndirectDraw arguments buffer.
    clearIndirectArgsBuffer( frameGraph );
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
            dkMat4x4f shadowMatrix = CreateGlobalShadowMatrix( lightDirection, cameraData->depthViewProjectionMatrix );

            // Bind resources
            ShadowSetup::SetupCSMParametersProperties.GlobalShadowMatrix = shadowMatrix;
            ShadowSetup::SetupCSMParametersProperties.ViewProjInv = cameraData->depthViewProjectionMatrix.inverse();
            ShadowSetup::SetupCSMParametersProperties.CameraNearClip = cameraData->nearPlane;
            ShadowSetup::SetupCSMParametersProperties.CameraFarClip = cameraData->farPlane;
            ShadowSetup::SetupCSMParametersProperties.ShadowMapSize = static_cast< f32 >( ShadowMapResolution );
            ShadowSetup::SetupCSMParametersProperties.PSSMLambda = 1.0000f;
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
            DepthPyramid::DepthReductionProperties.Projection = cameraData->finiteProjectionMatrix;
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
