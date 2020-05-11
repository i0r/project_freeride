/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#if 0
#include "ShadowMappingShared.h"

#include <Maths/MatrixTransformations.h>
#include <Framework/Cameras/Camera.h>

#include "Light.h"

namespace
{
    // CSM Settings
    static constexpr float SHADOW_MAP_DIM = static_cast<float>( CSM_SHADOW_MAP_DIMENSIONS );

    static constexpr float MinDistance = 0.0f;
    static constexpr float MaxDistance = 1.0f;

    static constexpr float nearClip = 25.0f; // 250f;
    static constexpr float farClip = 250.0f;
    static constexpr float clipRange = farClip - nearClip;

    static constexpr float minZ = nearClip + MinDistance * clipRange;
    static constexpr float maxZ = nearClip + MaxDistance * clipRange;

    static constexpr float range = maxZ - minZ;
    static constexpr float ratio = maxZ / minZ;

    // Compute the split distances based on the partitioning mode
    static constexpr float CascadeSplits[4] = {
        MinDistance + 0.050f * MaxDistance,
        MinDistance + 0.150f * MaxDistance,
        MinDistance + 0.500f * MaxDistance,
        MinDistance + 1.000f * MaxDistance
    };

    static dkVec3f TransformVec3( const dkVec3f& vector, const dkMat4x4f& matrix )
    {
        float x = ( vector.x * matrix[0].x ) + ( vector.y * matrix[1].x ) + ( vector.z * matrix[2].x ) + matrix[3].x;
        float y = ( vector.x * matrix[0].y ) + ( vector.y * matrix[1].y ) + ( vector.z * matrix[2].y ) + matrix[3].y;
        float z = ( vector.x * matrix[0].z ) + ( vector.y * matrix[1].z ) + ( vector.z * matrix[2].z ) + matrix[3].z;
        float w = ( vector.x * matrix[0].w ) + ( vector.y * matrix[1].w ) + ( vector.z * matrix[2].w ) + matrix[3].w;

        return dkVec3f( x, y, z ) / w;
    }
}

namespace dk
{
    namespace renderer
    {
        static dkMat4x4f CSMCreateGlobalShadowMatrix( const dkVec3f& lightDirNormalized, const dkMat4x4f& viewProjection )
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

