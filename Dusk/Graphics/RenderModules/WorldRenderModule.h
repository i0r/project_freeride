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

class WorldRenderModule
{
public:
	struct LightPassOutput
	{
		ResHandle_t	OutputRenderTarget;
		ResHandle_t	OutputDepthTarget;
		ResHandle_t	OutputVelocityTarget;
	};

public:
                        WorldRenderModule();
                        WorldRenderModule( WorldRenderModule& ) = delete;
                        WorldRenderModule& operator = ( WorldRenderModule& ) = delete;
                        ~WorldRenderModule();

    void                destroy( RenderDevice& renderDevice );
	void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

	LightPassOutput     addPrimitiveLightPass( FrameGraph& frameGraph, ResHandle_t perSceneBuffer, Material::RenderScenario scenario );

private:
	Buffer*				pickingBuffer;

	Buffer*				pickingReadbackBuffer;
};
