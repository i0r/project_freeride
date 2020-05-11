/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "BrunetonSkyModel.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Framework/Cameras/Camera.h>

#include <Maths/Helpers.h>
#include <Maths/MatrixTransformations.h>

#include "AtmosphereSettings.h"
#include "AtmosphereConstants.h"

BrunetonSkyRenderModule::BrunetonSkyRenderModule()
    : transmittanceTexture( nullptr )
    , scatteringTexture( nullptr )
    , irradianceTexture( nullptr )
    , sunVerticalAngle( 1.000f )
    , sunHorizontalAngle( 0.5f )
    , sunAngularRadius( static_cast<f32>( kSunAngularRadius ) )
{

}

BrunetonSkyRenderModule::~BrunetonSkyRenderModule()
{
    transmittanceTexture = nullptr;
    scatteringTexture = nullptr;
    irradianceTexture = nullptr;
}

ResHandle_t BrunetonSkyRenderModule::renderSky( FrameGraph& frameGraph, ResHandle_t renderTarget, ResHandle_t depthBuffer, const bool renderSunDisk, const bool useAutomaticExposure )
{
    struct PassData {
        ResHandle_t         output;
        ResHandle_t         depthBuffer;

        ResHandle_t         parametersBuffer;
        ResHandle_t         cameraBuffer;
        ResHandle_t         autoExposureBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        "Sky Render Pass",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.setUncullablePass();

            BufferDesc skyBufferDesc;
            skyBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            skyBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            skyBufferDesc.SizeInBytes = sizeof( parameters );

            passData.parametersBuffer = builder.allocateBuffer( skyBufferDesc, SHADER_STAGE_VERTEX | SHADER_STAGE_PIXEL );

            BufferDesc cameraBufferDesc;
            cameraBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            cameraBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            cameraBufferDesc.SizeInBytes = sizeof( CameraData );

            passData.cameraBuffer = builder.allocateBuffer( cameraBufferDesc, SHADER_STAGE_VERTEX );

            // Render Targets
            if ( renderTarget == RES_HANDLE_INVALID ) {
                ImageDesc rtDesc;
                rtDesc.dimension = ImageDesc::DIMENSION_2D;
                rtDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
                rtDesc.usage = RESOURCE_USAGE_DEFAULT;
                rtDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

                passData.output = builder.allocateImage( rtDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );
            } else {
                passData.output = builder.readImage( renderTarget );
            }

            if ( depthBuffer == RES_HANDLE_INVALID ) {
                ImageDesc zBufferRenderTargetDesc;
                zBufferRenderTargetDesc.dimension = ImageDesc::DIMENSION_2D;
                zBufferRenderTargetDesc.format = eViewFormat::VIEW_FORMAT_D32_FLOAT;
                zBufferRenderTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
                zBufferRenderTargetDesc.bindFlags = RESOURCE_BIND_DEPTH_STENCIL;

                passData.depthBuffer = builder.allocateImage( zBufferRenderTargetDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );
            } else {
                passData.depthBuffer = builder.readImage( depthBuffer );
            }
           
            if ( useAutomaticExposure )
                passData.autoExposureBuffer = builder.retrievePersistentBuffer( DUSK_STRING_HASH( "AutoExposure/ReadBuffer" ) );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            // Update viewport (using image quality scaling)
            const CameraData* camera = resources->getMainCamera();

            dkVec2f scaledViewportSize = camera->viewportSize * camera->imageQuality;

            Viewport vp;
            vp.X = 0;
            vp.Y = 0;
            vp.Width = static_cast<i32>( scaledViewportSize.x );
            vp.Height = static_cast<i32>( scaledViewportSize.y );
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            ScissorRegion sr;
            sr.Top = 0;
            sr.Left = 0;
            sr.Right = static_cast< i32 >( scaledViewportSize.x );
            sr.Bottom = static_cast< i32 >( scaledViewportSize.y );

            // Update Parameters
            parameters.SunSizeX = tanf( sunAngularRadius );
            parameters.SunSizeY = cosf( sunAngularRadius * 5.0f );
            parameters.SunDirection = dk::maths::SphericalToCarthesianCoordinates( sunHorizontalAngle, sunVerticalAngle );

            // Retrieve allocated resources
            Buffer* skyBuffer = resources->getBuffer( passData.parametersBuffer );
            Buffer* cameraBuffer = resources->getBuffer( passData.cameraBuffer );
            Buffer* autoExposureBuffer = resources->getPersistentBuffer( passData.autoExposureBuffer );
            Image* outputTarget = resources->getImage( passData.output );
            Image* depthBufferTarget = resources->getImage( passData.depthBuffer );

            // Pipeline State
            PipelineState* pipelineStateObject = getPipelineStatePermutation( camera->msaaSamplerCount, useAutomaticExposure, useAutomaticExposure );

            cmdList->pushEventMarker( DUSK_STRING( "Bruneton Sky" ) );
            cmdList->bindPipelineState( pipelineStateObject );

            cmdList->setViewport( vp );
            cmdList->setScissor( sr );

            cmdList->transitionImage( *scatteringTexture, eResourceState::RESOURCE_STATE_PIXEL_BINDED_RESOURCE );
            cmdList->transitionImage( *transmittanceTexture, eResourceState::RESOURCE_STATE_PIXEL_BINDED_RESOURCE );
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_RENDER_TARGET );

            // Bind resources
            cmdList->bindConstantBuffer( DUSK_STRING_HASH( "g_ActiveCameraBuffer" ), cameraBuffer );
            cmdList->bindConstantBuffer( DUSK_STRING_HASH( "g_AtmosphereBuffer" ), skyBuffer );
            cmdList->bindImage( DUSK_STRING_HASH( "g__ScatteringTexture" ), scatteringTexture );
            cmdList->bindImage( DUSK_STRING_HASH( "g__TransmittanceTexture" ), transmittanceTexture );
            cmdList->bindImage( DUSK_STRING_HASH( "g__SingleMie_ScatteringTexture" ), scatteringTexture );

            if ( useAutomaticExposure ) {
                cmdList->bindBuffer( DUSK_STRING_HASH( "g_AutoExposureBuffer" ), autoExposureBuffer );
            }

            cmdList->updateBuffer( *skyBuffer, &parameters, sizeof( parameters ) );
            cmdList->updateBuffer( *cameraBuffer, camera, sizeof( CameraData ) );

            cmdList->prepareAndBindResourceList( pipelineStateObject );
            cmdList->setupFramebuffer( &outputTarget, depthBufferTarget );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_RENDER_TARGET, 0, CommandList::TRANSITION_GRAPHICS_TO_COMPUTE );
        }
    );

    return passData.output;
}

