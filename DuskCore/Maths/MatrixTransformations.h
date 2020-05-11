/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#pragma once

#include "Matrix.h"

namespace dk
{
    namespace maths
    {
        dkMat4x4f  MakeInfReversedZProj( const f32 fovY_radians, const f32 aspectWbyH, const f32 zNear );
        dkMat4x4f  MakeFovProj( const f32 fovY_radians, const f32 aspectWbyH, f32 zNear, f32 zFar );

        dkMat4x4f  MakeTranslationMat( const dkVec3f& translation, const dkMat4x4f& matrix = dkMat4x4f::Identity );
        dkMat4x4f  MakeRotationMatrix( const f32 rotation_radians, const dkVec3f& axis, const dkMat4x4f& matrix = dkMat4x4f::Identity );
        dkMat4x4f  MakeScaleMat( const dkVec3f& scale, const dkMat4x4f& matrix = dkMat4x4f::Identity );
    
        dkMat4x4f  MakeLookAtMat( const dkVec3f& eye, const dkVec3f& center, const dkVec3f& up );
        dkMat4x4f  MakeOrtho( const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 zNear, const f32 zFar );
    }
}
