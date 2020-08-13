/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "AtmosphereRenderModule.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Framework/Cameras/Camera.h>

#include <Maths/Helpers.h>
#include <Maths/MatrixTransformations.h>

#include "Generated/AtmosphereBruneton.generated.h"

AtmosphereRenderModule::AtmosphereRenderModule()
    : lutComputeModule()
    , sunHorizontalAngle( 0.5f )
    , sunVerticalAngle( 0.5f )
    , transmittanceLut( nullptr )
    , irradianceLut( nullptr )
    , scatteringLut( nullptr )
    , mieScatteringLut( nullptr )
{

}

AtmosphereRenderModule::~AtmosphereRenderModule()
{

}

FGHandle AtmosphereRenderModule::renderAtmosphere( FrameGraph& frameGraph, FGHandle renderTarget, FGHandle depthBuffer )
{
    if ( !lutComputeJobTodo.empty() ) {
        RecomputeJob& job = lutComputeJobTodo.front();
        lutComputeJobTodo.pop();

        switch ( job.Step ) {
        case RecomputeStep::CompleteRecompute:
            lutComputeModule.precomputePipelineResources( frameGraph );

            // TODO We'll need to either copy those to a local image or double buffer images in the lut compute module.
            transmittanceLut = lutComputeModule.getTransmittanceLut();
            scatteringLut = lutComputeModule.getScatteringLut();
            irradianceLut = lutComputeModule.getIrradianceLut();
            mieScatteringLut = lutComputeModule.getMieScatteringLut();

            parameters = lutComputeModule.getAtmosphereParameters();
            break;
        default:
            break;
        }
	}

    FGHandle skyRendering = renderSky( frameGraph, renderTarget, depthBuffer );
	return skyRendering;
}

FGHandle AtmosphereRenderModule::renderSky( FrameGraph& frameGraph, FGHandle renderTarget, FGHandle depthBuffer )
{
    struct PassData {
        FGHandle         Output;
        FGHandle         DepthBuffer;

        FGHandle         PerPassBuffer;
        FGHandle         PerViewBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        AtmosphereBruneton::BrunetonSky_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            passData.Output = builder.readImage( renderTarget );
            passData.DepthBuffer = builder.readImage( depthBuffer );

            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.SizeInBytes = sizeof( AtmosphereBruneton::BrunetonSkyRuntimeProperties );
            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );

            passData.PerViewBuffer = builder.retrievePerViewBuffer();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            PipelineStateDesc psoDesc;
            psoDesc.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            psoDesc.RasterizerState.CullMode = eCullMode::CULL_MODE_BACK;
            psoDesc.DepthStencilState.EnableDepthTest = true;
            psoDesc.DepthStencilState.EnableDepthWrite = false;
            psoDesc.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_GEQUAL;
            psoDesc.FramebufferLayout.declareRTV( 0, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT );
            psoDesc.FramebufferLayout.declareDSV( eViewFormat::VIEW_FORMAT_D32_FLOAT );

            psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );
            psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, AtmosphereBruneton::BrunetonSky_ShaderBinding );

            // Update Parameters
            AtmosphereBruneton::BrunetonSkyProperties.EarthCenter = dkVec3f( 0.0, 0.0, -kBottomRadius / kLengthUnitInMeters );
            AtmosphereBruneton::BrunetonSkyProperties.SunDirection = dk::maths::SphericalToCarthesianCoordinates( sunHorizontalAngle, sunVerticalAngle );
            AtmosphereBruneton::BrunetonSkyProperties.SunSizeX = tanf( static_cast< f32 >( kSunAngularRadius ) );
            AtmosphereBruneton::BrunetonSkyProperties.SunSizeY = cosf( static_cast< f32 >( kSunAngularRadius ) * 5.0f );
            AtmosphereBruneton::BrunetonSkyProperties.SunExponant = 2400.0f; // TODO Should be set accordingly to the ToD
            AtmosphereBruneton::BrunetonSkyProperties.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = lutComputeModule.getSkySpectralRadianceToLuminance();
            AtmosphereBruneton::BrunetonSkyProperties.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = lutComputeModule.getSunSpectralRadianceToLuminance();
            AtmosphereBruneton::BrunetonSkyProperties.AtmosphereParams = parameters;

            cmdList->pushEventMarker( AtmosphereBruneton::BrunetonSky_EventName );
            cmdList->bindPipelineState( pipelineState );

            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* viewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            Image* outputImage = resources->getImage( passData.Output );
            Image* depthBuffer = resources->getImage( passData.DepthBuffer );

            // Bind resources

            cmdList->bindImage( AtmosphereBruneton::BrunetonSky_DepthBuffer_Hashcode, depthBuffer );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSky_TransmittanceTextureInput_Hashcode, transmittanceLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSky_IrradianceTextureInput_Hashcode, irradianceLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSky_ScatteringTextureInput_Hashcode, scatteringLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSky_MieScatteringTextureInput_Hashcode, mieScatteringLut );

            cmdList->bindConstantBuffer( PerViewBufferHashcode, viewBuffer );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

            // Update viewport (using image quality scaling)
            const CameraData* camera = resources->getMainCamera();

            dkVec2f scaledViewportSize = camera->viewportSize * camera->imageQuality;

            Viewport vp;
            vp.X = 0;
            vp.Y = 0;
            vp.Width = static_cast< i32 >( scaledViewportSize.x );
            vp.Height = static_cast< i32 >( scaledViewportSize.y );
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            ScissorRegion sr;
            sr.Top = 0;
            sr.Left = 0;
            sr.Right = static_cast< i32 >( scaledViewportSize.x );
            sr.Bottom = static_cast< i32 >( scaledViewportSize.y );

            cmdList->setViewport( vp );
            cmdList->setScissor( sr );

            cmdList->prepareAndBindResourceList();

            cmdList->updateBuffer( *passBuffer, &AtmosphereBruneton::BrunetonSkyProperties, sizeof( AtmosphereBruneton::BrunetonSkyRuntimeProperties ) );

            FramebufferAttachment output( outputImage );
            cmdList->setupFramebuffer( &output, FramebufferAttachment( depthBuffer ) );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}

