/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "BoundingSphere.h"

#include "Vector.h"

void dk::maths::CreateSphere( BoundingSphere& sphere, const dkVec3f& sphereCenter, const f32 sphereRadius )
{
    sphere.center = sphereCenter;
    sphere.radius = sphereRadius;
}

bool dk::maths::SphereSphereIntersectionTest( const BoundingSphere& left, const BoundingSphere& right )
{
    f32 r = left.radius + right.radius;
    r *= r;

    f32 xDistance = ( left.center.x + right.center.x );
    f32 yDistance = ( left.center.y + right.center.y );
    f32 zDistance = ( left.center.z + right.center.z );

    xDistance *= xDistance;
    yDistance *= yDistance;
    zDistance *= zDistance;

    return r < ( xDistance + yDistance + zDistance );
}

bool dk::maths::RaySphereIntersectionTest( const BoundingSphere& sphere, const Ray& ray, f32& hitDistance )
{
    f32 sphereRadiusSqr = sphere.radius * sphere.radius;

    dkVec3f diff = sphere.center - ray.origin;
    f32 t0 = dkVec3f::dot( diff, ray.direction );
    f32 dSquared = dkVec3f::dot( diff, diff ) - t0 * t0;
    if ( dSquared > sphereRadiusSqr ) {
        return false;
    }

    f32 t1 = ::sqrt( sphereRadiusSqr - dSquared );
    hitDistance = t0 > t1 + dkVec3f::EPSILON ? t0 - t1 : t0 + t1;

    return ( hitDistance > dkVec3f::EPSILON );
}