void BrunetonSkyRenderModule::destroy( RenderDevice& renderDevice )
{
    for ( u32 i = 0; i < 5u; i++ ) {
        renderDevice.destroyPipelineState( skyRenderPso[i] );
        renderDevice.destroyPipelineState( skyRenderNoSunFixedExposurePso[i] );
    }
}

void BrunetonSkyRenderModule::loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache )
{
    PipelineStateDesc psoDesc = {};
    psoDesc.vertexShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_VERTEX>( "Atmosphere/BrunetonSky" );
    psoDesc.pixelShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_PIXEL>( "Atmosphere/BrunetonSky+DUSK_FIXED_EXPOSURE" );
    psoDesc.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    psoDesc.RasterizerState.CullMode = eCullMode::CULL_MODE_BACK;
    psoDesc.DepthStencilState.EnableDepthTest = true;
    psoDesc.DepthStencilState.EnableDepthWrite = false;
    psoDesc.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_GEQUAL;
    //psoDesc.BlendState = RenderingHelpers::BS_AdditiveNoAlpha;

    psoDesc.FramebufferLayout.declareRTV( 0, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT/*, FramebufferLayoutDesc::CLEAR*/ );
    psoDesc.FramebufferLayout.declareDSV( eViewFormat::VIEW_FORMAT_D32_FLOAT );

    psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

    psoDesc.samplerCount = 1u;

    // RenderPass
    skyRenderNoSunFixedExposurePso[0] = renderDevice.createPipelineState( psoDesc );

    // Allocate and cache pipeline state
    u32 samplerCount = 2u;
    for ( u32 i = 1u; i < 5u; i++ ) {
        psoDesc.samplerCount = samplerCount;

        skyRenderNoSunFixedExposurePso[i] = renderDevice.createPipelineState( psoDesc );

        samplerCount *= 2u;
    }

    psoDesc.samplerCount = 1u;
    psoDesc.pixelShader = shaderCache.getOrUploadStageDynamic<SHADER_STAGE_PIXEL>( "Atmosphere/BrunetonSky+DUSK_RENDER_SUN_DISC" );

    skyRenderPso[0] = renderDevice.createPipelineState( psoDesc );

    // Allocate and cache pipeline state
    samplerCount = 2u;
    for ( u32 i = 1u; i < 5u; i++ ) {
        psoDesc.samplerCount = samplerCount;

        skyRenderPso[i] = renderDevice.createPipelineState( psoDesc );

        samplerCount *= 2u;
    }

    // Load precomputed table and shaders
    transmittanceTexture = graphicsAssetCache.getImage( ATMOSPHERE_TRANSMITTANCE_TEXTURE_NAME );
    scatteringTexture = graphicsAssetCache.getImage( ATMOSPHERE_SCATTERING_TEXTURE_NAME );
    irradianceTexture = graphicsAssetCache.getImage( ATMOSPHERE_IRRADIANCE_TEXTURE_NAME );

    // Set Default Parameters
    parameters.EarthCenter = dkVec3f( 0.0, 0.0, -kBottomRadius / kLengthUnitInMeters );
}

void BrunetonSkyRenderModule::setSunSphericalPosition( const f32 verticalAngleRads, const f32 horizontalAngleRads )
{
    sunVerticalAngle = verticalAngleRads;
    sunHorizontalAngle = horizontalAngleRads;
}

dkVec2f BrunetonSkyRenderModule::getSunSphericalPosition()
{
    return dkVec2f( sunVerticalAngle, sunHorizontalAngle );
}

PipelineState* BrunetonSkyRenderModule::getPipelineStatePermutation( const u32 samplerCount, const bool renderSunDisk, const bool useAutomaticExposure )
{
    switch ( samplerCount ) {
    case 1:
        return ( renderSunDisk ) ? skyRenderPso[0] : skyRenderNoSunFixedExposurePso[0];
    case 2:
        return ( renderSunDisk ) ? skyRenderPso[1] : skyRenderNoSunFixedExposurePso[1];
    case 4:
        return ( renderSunDisk ) ? skyRenderPso[2] : skyRenderNoSunFixedExposurePso[2];
    case 8:
        return ( renderSunDisk ) ? skyRenderPso[3] : skyRenderNoSunFixedExposurePso[3];
    case 16:
        return ( renderSunDisk ) ? skyRenderPso[4] : skyRenderNoSunFixedExposurePso[4];
    default:
        return ( renderSunDisk ) ? skyRenderPso[0] : skyRenderNoSunFixedExposurePso[0];
    }
}
