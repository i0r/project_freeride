/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Vector.h"
#include "Matrix.h"

struct Frustum
{
    dkVec4f planes[6];
};
static_assert( std::is_pod<Frustum>(), "Frustum must be POD (since it is used on the GPU side; see Camera constant buffer declaration" );

namespace dk
{
    namespace maths
    {
        void UpdateFrustumPlanes( const dkMat4x4f& viewProjectionMatrix, Frustum& frustum );
    }
}
