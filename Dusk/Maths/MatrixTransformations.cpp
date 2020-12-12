/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "MatrixTransformations.h"

#include "Helpers.h"

namespace dk
{
    namespace maths
    {
        dkMat4x4f MakeFovProj( const f32 fovY_radians, const f32 aspectWbyH, f32 zNear, f32 zFar )
        {
            f32 f = 1.0f / tan( fovY_radians * 0.5f );
            f32 q = zFar / ( zFar - zNear );

            return dkMat4x4f(
                f / aspectWbyH, 0.0f, 0.0f, 0.0f,
                0.0f, f, 0.0f, 0.0f,
                0.0f, 0.0f, q, 1.0f,
                0.0f, 0.0f, -q * zNear, 0.0f );
        }
            
        dkMat4x4f MakeInfReversedZProj( const f32 fovY_radians, const f32 aspectWbyH, const f32 zNear )
        {
            f32 f = 1.0f / tan( fovY_radians * 0.5f );

            return dkMat4x4f(
                f / aspectWbyH, 0.0f, 0.0f, 0.0f,
                0.0f, f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, zNear, 0.0f );
        }

        dkMat4x4f MakeTranslationMat( const dkVec3f& translation, const dkMat4x4f& matrix )
        {
            dkMat4x4f translationMatrix( matrix );
            translationMatrix[3] = matrix[0] * translation[0] + matrix[1] * translation[1] + matrix[2] * translation[2] + matrix[3];

            return translationMatrix;
        }
        
        dkMat4x4f MakeRotationMatrix( const f32 rotation_radians, const dkVec3f& axis, const dkMat4x4f& matrix )
        {
            f32 cos_angle = cos( rotation_radians );
            f32 sin_angle = sin( rotation_radians );

            dkVec3f axisNormalized = axis.normalize();
            dkVec3f tmp( axisNormalized * ( 1.0f - cos_angle ) );

            dkMat4x4f rotationMatrix( matrix );
            rotationMatrix._00 = cos_angle + tmp[0] * axisNormalized[0];
            rotationMatrix._01 = tmp[0] * axisNormalized[1] + sin_angle * axisNormalized[2];
            rotationMatrix._02 = tmp[0] * axisNormalized[2] - sin_angle * axisNormalized[1];

            rotationMatrix._10 = tmp[1] * axisNormalized[0] - sin_angle * axisNormalized[2];
            rotationMatrix._11 = cos_angle + tmp[1] * axisNormalized[1];
            rotationMatrix._12 = tmp[1] * axisNormalized[2] + sin_angle * axisNormalized[0];

            rotationMatrix._20 = tmp[2] * axisNormalized[0] + sin_angle * axisNormalized[1];
            rotationMatrix._21 = tmp[2] * axisNormalized[1] - sin_angle * axisNormalized[0];
            rotationMatrix._22 = cos_angle + tmp[2] * axisNormalized[2];

            dkMat4x4f Result;
            Result[0] = matrix[0] * rotationMatrix[0][0] + matrix[1] * rotationMatrix[0][1] + matrix[2] * rotationMatrix[0][2];
            Result[1] = matrix[0] * rotationMatrix[1][0] + matrix[1] * rotationMatrix[1][1] + matrix[2] * rotationMatrix[1][2];
            Result[2] = matrix[0] * rotationMatrix[2][0] + matrix[1] * rotationMatrix[2][1] + matrix[2] * rotationMatrix[2][2];
            Result[3] = matrix[3];
            return Result;
        }

        dkMat4x4f MakeScaleMat( const dkVec3f& scale, const dkMat4x4f& matrix )
        {
            dkMat4x4f scaleMatrix( matrix );
            scaleMatrix[0] = matrix[0] * scale[0];
            scaleMatrix[1] = matrix[1] * scale[1];
            scaleMatrix[2] = matrix[2] * scale[2];
            scaleMatrix[3] = matrix[3];
            
            return scaleMatrix;
        }

        dkMat4x4f MakeLookAtMat( const dkVec3f& eye, const dkVec3f& center, const dkVec3f& up )
        {
            dkVec3f zaxis = ( ( center - eye ).normalize() );
            dkVec3f xaxis = dkVec3f::cross( up, zaxis ).normalize();
            dkVec3f yaxis = dkVec3f::cross( zaxis, xaxis );

            dkMat4x4f lookAtMat = dkMat4x4f::Identity;
            lookAtMat[0][0] = xaxis.x;
            lookAtMat[1][0] = xaxis.y;
            lookAtMat[2][0] = xaxis.z;
            lookAtMat[0][1] = yaxis.x;
            lookAtMat[1][1] = yaxis.y;
            lookAtMat[2][1] = yaxis.z;
            lookAtMat[0][2] = zaxis.x;
            lookAtMat[1][2] = zaxis.y;
            lookAtMat[2][2] = zaxis.z;
            lookAtMat[3][0] = -dkVec3f::dot( xaxis, eye );
            lookAtMat[3][1] = -dkVec3f::dot( yaxis, eye );
            lookAtMat[3][2] = -dkVec3f::dot( zaxis, eye );

            return lookAtMat;
        }
        
        dkMat4x4f MakeOrtho( const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 zNear, const f32 zFar )
        {
            f32 ReciprocalWidth = 1.0f / ( right - left );
            f32 ReciprocalHeight = 1.0f / ( top - bottom );
            f32 fRange = 1.0f / ( zFar - zNear );

            dkMat4x4f matrix = dkMat4x4f::Identity;
            matrix._00 = ReciprocalWidth + ReciprocalWidth;
            matrix._11 = ReciprocalHeight + ReciprocalHeight;
            matrix._22 = fRange;

            matrix._30 = -( right + left ) * ReciprocalWidth;
            matrix._31 = -( top + bottom ) * ReciprocalHeight;
            matrix._32 = -fRange * zNear;

            return matrix;
        }
    }
}
