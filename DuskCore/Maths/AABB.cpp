/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "AABB.h"

#include "Helpers.h"

using namespace dk::maths;

void dk::maths::CreateAABBFromMinMaxPoints( AABB& aabb, const dkVec3f& minPoint, const dkVec3f& maxPoint )
{
    aabb.minPoint = minPoint;
    aabb.maxPoint = maxPoint;
}

void dk::maths::CreateAABB( AABB& aabb, const dkVec3f& boxCentroid, const dkVec3f& boxHalfExtents )
{
    aabb.minPoint = boxCentroid - boxHalfExtents;
    aabb.maxPoint = boxCentroid + boxHalfExtents;
}

dkVec3f dk::maths::GetAABBHalfExtents( const AABB& aabb )
{
    return ( ( aabb.maxPoint - aabb.minPoint ) * 0.5f );
}

dkVec3f dk::maths::GetAABBCentroid( const AABB& aabb )
{
    dkVec3f halfExtents = GetAABBHalfExtents( aabb );

    return ( aabb.minPoint + halfExtents );
}

void dk::maths::ExpandAABB( AABB& aabb, const dkVec3f& pointToInclude )
{
    aabb.minPoint = dkVec3f::min( aabb.minPoint, pointToInclude );
    aabb.maxPoint = dkVec3f::max( aabb.maxPoint, pointToInclude );
}

void dk::maths::ExpandAABB( AABB& aabb, const AABB& aabbToInclude )
{
    aabb.minPoint = dkVec3f::min( aabb.minPoint, aabbToInclude.minPoint );
    aabb.maxPoint = dkVec3f::max( aabb.maxPoint, aabbToInclude.maxPoint );
}

void dk::maths::ExpandAABB( AABB& aabb, const BoundingSphere& sphereToInclude )
{
    dkVec3f minPoint = sphereToInclude.center - sphereToInclude.radius;
    dkVec3f maxPoint = sphereToInclude.center + sphereToInclude.radius;

    aabb.minPoint = dkVec3f::min( aabb.minPoint, minPoint );
    aabb.maxPoint = dkVec3f::max( aabb.maxPoint, maxPoint );
}

bool dk::maths::RayAABBIntersectionTest( const AABB& aabb, const Ray& ray, f32& minHit, f32& maxHit )
{
    f64 txMin, txMax, tyMin, tyMax, tzMin, tzMax;

    f64 invx = 1 / ray.direction.x;
    f64 tx1 = ( aabb.minPoint.x - ray.origin.x ) * invx;
    f64 tx2 = ( aabb.maxPoint.x - ray.origin.x ) * invx;
    txMin = Min( tx1, tx2 );
    txMax = Max( tx1, tx2 );

    f64 invy = 1 / ray.direction.y;
    f64 ty1 = ( aabb.minPoint.y - ray.origin.y ) * invy;
    f64 ty2 = ( aabb.maxPoint.y - ray.origin.y ) * invy;
    tyMin = Min( ty1, ty2 );
    tyMax = Max( ty1, ty2 );

    f64 invz = 1 / ray.direction.z;
    f64 tz1 = ( aabb.minPoint.z - ray.origin.z ) * invz;
    f64 tz2 = ( aabb.maxPoint.z - ray.origin.z ) * invz;
    tzMin = Min( tz1, tz2 );
    tzMax = Max( tz1, tz2 );

    f64 mint = Max( txMin, Max( tyMin, tzMin ) );
    f64 maxt = Min( txMax, Min( tyMax, tzMax ) );

    if ( mint > maxt ) {
        return false;
    }

    minHit = static_cast<f32>( mint );
    maxHit = static_cast<f32>( maxt );

    return maxt >= mint && maxt >= 0.0;
}

u32 dk::maths::GetMaxDimensionAxisAABB( const AABB& aabb )
{
    dkVec3f halfExtents = GetAABBHalfExtents( aabb );

    u32 result = 0;

    if ( halfExtents.y > halfExtents.x ) {
        result = 1;

        if ( halfExtents.z > halfExtents.y ) {
            result = 2;
        }
    } else if ( halfExtents.z > halfExtents.x ) {
        result = 2;
    }

    return result;
}
