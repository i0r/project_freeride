/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Primitives.h"

#include "Helpers.h"
#include "Vector.h"

using namespace dk::maths;

PrimitiveData dk::maths::CreateSpherePrimitive( const u32 stacks, const u32 slices )
{
    PrimitiveData primitive;

    for ( u32 stack = 0; stack <= stacks; stack++ ) {
        f32 thetaV = PI<f32>() * ( ( f32 )stack / ( f32 )stacks );
        f32 r = sin( thetaV );
        f32 y = cos( thetaV );

        for ( u32 slice = 0; slice <= slices; slice++ ) {
            f32 thetaH = 2.0f * PI<f32>() * ( ( f32 )slice / ( f32 )slices );
            f32 x = r * cos( thetaH );
            f32 z = r * sin( thetaH );

            primitive.position.push_back( x );
            primitive.position.push_back( y );
            primitive.position.push_back( z );

            f32 pointInverseLength = 1.0f / dkVec3f( x, y, z ).length();
            primitive.normal.push_back( x * pointInverseLength );
            primitive.normal.push_back( y * pointInverseLength );
            primitive.normal.push_back( z * pointInverseLength );

            f32 u = ( f32 )slice / ( f32 )slices;
            f32 v = ( f32 )stack / ( f32 )stacks;

            primitive.uv.push_back( u );
            primitive.uv.push_back( 1.0f - v );
        }
    }

    for ( u32 stack = 0; stack < stacks; stack++ ) {
        for ( u32 slice = 0; slice < slices; slice++ ) {
            u16 count = slice + ( ( slices + 1 ) * stack );

            primitive.indices.push_back( count );
            primitive.indices.push_back( count + 1 );
            primitive.indices.push_back( count + slices + 2 );

            primitive.indices.push_back( count );
            primitive.indices.push_back( count + slices + 2 );
            primitive.indices.push_back( count + slices + 1 );
        }
    }

    return primitive;
}

PrimitiveData dk::maths::CreateQuadPrimitive()
{
    PrimitiveData primitive;

    primitive.position = {
        -1.0f, -1.0f, 1.0f,
        +1.0f, -1.0f, 1.0f,
        +1.0f, +1.0f, 1.0f,
        -1.0f, +1.0f, 1.0f,
    };

    primitive.normal = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };

    primitive.uv = {
        +0.0f, +0.0f,
        +1.0f, +0.0f,
        +1.0f, +1.0f,
        +0.0f, +1.0f,
    };

    primitive.indices = {
        0u, 1u, 2u, 2u, 3u, 0u
    };

    return primitive;
}
