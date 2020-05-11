/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Ray.h"
#include "BoundingSphere.h"

struct AABB
{
    dkVec3f minPoint;
    u32  __PADDING1__;
    dkVec3f maxPoint;
    u32  __PADDING2__;
};

namespace dk
{
    namespace maths
    {
        void CreateAABB( AABB& aabb, const dkVec3f& boxCentroid, const dkVec3f& boxHalfExtents );
        void CreateAABBFromMinMaxPoints( AABB& aabb, const dkVec3f& minPoint, const dkVec3f& maxPoint );

        dkVec3f GetAABBHalfExtents( const AABB& aabb );
        dkVec3f GetAABBCentroid( const AABB& aabb );

        void ExpandAABB( AABB& aabb, const dkVec3f& pointToInclude );
        void ExpandAABB( AABB& aabb, const AABB& aabbToInclude );
        void ExpandAABB( AABB& aabb, const BoundingSphere& sphereToInclude );

        bool RayAABBIntersectionTest( const AABB& aabb, const Ray& ray, f32& minHit, f32& maxHit );

        u32 GetMaxDimensionAxisAABB( const AABB& aabb );
    }
}