        static void CSMComputeSliceData( const DirectionalLightData* lightData, const int cascadeIdx, CameraData* cameraData )
        {
            // Get the 8 points of the view frustum in world space
            dkVec3f frustumCornersWS[8] = {
                dkVec3f( -1.0f,  1.0f, 0.0f ),
                dkVec3f( 1.0f,  1.0f, 0.0f ),
                dkVec3f( 1.0f, -1.0f, 0.0f ),
                dkVec3f( -1.0f, -1.0f, 0.0f ),
                dkVec3f( -1.0f,  1.0f, 1.0f ),
                dkVec3f( 1.0f,  1.0f, 1.0f ),
                dkVec3f( 1.0f, -1.0f, 1.0f ),
                dkVec3f( -1.0f, -1.0f, 1.0f ),
            };

            float prevSplitDist = ( cascadeIdx == 0 ) ? MinDistance : CascadeSplits[cascadeIdx - 1];
            float splitDist = CascadeSplits[cascadeIdx];

            auto inverseViewProjection = cameraData->depthProjectionMatrix.inverse().transpose();

            for ( int i = 0; i < 8; ++i ) {
                frustumCornersWS[i] = TransformVec3( frustumCornersWS[i], inverseViewProjection );
            }

            // Get the corners of the current cascade slice of the view frustum
            for ( int i = 0; i < 4; ++i ) {
                dkVec3f cornerRay = frustumCornersWS[i + 4] - frustumCornersWS[i];
                dkVec3f nearCornerRay = cornerRay * prevSplitDist;
                dkVec3f farCornerRay = cornerRay * splitDist;
                frustumCornersWS[i + 4] = frustumCornersWS[i] + farCornerRay;
                frustumCornersWS[i] = frustumCornersWS[i] + nearCornerRay;
            }

            // Calculate the centroid of the view frustum slice
            dkVec3f frustumCenter( 0.0f );
            for ( int i = 0; i < 8; ++i ) {
                frustumCenter = frustumCenter + frustumCornersWS[i];
            }

            frustumCenter *= 1.0f / 8.0f;

            dkVec3f minExtents;
            dkVec3f maxExtents;

            // Pick the up vector to use for the light camera
            const dkVec3f upDir = dkVec3f( 0, 1, 0 );

            float sphereRadius = 0.0f;
            for ( int i = 0; i < 8; ++i ) {
                float dist = ( frustumCornersWS[i] - frustumCenter ).length();
                sphereRadius = dk::maths::max( sphereRadius, dist );
            }

            sphereRadius = std::ceil( sphereRadius * 16.0f ) / 16.0f;

            maxExtents = dkVec3f( sphereRadius, sphereRadius, sphereRadius );
            minExtents = -maxExtents;

            dkVec3f cascadeExtents = maxExtents - minExtents;

            // Get position of the shadow camera
            dkVec3f shadowCameraPos = frustumCenter + lightData->direction.normalize() * -minExtents.z;

            // Come up with a new orthographic camera for the shadow caster
            dkMat4x4f shadowCamera = dk::maths::MakeOrtho( minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, cascadeExtents.z );
            dkMat4x4f shadowLookAt = dk::maths::MakeLookAtMat( shadowCameraPos, frustumCenter, upDir );
            dkMat4x4f shadowMatrix = shadowCamera * shadowLookAt;

            // Create the rounding matrix, by projecting the world-space origin and determining
            // the fractional offset in texel space
            dkVec3f shadowOrigin = TransformVec3( dkVec3f( 0.0f ), shadowMatrix ) * ( SHADOW_MAP_DIM / 2.0f );
            dkVec3f roundedOrigin = dkVec3f( roundf( shadowOrigin.x ), roundf( shadowOrigin.y ), roundf( shadowOrigin.z ) );

            dkVec3f roundOffset = roundedOrigin - shadowOrigin;
            roundOffset = roundOffset * ( 2.0f / SHADOW_MAP_DIM );
            roundOffset.z = 0.0f;

            shadowCamera[3].x += roundOffset.x;
            shadowCamera[3].y += roundOffset.y;
            shadowCamera[3].z += roundOffset.z;

            shadowMatrix = shadowCamera * shadowLookAt;
            cameraData->shadowViewMatrix[cascadeIdx] = shadowMatrix.transpose();

            // Apply the scale/offset matrix, which transforms from [-1,1]
            // post-projection space to [0,1] UV space
            dkMat4x4f texScaleBias( 
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, -0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.5f, 0.0f, 1.0f );

            shadowMatrix = texScaleBias * shadowMatrix;

            // Store the split distance in terms of view space depth
            cameraData->cascadeSplitDistances[cascadeIdx] = nearClip + splitDist * clipRange;

            // Calculate the position of the lower corner of the cascade partition, in the UV space
            // of the first cascade partition
            dkMat4x4f invCascadeMat = shadowMatrix.inverse().transpose();
            dkVec3f cascadeCorner = TransformVec3( dkVec3f( 0.0f ), invCascadeMat );
            cascadeCorner = TransformVec3( cascadeCorner, cameraData->globalShadowMatrix );

            // Do the same for the upper corner
            dkVec3f otherCorner = TransformVec3( dkVec3f( 1.0f ), invCascadeMat );
            otherCorner = TransformVec3( otherCorner, cameraData->globalShadowMatrix );

            // Calculate the scale and offset
            dkVec3f cascadeScale = dkVec3f( 1.0f, 1.0f, 1.0f ) / ( otherCorner - cascadeCorner );

            cameraData->cascadeOffsets[cascadeIdx] = dkVec4f( -cascadeCorner, 0.0f );
            cameraData->cascadeScales[cascadeIdx] = dkVec4f( cascadeScale, 1.0f );
        }
    }
}
#endif