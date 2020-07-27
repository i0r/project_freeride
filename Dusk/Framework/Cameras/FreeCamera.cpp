/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "FreeCamera.h"

#include <FileSystem/FileSystemObject.h>

#include <Maths/Helpers.h>
#include <Maths/MatrixTransformations.h>

using namespace dk::maths;

FreeCamera::FreeCamera( const f32 camSpeed )
	: rightVector{ 0.0f }
	, eyeDirection{ 1.0f, 0.0f, 0.0f }
    , speedX( 0.0f )
    , speedY( 0.0f )
    , speedAltitude( 0.0f )
	, yaw( 0.0f )
	, pitch( 0.0f )
	, roll( 0.0f )
	, moveSpeed( camSpeed )
    , moveDamping( 1.0f )
    , aspectRatio( 1.0f )
    , fov( radians( 90.0f ) )
    , nearPlane( 0.1f )
{
    data = {};
}

void FreeCamera::update( const f32 frameTime )
{
    if ( yaw < -PI<f32>() ) {
        yaw += TWO_PI<f32>();
    } else if ( yaw > PI<f32>() ) {
        yaw -= TWO_PI<f32>();
    }

    pitch = clamp( pitch, -HALF_PI<f32>(), HALF_PI<f32>() );

    eyeDirection = {
        cosf( pitch ) * sinf( yaw ),
        sinf( pitch ),
        cosf( pitch ) * cosf( yaw )
    };
    eyeDirection = eyeDirection.normalize();

    const f32 yawResult = yaw - HALF_PI<f32>();

    rightVector = {
        sinf( yawResult + roll ),
        0.0f,
        cosf( yawResult + roll )
    };
    rightVector = rightVector.normalize();

    dkVec3f upVector = getUpVector();
    
    // Update world position
    dkVec3f nextWorldPosition = data.worldPosition + eyeDirection * speedX;
    data.worldPosition = lerp( data.worldPosition, nextWorldPosition, frameTime );

    if ( speedX > 0.0f ) {
        speedX -= frameTime * moveSpeed * 0.50f;
        speedX = Max( speedX, 0.0f );
    } else if ( speedX < 0.0f ) {
        speedX += frameTime * moveSpeed * 0.50f;
        speedX = Min( speedX, 0.0f );
    }

    nextWorldPosition = data.worldPosition + rightVector * speedY;
    data.worldPosition = lerp( data.worldPosition, nextWorldPosition, frameTime );

    if ( speedY > 0.0f ) {
        speedY -= frameTime * moveSpeed * 0.50f;
        speedY = Max( speedY, 0.0f );
    } else if ( speedY < 0.0f ) {
        speedY += frameTime * moveSpeed * 0.50f;
        speedY = Min( speedY, 0.0f );
    }

    auto nextUpWorldPosition = data.worldPosition + dkVec3f( 0, 1, 0 ) * speedAltitude;
    data.worldPosition = lerp( data.worldPosition, nextUpWorldPosition, frameTime );

    if ( speedAltitude > 0.0f ) {
        speedAltitude -= frameTime * moveSpeed * 0.50f;
        speedAltitude = Max( speedAltitude, 0.0f );
    } else if ( speedAltitude < 0.0f ) {
        speedAltitude += frameTime * moveSpeed * 0.50f;
        speedAltitude = Min( speedAltitude, 0.0f );
    }

    const dkVec3f lookAtVector = data.worldPosition + eyeDirection;

    // Save previous frame matrices (for temporal-based effects)
    data.previousViewProjectionMatrix = data.viewProjectionMatrix;
    data.previousViewMatrix = data.viewMatrix;

    // Rebuild matrices
    data.viewMatrix = MakeLookAtMat( data.worldPosition, lookAtVector, upVector );
    data.inverseViewMatrix = data.viewMatrix.inverse();
    data.depthViewProjectionMatrix = data.depthProjectionMatrix * data.viewMatrix;
    data.viewProjectionMatrix = data.projectionMatrix * data.viewMatrix;

    if ( data.flags.enableTAA ) {
        const uint32_t samplingIndex = ( data.cameraFrameNumber % 16 );

        static constexpr f32 TAAJitteringScale = 0.01f;
        dkVec2f projectionJittering = ( dk::maths::Hammersley2D( samplingIndex, 16 ) - dkVec2f( 0.5f ) ) * TAAJitteringScale;

        const dkVec2f offset = projectionJittering * data.inverseViewportSize;

        // Apply jittering to projection matrix
        data.projectionMatrix = MakeTranslationMat( dkVec3f( offset.x, -offset.y, 0.0f ), data.projectionMatrix );

        data.jitteringOffset = ( projectionJittering - data.previousJitteringOffset ) * 0.5f;
        data.previousJitteringOffset = projectionJittering;
    }
    
    data.viewDirection = eyeDirection;
    data.upVector = upVector;
    data.fov = fov;

    data.rightVector = rightVector;
    data.aspectRatio = aspectRatio;

    data.inverseViewProjectionMatrix = data.viewProjectionMatrix.inverse();

    // Update frustum with the latest view projection matrix
    UpdateFrustumPlanes( data.finiteProjectionMatrix, data.frustum );

    // Update camera frame number
    data.cameraFrameNumber++;
}

