/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "EnvironmentProbeStreaming.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>
#include <Graphics/FrameGraph.h>

#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>
#include <Core/ViewFormat.h>

#include "RenderPasses/Headers/Light.h"

#include "RenderModules/Generated/BrdfLut.generated.h"

#include <Graphics/RenderModules/AtmosphereRenderModule.h>

static constexpr dkVec3f PROBE_FACE_VIEW_DIRECTION[eProbeUpdateFace::FACE_COUNT] = {
	dkVec3f( +1.0f, 0.0f, 0.0f ),
	dkVec3f( -1.0f, 0.0f, 0.0f ),
	dkVec3f( 0.0f, +1.0f, 0.0f ),
	dkVec3f( 0.0f, -1.0f, 0.0f ),
	dkVec3f( 0.0f, 0.0f, +1.0f ),
	dkVec3f( 0.0f, 0.0f, -1.0f )
};

static constexpr dkVec3f PROBE_FACE_RIGHT_DIRECTION[eProbeUpdateFace::FACE_COUNT] = {
    dkVec3f( 0.0f, 0.0f, +1.0f ),
    dkVec3f( 0.0f, 0.0f, -1.0f ),

	dkVec3f( +1.0f, 0.0f, 0.0f ),
	dkVec3f( -1.0f, 0.0f, 0.0f ),

	dkVec3f( -1.0f, 0.0f, 0.0f ),
	dkVec3f( +1.0f, 0.0f, 0.0f ),
};

static constexpr dkVec3f PROBE_FACE_UP_DIRECTION[eProbeUpdateFace::FACE_COUNT] = {
	dkVec3f( 0, 1, 0 ),
	dkVec3f( 0, 1, 0 ),

	dkVec3f( 0, 0, -1 ),
	dkVec3f( 0, 0, +1 ),

	dkVec3f( 0, 1, 0 ),
	dkVec3f( 0, 1, 0 ),
};

static dkMat4x4f GetProbeCaptureViewMatrix( const dkVec3f& probePositionWorldSpace, const eProbeUpdateFace captureStep )
{
    return dk::maths::MakeLookAtMat( probePositionWorldSpace, probePositionWorldSpace + PROBE_FACE_VIEW_DIRECTION[captureStep], PROBE_FACE_UP_DIRECTION[captureStep] );
}

EnvironmentProbeStreaming::EnvironmentProbeStreaming()
	: distantProbeUpdateStep( eProbeUpdateStep::PROBE_CAPTURE )
	, distantProbeFace( eProbeUpdateFace::FACE_X_PLUS )
	, distantProbeWriteIndex( 0 )
	, distantProbeMipIndex( 0 )
{
	for ( i32 i = 0; i < DISTANT_PROBE_BUFFER_COUNT; i++ ) {
		for ( i32 j = 0; j < PROBE_COMPONENT_COUNT; j++ ) {
			distantProbe[i][j] = nullptr;
		}
	}

	for ( i32 j = 0; j < PROBE_COMPONENT_COUNT; j++ ) {
		probeAtlas[j] = nullptr;
	}
}

EnvironmentProbeStreaming::~EnvironmentProbeStreaming()
{
	for ( i32 i = 0; i < DISTANT_PROBE_BUFFER_COUNT; i++ ) {
		for ( i32 j = 0; j < PROBE_COMPONENT_COUNT; j++ ) {
			distantProbe[i][j] = nullptr;
		}
	}

	for ( i32 j = 0; j < PROBE_COMPONENT_COUNT; j++ ) {
		probeAtlas[j] = nullptr;
	}
}

void EnvironmentProbeStreaming::updateProbeCapture( FrameGraph& frameGraph, WorldRenderer* worldRenderer )
{
	updateDistantProbe( frameGraph, worldRenderer );
}

u32 EnvironmentProbeStreaming::addProbeForStreaming( const dkVec3f& worldPosition, const f32 probeRadius, const dkMat4x4f& inverseModelMatrix )
{
    return 0;
}

void EnvironmentProbeStreaming::addProbeRelight( const u32 probeIndex )
{

}

void EnvironmentProbeStreaming::addProbeConvlution( const u32 probeIndex )
{

}

