/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Ray.h"

template <typename Precision, i32 ScalarCount>
struct Vector;
using dkVec3f = Vector<f32, 3>;

struct BoundingSphere
{
    dkVec3f   center;
    f32       radius;
};

namespace dk
{
    namespace maths
    {
        void CreateSphere( BoundingSphere& sphere, const dkVec3f& sphereCenter, const f32 sphereRadius );
        bool SphereSphereIntersectionTest( const BoundingSphere& left, const BoundingSphere& right );
        bool RaySphereIntersectionTest( const BoundingSphere& sphere, const Ray& ray, f32& hitDistance );
    }
}