void AtmosphereRenderModule::renderSkyForProbeCapture( FrameGraph& frameGraph, Image* outputImage, const ImageViewDesc& outputImageTarget, const CameraData& cameraData )
{
    struct PassData {
        FGHandle         PerPassBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        AtmosphereBruneton::BrunetonSkyProbeCapture_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.setUncullablePass();
            builder.useAsyncCompute();

            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.SizeInBytes = sizeof( AtmosphereBruneton::BrunetonSkyRuntimeProperties );
            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
            psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, AtmosphereBruneton::BrunetonSkyProbeCapture_ShaderBinding );

            // Update Parameters
            AtmosphereBruneton::BrunetonSkyProperties.EarthCenter = dkVec3f( 0.0, 0.0, -kBottomRadius / kLengthUnitInMeters );
            AtmosphereBruneton::BrunetonSkyProperties.SunDirection = dk::maths::SphericalToCarthesianCoordinates( sunHorizontalAngle, sunVerticalAngle );
            AtmosphereBruneton::BrunetonSkyProperties.SunSizeX = tanf( static_cast<f32>( kSunAngularRadius ) );
            AtmosphereBruneton::BrunetonSkyProperties.SunSizeY = cosf( static_cast< f32 >( kSunAngularRadius ) * 2.0f );
            AtmosphereBruneton::BrunetonSkyProperties.SunExponant = 2400.0f; // TODO Should be set accordingly to the ToD
            AtmosphereBruneton::BrunetonSkyProperties.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = lutComputeModule.getSkySpectralRadianceToLuminance();
            AtmosphereBruneton::BrunetonSkyProperties.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = lutComputeModule.getSunSpectralRadianceToLuminance();
            AtmosphereBruneton::BrunetonSkyProperties.AtmosphereParams = parameters;
            AtmosphereBruneton::BrunetonSkyProperties.WhitePoint = lutComputeModule.getWhitePoint();
            AtmosphereBruneton::BrunetonSkyProperties.AspectRatio = cameraData.aspectRatio;
            AtmosphereBruneton::BrunetonSkyProperties.ScreenSize = cameraData.viewportSize;
            AtmosphereBruneton::BrunetonSkyProperties.ViewDirection = cameraData.viewDirection;
            AtmosphereBruneton::BrunetonSkyProperties.RightVector = cameraData.rightVector;
            AtmosphereBruneton::BrunetonSkyProperties.UpVector = cameraData.upVector;
            AtmosphereBruneton::BrunetonSkyProperties.WorldPosition = cameraData.worldPosition;

            cmdList->pushEventMarker( AtmosphereBruneton::BrunetonSkyProbeCapture_EventName );
            cmdList->bindPipelineState( pipelineState );

            Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );

			// Bind resources
			cmdList->bindImage( AtmosphereBruneton::BrunetonSkyProbeCapture_OutputRenderTarget_Hashcode, outputImage, outputImageTarget );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSkyProbeCapture_TransmittanceTextureInput_Hashcode, transmittanceLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSkyProbeCapture_IrradianceTextureInput_Hashcode, irradianceLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSkyProbeCapture_ScatteringTextureInput_Hashcode, scatteringLut );
            cmdList->bindImage( AtmosphereBruneton::BrunetonSkyProbeCapture_MieScatteringTextureInput_Hashcode, mieScatteringLut );
            
            cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

            cmdList->prepareAndBindResourceList();

            cmdList->updateBuffer( *passBuffer, &AtmosphereBruneton::BrunetonSkyProperties, sizeof( AtmosphereBruneton::BrunetonSkyRuntimeProperties ) );

            const dkVec2f rtSize = cameraData.viewportSize;
			const u32 ThreadCountX = static_cast< u32 >( rtSize.x ) / AtmosphereBruneton::BrunetonSkyProbeCapture_DispatchX;
			const u32 ThreadCountY = static_cast< u32 >( rtSize.y ) / AtmosphereBruneton::BrunetonSkyProbeCapture_DispatchY;
            
            cmdList->dispatchCompute( ThreadCountX, ThreadCountY, 1u );

            cmdList->popEventMarker();
        }
    );
}

FGHandle AtmosphereRenderModule::renderAtmoshpericFog( FrameGraph& frameGraph, FGHandle renderTarget, FGHandle depthBuffer )
{
	return -1;
}

void AtmosphereRenderModule::destroy( RenderDevice& renderDevice )
{
    lutComputeModule.destroy( renderDevice );
}

void AtmosphereRenderModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    lutComputeModule.loadCachedResources( renderDevice, graphicsAssetCache );
}

void AtmosphereRenderModule::triggerLutRecompute( const bool forceImmediateRecompute /*= false */ )
{
    //if ( forceImmediateRecompute ) {
        lutComputeJobTodo.push( RecomputeJob() );
   // } else {
        // TODO Amortized recompute support
   // }
}