void FreeCamera::save( FileSystemObject* stream )
{
    stream->write( data.worldPosition );
    stream->write( moveSpeed );
    stream->write( moveDamping );
    stream->write( nearPlane );
    stream->write( fov );
    stream->write( data.viewportSize );
}

void FreeCamera::restore( FileSystemObject* stream )
{
    stream->read( data.worldPosition );
    stream->read( moveSpeed );
    stream->read( moveDamping );
    stream->read( nearPlane );
    stream->read( fov );
    stream->read( data.viewportSize );

    setProjectionMatrix( degrees( fov ), data.viewportSize.x, data.viewportSize.y, nearPlane );
}

void FreeCamera::setProjectionMatrix( const f32 fieldOfView, const f32 screenWidth, f32 screenHeight, const f32 zNear )
{
    aspectRatio = ( screenWidth / screenHeight );
    fov = radians( fieldOfView );

    nearPlane = zNear;

    data.depthNearPlane = 0.25f;
    data.depthFarPlane = 100.0f; // 250.0f;

    data.viewportSize = dkVec2f( screenWidth, screenHeight );
    data.inverseViewportSize = 1.0f / data.viewportSize;

    data.projectionMatrix = MakeInfReversedZProj( fov, aspectRatio, nearPlane );
    data.inverseProjectionMatrix = data.projectionMatrix.inverse();
	data.depthProjectionMatrix = MakeFovProj( fov, aspectRatio, data.depthNearPlane, data.depthFarPlane );

    data.nearPlane = zNear;
    // TODO Make this tweakable?
    data.farPlane = data.depthFarPlane;

    // Required for anything requiring a projection with a finite far plane (e.g. Guizmo rendering).
	data.finiteProjectionMatrix = MakeFovProj( fov, aspectRatio, nearPlane, data.depthFarPlane );
}

void FreeCamera::updateMouse( const f32 frameTime, const double mouseDeltaX, const double mouseDeltaY ) noexcept
{
    pitch -= static_cast< f32 >( mouseDeltaY ) * frameTime * 4;
    yaw += static_cast< f32 >( mouseDeltaX ) * frameTime * 4;
}

void FreeCamera::moveForward( const f32 dt )
{
    speedX += ( dt * moveSpeed );
}

void FreeCamera::moveBackward( const f32 dt )
{
    speedX -= ( dt * moveSpeed );
}

void FreeCamera::moveLeft( const f32 dt )
{
    speedY += ( dt * moveSpeed );
}

void FreeCamera::moveRight( const f32 dt )
{
    speedY -= ( dt * moveSpeed );
}

void FreeCamera::takeAltitude( const f32 dt )
{
    speedAltitude += ( dt * moveSpeed );
}

void FreeCamera::lowerAltitude( const f32 dt )
{
    speedAltitude -= ( dt * moveSpeed );
}

void FreeCamera::setOrientation( const f32 yaw, const f32 pitch, const f32 roll )
{
    this->yaw = yaw;
    this->pitch = pitch;
    this->roll = roll;
}

CameraData& FreeCamera::getData()
{
    return data;
}

void FreeCamera::setMSAASamplerCount( const uint32_t samplerCount )
{
    data.msaaSamplerCount = Max( 1u, samplerCount );
}

void FreeCamera::setImageQuality( const f32 imageQuality )
{
    data.imageQuality = Max( 0.1f, imageQuality );
}
