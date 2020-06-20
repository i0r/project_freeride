/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "Cameras/Camera.h"

struct Area 
{
    // Size of a single area (in world units).
    static constexpr f32 DIMENSION = 32.0f;

    // Area X coordinate on the world grid (horizontal).
    i32 GridX;

    // Area Y coordinate on the world grid (height).
    i32 GridY;

    // Area Z coordinate on the world grid (vertical).
    //i32 GridZ;

    //// Return this area world origin (top left up of the area).
    //dkVec3f getAreaWorldOrigin() const { return dkVec3f( static_cast< f32 >( GridX ), static_cast< f32 >( GridY ), static_cast< f32 >( GridZ ) ) * DIMENSION; }

    //// Return this area world location (its center; not its origin!).
    //dkVec3f getAreaWorldLocation() const { return getAreaWorldOrigin() + dkVec3f( DIMENSION * 0.5f ); }
};

class AreaStreaming 
{
public:
            AreaStreaming();
            ~AreaStreaming();

    void    updateFromCameraView( const CameraData& camera );
};
