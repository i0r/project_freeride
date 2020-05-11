/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Vector.h"

struct Ray
{
    dkVec3f     origin;
    f32         maxDepth;
    dkVec3f     direction;
    f32         minDepth;

    Ray( const dkVec3f& origin, const dkVec3f& direction )
        : origin( origin )
        , direction( direction )
        , maxDepth( std::numeric_limits<f32>::max() )
        , minDepth( std::numeric_limits<f32>::epsilon() )
    {

    }

    ~Ray()
    {
        origin.x = origin.y = origin.z = 0.0f;
        direction.x = direction.y = direction.z = 0.0f;
        maxDepth = 0.0f;
        minDepth = 0.0f;
    }
};
