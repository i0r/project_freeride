/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "Cameras/Camera.h"

class AreaStreaming 
{
public:
            AreaStreaming();
            ~AreaStreaming();

    void    updateFromCameraView( const CameraData& camera );
};