void EnvironmentProbeStreaming::addProbeRecapture( const u32 probeIndex )
{

}

void EnvironmentProbeStreaming::createResources( RenderDevice* renderDevice )
{
	ImageDesc cubemapDesc;
    cubemapDesc.width = PROBE_FACE_SIZE;
    cubemapDesc.height = PROBE_FACE_SIZE;
    cubemapDesc.depth = 1;
    cubemapDesc.arraySize = 6;
    cubemapDesc.mipCount = 1;
    cubemapDesc.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
	cubemapDesc.dimension = ImageDesc::DIMENSION_2D;
	cubemapDesc.miscFlags = ImageDesc::IS_CUBE_MAP;
    cubemapDesc.usage = RESOURCE_USAGE_DEFAULT;
    cubemapDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;

    for ( i32 i = 0; i < DISTANT_PROBE_BUFFER_COUNT; i++ ) {
        cubemapDesc.mipCount = 1;
        distantProbe[i][0] = renderDevice->createImage( cubemapDesc );

        cubemapDesc.mipCount = 6;
		for ( i32 j = 1; j < PROBE_COMPONENT_COUNT; j++ ) {
			distantProbe[i][j] = renderDevice->createImage( cubemapDesc );
		}

		// Create view for each face to capture
        ImageViewDesc viewDesc;
        viewDesc.MipCount = 1;
        viewDesc.ImageCount = 1;

        for ( i32 faceIdx = 0; faceIdx < 6; faceIdx++ ) {
            viewDesc.StartImageIndex = faceIdx;
            viewDesc.StartMipIndex = 0;
			renderDevice->createImageView( *distantProbe[i][0], viewDesc, IMAGE_VIEW_CREATE_UAV | IMAGE_VIEW_CREATE_SRV );

            for ( i32 mipIdx = 0; mipIdx < 6; mipIdx++ ) {
                viewDesc.StartMipIndex = mipIdx;

                renderDevice->createImageView( *distantProbe[i][1], viewDesc, IMAGE_VIEW_CREATE_UAV | IMAGE_VIEW_CREATE_SRV );
                renderDevice->createImageView( *distantProbe[i][2], viewDesc, IMAGE_VIEW_CREATE_UAV | IMAGE_VIEW_CREATE_SRV );
			}
        }
	}
}

void EnvironmentProbeStreaming::destroyResources( RenderDevice* renderDevice )
{
	for ( i32 i = 0; i < DISTANT_PROBE_BUFFER_COUNT; i++ ) {
		for ( i32 j = 0; j < PROBE_COMPONENT_COUNT; j++ ) {
			renderDevice->destroyImage( distantProbe[i][j] );
		}
	}
}

