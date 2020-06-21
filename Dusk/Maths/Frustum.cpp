/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Frustum.h"

void dk::maths::UpdateFrustumPlanes( const dkMat4x4f& viewProjectionMatrix, Frustum& frustum )
{
    frustum.planes[0] = dkVec4f( viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0],
                                viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
                                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0],
                                viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0] );

    frustum.planes[1] = dkVec4f( viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0],
                                viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0],
                                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0],
                                viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0] );

    frustum.planes[2] = dkVec4f( viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1],
                                viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
                                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1],
                                viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1] );

    frustum.planes[3] = dkVec4f( viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1],
                                viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
                                viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1],
                                viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1] );

    // Near
    frustum.planes[5] = dkVec4f( viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2],
                                viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
                                viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2],
                                viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2] );

    // Far
    frustum.planes[4] = dkVec4f( viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2],
                                    viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
                                    viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2],
                                    viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2] );

    // Normalize them
    for ( i32 i = 0; i < 6; i++ ) {
        f32 invl = ::sqrt( frustum.planes[i].x * frustum.planes[i].x +
                                       frustum.planes[i].y * frustum.planes[i].y +
                                       frustum.planes[i].z * frustum.planes[i].z );

        frustum.planes[i] /= invl;
    }
}
