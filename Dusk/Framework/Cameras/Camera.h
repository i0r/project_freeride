/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Maths/Frustum.h>

//#include <Shaders/ShadowMappingShared.h>

#include <Maths/Matrix.h>
#include <Maths/Vector.h>

struct CameraData
{
    dkMat4x4f  viewMatrix;
	dkMat4x4f  projectionMatrix;
    dkMat4x4f  finiteProjectionMatrix;
    dkMat4x4f  inverseFiniteProjectionMatrix;
    dkMat4x4f  inverseViewMatrix;
    dkMat4x4f  inverseProjectionMatrix;
    dkMat4x4f  viewProjectionMatrix;

    dkMat4x4f  inverseViewProjectionMatrix;
    dkVec3f    worldPosition;
    int32_t    cameraFrameNumber;

    dkVec2f    viewportSize;
    dkVec2f    inverseViewportSize;

    uint32_t   msaaSamplerCount;
    float      imageQuality;

    dkVec3f    viewDirection;

    dkVec3f     upVector;
    f32         fov;

    dkVec3f     rightVector;
    f32         aspectRatio;

    f32         depthNearPlane;
	f32         depthFarPlane;

    f32         nearPlane;
    f32         farPlane;

    union
    {
        struct {
            uint8_t enableTAA : 1;

            uint8_t : 0;
        } flags;

        uint64_t flagsData;
    };

    // Shadow mapping rendering specifics
    dkMat4x4f  depthProjectionMatrix;
    dkMat4x4f  depthViewProjectionMatrix;

    // Temporal infos
    dkMat4x4f  previousViewProjectionMatrix;
    dkMat4x4f  previousViewMatrix;
    dkVec2f    jitteringOffset;
    dkVec2f    previousJitteringOffset;

    Frustum    frustum;
};