void EnvironmentProbeStreaming::addProbeConvlutionPass( FrameGraph& frameGraph, Image* capturedCubemap, const u32 faceIndex, const u32 mipLevel, Image* outDiffuse, Image* outSpecular )
{
	struct PassData {
		ResHandle_t         PerPassBuffer;
	};

	PassData& passData = frameGraph.addRenderPass<PassData>(
		BrdfLut::ConvoluteCubeFace_Name,
		[&]( FrameGraphBuilder& builder, PassData& passData ) {
			builder.useAsyncCompute();
			builder.setUncullablePass();

			BufferDesc perPassBufferDesc;
			perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			perPassBufferDesc.SizeInBytes = sizeof( BrdfLut::ConvoluteCubeFaceRuntimeProperties );
			passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_COMPUTE );
		},
		[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
			PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
			psoDesc.addStaticSampler( RenderingHelpers::S_TrilinearClampEdge );

			PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, BrdfLut::ConvoluteCubeFace_ShaderBinding );

			// Update Parameters
			u32 currentFaceSize = ( PROBE_FACE_SIZE >> mipLevel );

            BrdfLut::ConvoluteCubeFaceProperties.CubeFace = faceIndex;
			BrdfLut::ConvoluteCubeFaceProperties.Width = static_cast< f32 >( currentFaceSize );
			BrdfLut::ConvoluteCubeFaceProperties.Roughness = mipLevel / Max( 1.0f, std::log2( BrdfLut::ConvoluteCubeFaceProperties.Width ) );

			cmdList->pushEventMarker( BrdfLut::ConvoluteCubeFace_EventName );
			cmdList->bindPipelineState( pipelineState );

			Buffer* passBuffer = resources->getBuffer( passData.PerPassBuffer );
			
			ImageViewDesc cubeView;
			cubeView.ImageCount = 1;
			cubeView.MipCount = 1;
			cubeView.StartImageIndex = faceIndex;
			cubeView.StartMipIndex = mipLevel;

			// Bind resources
            cmdList->bindImage( BrdfLut::ConvoluteCubeFace_IBLDiffuseOutput_Hashcode, outDiffuse, cubeView );
            cmdList->bindImage( BrdfLut::ConvoluteCubeFace_IBLSpecularOutput_Hashcode, outSpecular, cubeView );
            cmdList->bindImage( BrdfLut::ConvoluteCubeFace_EnvironmentCube_Hashcode, capturedCubemap );

			cmdList->bindConstantBuffer( PerPassBufferHashcode, passBuffer );

			cmdList->prepareAndBindResourceList();

			cmdList->updateBuffer( *passBuffer, &BrdfLut::ConvoluteCubeFaceProperties, sizeof( BrdfLut::ConvoluteCubeFaceRuntimeProperties ) );

			const u32 ThreadCountX = currentFaceSize / BrdfLut::ConvoluteCubeFace_DispatchX;
			const u32 ThreadCountY = currentFaceSize / BrdfLut::ConvoluteCubeFace_DispatchY;

			cmdList->dispatchCompute( ThreadCountX, ThreadCountY, 1u );

			cmdList->popEventMarker();
		}
	);
}

void EnvironmentProbeStreaming::updateDistantProbe( FrameGraph& frameGraph, WorldRenderer* worldRenderer )
{
	switch ( distantProbeUpdateStep ) {
	case PROBE_CAPTURE:
	{
		dkMat4x4f viewMatrix = GetProbeCaptureViewMatrix( dkVec3f::Zero, static_cast<eProbeUpdateFace>( distantProbeFace ) );

		CameraData cameraData;
		cameraData.viewportSize = dkVec2f( PROBE_FACE_SIZE, PROBE_FACE_SIZE );
		cameraData.upVector = PROBE_FACE_UP_DIRECTION[distantProbeFace];
		cameraData.viewDirection = PROBE_FACE_VIEW_DIRECTION[distantProbeFace];
        cameraData.rightVector = PROBE_FACE_RIGHT_DIRECTION[distantProbeFace];
        cameraData.worldPosition = dkVec3f::Zero;
		cameraData.aspectRatio = 1.0f;

        ImageViewDesc viewDesc;
        viewDesc.StartImageIndex = distantProbeFace;
        viewDesc.StartMipIndex = 0;
        viewDesc.MipCount = 1;
		viewDesc.ImageCount = 1;

		worldRenderer->AtmosphereRendering->renderSkyForProbeCapture( frameGraph, distantProbe[distantProbeWriteIndex][0], viewDesc, cameraData );

		distantProbeFace++;
	} break;
	case PROBE_CONVOLUTION:
	{
		Image* cubemap = distantProbe[distantProbeWriteIndex][0];
		Image* irradiance = distantProbe[distantProbeWriteIndex][1];
        Image* specular = distantProbe[distantProbeWriteIndex][2];

		addProbeConvlutionPass( frameGraph, cubemap, distantProbeFace, distantProbeMipIndex, irradiance, specular );

		distantProbeMipIndex++;
	} break;
	}

	if ( distantProbeMipIndex == 6 ) {
		distantProbeMipIndex = 0;
		distantProbeFace++;
	}

	if ( distantProbeFace == FACE_COUNT ) {
		distantProbeFace = FACE_X_PLUS;
		distantProbeUpdateStep++;

		if ( distantProbeUpdateStep == PROBE_RELIGHT ) {
			distantProbeMipIndex = 0;
			distantProbeUpdateStep = PROBE_CAPTURE;
			distantProbeWriteIndex = ( ( distantProbeWriteIndex + 1 ) % 2 );
		}
	}
}
