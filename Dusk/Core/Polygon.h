/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Vector.h>

// A generic class to represent a N sided polygon.
template<i32 VertexCount>
struct dkPolygon
{
    dkVec3f Position[VertexCount];
    dkVec2f TextureCoordinates[VertexCount];
    dkVec3f Normal;

    // Calculate the normal of this polygon. The polygon vertices position must be valid.
    void calculateNormal()
    {
        Normal = dkVec3f::Zero;
        for ( i32 i = 2; i < VertexCount; i++ ) {
            Normal += dkVec3f::cross( ( Position[i - 1] - Position[0] ), ( Position[i] - Position[0] ) );
        }

        Normal = Normal.normalize();
    }

    // Scale this polygon attributes with a non-uniform scale.
    void scale( const dkVec3f& scale )
    {
        for ( i32 i = 0; i < VertexCount; i++ ) {
            Position[i] *= scale;
        }

        for ( i32 i = 0; i < VertexCount; i++ ) {
            TextureCoordinates[i] /= scale;
        }

        calculateNormal();
    }

    // Scale this polygon attributes with a uniform scale.
    void scaleUniform( const float uniformScale )
    {
        scale( dkVec3f( uniformScale ) );
    }
};

using dkTriangle = dkPolygon<3>;
using dkQuad = dkPolygon<4>;
