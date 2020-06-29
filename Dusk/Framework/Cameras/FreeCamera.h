/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FileSystemObject;

#include "Camera.h"

class FreeCamera
{
public:
    // Return this camera up vector (normalized).
    DUSK_INLINE dkVec3f getUpVector() const
    {
        return dkVec3f::cross( rightVector, eyeDirection ).normalize();
    }

    // Return this camera right vector (normalized).
    DUSK_INLINE dkVec3f getRightVector() const
    {
        return rightVector;
    }

    // Return this camera forward vector (normalized).
    DUSK_INLINE dkVec3f getForwardVector() const
    {
        return dkVec3f::cross( rightVector, getUpVector() ).normalize();
    }

    DUSK_INLINE dkVec3f getEyeDirection() const
    {
        return eyeDirection;
    }

public:
                    FreeCamera( const f32 camSpeed = 50.0f );
                    FreeCamera( FreeCamera& camera ) = default;
                    ~FreeCamera() = default;

    void            update( const f32 frameTime );
    void            updateMouse( const f32 frameTime, const double mouseDeltaX, const double mouseDeltaY ) noexcept;

    void            save( FileSystemObject* stream );
    void            restore( FileSystemObject* stream );

    void            setProjectionMatrix( const f32 fieldOfView, const f32 screenWidth, f32 screenHeight, const f32 zNear = 0.01f );

    void            moveForward( const f32 dt );
    void            moveBackward( const f32 dt );
    void            moveLeft( const f32 dt );
    void            moveRight( const f32 dt );

    void            takeAltitude( const f32 dt );
    void            lowerAltitude( const f32 dt );

    // NOTE Override current camera state
    void            setOrientation( const f32 yaw, const f32 pitch, const f32 roll );
    dkVec3f         getOrientation() const { return dkVec3f( yaw, pitch, roll ); }

    CameraData&     getData();

    decltype( CameraData::flags )& getUpdatableFlagset()
    {
        return data.flags;
    }

    void            setMSAASamplerCount( const uint32_t samplerCount = 1 );
    void            setImageQuality( const f32 imageQuality = 1.0f );

private:
    CameraData      data;

    dkVec3f         rightVector;
    dkVec3f         eyeDirection;

    f32             speedX;
    f32           speedY;
    f32           speedAltitude;

    f32           yaw;
    f32           pitch;
    f32           roll;

    f32           moveSpeed;
    f32           moveDamping;

    f32           aspectRatio;
    f32           fov;
    f32           nearPlane;
};
