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

#include <Graphics/RenderModules/AtmosphereRenderModule.h>

static constexpr dkVec3f PROBE_FACE_VIEW_DIRECTION[eProbeUpdateFace::FACE_COUNT] = {
	dkVec3f( +1.0f, 0.0f, 0.0f ),
	dkVec3f( -1.0f, 0.0f, 0.0f ),
	dkVec3f( 0.0f, -1.0f, 0.0f ),
	dkVec3f( 0.0f, +1.0f, 0.0f ),
	dkVec3f( 0.0f, 0.0f, +1.0f ),
	dkVec3f( 0.0f, 0.0f, -1.0f )
};

static constexpr dkVec3f PROBE_FACE_RIGHT_DIRECTION[eProbeUpdateFace::FACE_COUNT] = {
    dkVec3f( 0.0f, 0.0f, +1.0f ),
    dkVec3f( 0.0f, 0.0f, -1.0f ),

    dkVec3f( +1.0f, 0.0f, 0.0f ),
    dkVec3f( -1.0f, 0.0f, 0.0f ),
	
	dkVec3f( 0.0f, 0.0f, +1.0f ),
    dkVec3f( 0.0f, 0.0f, -1.0f ),
};

static constexpr dkVec3f PROBE_UP_VECTOR = dkVec3f( 0, 1, 0 );

static dkMat4x4f GetProbeCaptureViewMatrix( const dkVec3f& probePositionWorldSpace, const eProbeUpdateFace captureStep )
{
    return dk::maths::MakeLookAtMat( probePositionWorldSpace, probePositionWorldSpace + PROBE_FACE_VIEW_DIRECTION[captureStep], PROBE_UP_VECTOR );
}

EnvironmentProbeStreaming::EnvironmentProbeStreaming()
	: distantProbeUpdateStep( eProbeUpdateStep::PROBE_CAPTURE )
	, distantProbeFace( eProbeUpdateFace::FACE_X_PLUS )
	, distantProbeWriteIndex( 0 )
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

void EnvironmentProbeStreaming::updateDistantProbe( FrameGraph& frameGraph, WorldRenderer* worldRenderer )
{
	switch ( distantProbeUpdateStep ) {
	case PROBE_CAPTURE:
	{
		dkMat4x4f viewMatrix = GetProbeCaptureViewMatrix( dkVec3f::Zero, distantProbeFace );

		CameraData cameraData;
		cameraData.viewportSize = dkVec2f( PROBE_FACE_SIZE, PROBE_FACE_SIZE );
		cameraData.upVector = PROBE_UP_VECTOR;
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
	} break;
	case PROBE_CONVOLUTION:
	{

	} break;
	}
}
