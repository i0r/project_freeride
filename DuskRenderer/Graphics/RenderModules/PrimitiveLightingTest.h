/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;
class Model;

using ResHandle_t = uint32_t;

#include "Generated/PrimitiveLighting.generated.h"

PrimitiveLighting::PrimitiveLighting_Generic_Output AddPrimitiveLightTest( FrameGraph& frameGraph, Model* modelTest, ResHandle_t perSceneBuffer );
