/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class RenderDevice;
class ShaderCache;
class Model;
class Material;

#include <Graphics/Material.h>

using ResHandle_t = uint32_t;

struct LightPassOutput {
    ResHandle_t	OutputRenderTarget;
    ResHandle_t	OutputDepthTarget;
};

LightPassOutput AddPrimitiveLightTest( FrameGraph& frameGraph, Model* modelTest, Material* materialTest, ResHandle_t perSceneBuffer, Material::RenderScenario scenario );
