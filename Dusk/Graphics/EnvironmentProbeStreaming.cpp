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
#include <Graphics/RenderModules/IBLUtilitiesModule.h>

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

	dkVec3f( -1.0f, 0.0f, 0.0f ),
	dkVec3f( +1.0f, 0.0f, 0.0f ),

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

		cubemapDesc.width = 64;
		cubemapDesc.height = 64;
		distantProbe[i][1] = renderDevice->createImage( cubemapDesc );

		cubemapDesc.width = PROBE_FACE_SIZE;
		cubemapDesc.height = PROBE_FACE_SIZE;
		cubemapDesc.mipCount = PROBE_FILTERED_MIP_COUNT;
		distantProbe[i][2] = renderDevice->createImage( cubemapDesc );

		// Create view for each face to capture
        ImageViewDesc viewDesc;
        viewDesc.MipCount = 1;
        viewDesc.ImageCount = 1;

        for ( i32 faceIdx = 0; faceIdx < FACE_COUNT; faceIdx++ ) {
            viewDesc.StartImageIndex = faceIdx;
            viewDesc.StartMipIndex = 0;
			renderDevice->createImageView( *distantProbe[i][0], viewDesc, IMAGE_VIEW_CREATE_UAV | IMAGE_VIEW_CREATE_SRV );
			renderDevice->createImageView( *distantProbe[i][1], viewDesc, IMAGE_VIEW_CREATE_UAV | IMAGE_VIEW_CREATE_SRV );

            for ( i32 mipIdx = 0; mipIdx < PROBE_FILTERED_MIP_COUNT; mipIdx++ ) {
                viewDesc.StartMipIndex = mipIdx;

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
	case PROBE_COMPUTE_IRRADIANCE:
	{
		Image* cubemap = distantProbe[distantProbeWriteIndex][0];
		Image* irradiance = distantProbe[distantProbeWriteIndex][1];

		worldRenderer->IBLUtilities->addCubeFaceIrradianceComputePass( frameGraph, cubemap, irradiance, distantProbeFace );

		distantProbeFace++;
	} break;
	case PROBE_PREFILTER:
	{
		Image* cubemap = distantProbe[distantProbeWriteIndex][0];
		Image* filteredCubemap = distantProbe[distantProbeWriteIndex][2];

		worldRenderer->IBLUtilities->addCubeFaceFilteringPass( frameGraph, cubemap, filteredCubemap, distantProbeFace, distantProbeMipIndex );

		distantProbeMipIndex++;
	} break;
	}

	if ( distantProbeMipIndex == PROBE_FILTERED_MIP_COUNT ) {
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
