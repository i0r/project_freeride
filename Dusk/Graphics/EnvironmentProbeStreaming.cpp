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

static dkMat4x4f GetProbeCaptureViewMatrix( const dkVec3f& probePositionWorldSpace, const eProbeUpdateFace captureStep )
{
	switch ( captureStep ) {
	case eProbeUpdateFace::FACE_X_PLUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( 1.0f, 0.0f, 0.0f ),
			dkVec3f( 0.0f, 1.0f, 0.0f ) );

	case eProbeUpdateFace::FACE_X_MINUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( -1.0f, 0.0f, 0.0f ),
			dkVec3f( 0.0f, 1.0f, 0.0f ) );

	case eProbeUpdateFace::FACE_Y_MINUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( 0.0f, -1.0f, 0.0f ),
			dkVec3f( 0.0f, 0.0f, 1.0f ) );

	case eProbeUpdateFace::FACE_Y_PLUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( 0.0f, 1.0f, 0.0f ),
			dkVec3f( 0.0f, 0.0f, -1.0f ) );

	case eProbeUpdateFace::FACE_Z_PLUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( 0.0f, 0.0f, 1.0f ),
			dkVec3f( 0.0f, 1.0f, 0.0f ) );

	case eProbeUpdateFace::FACE_Z_MINUS:
		return dk::maths::MakeLookAtMat(
			probePositionWorldSpace,
			probePositionWorldSpace + dkVec3f( 0.0f, 0.0f, -1.0f ),
			dkVec3f( 0.0f, 1.0f, 0.0f ) );
	}

	return dkMat4x4f::Identity;
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

void EnvironmentProbeStreaming::updateProbeCapture( FrameGraph& frameGraph )
{
	updateDistantProbe( frameGraph );
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

void EnvironmentProbeStreaming::updateDistantProbe( FrameGraph& frameGraph )
{
	switch ( distantProbeUpdateStep ) {
	case PROBE_CAPTURE:
	{
		dkMat4x4f viewMatrix = GetProbeCaptureViewMatrix( dkVec3f::Zero, distantProbeFace );
	} break;
	case PROBE_CONVOLUTION:
	{

	} break;
	}
}
