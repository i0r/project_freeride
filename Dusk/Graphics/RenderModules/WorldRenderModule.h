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
	// Return picked entity id (or Entity::INVALID_ID if no entity is picked/picking has not been
	// requested by the application).
	DUSK_INLINE u32 getAndConsumePickedEntityId() { isResultAvailable = false; return pickedEntityId; }

	DUSK_INLINE const bool isPickingResultAvailable() const { return isResultAvailable; }

public:
                        WorldRenderModule();
                        WorldRenderModule( WorldRenderModule& ) = delete;
                        WorldRenderModule& operator = ( WorldRenderModule& ) = delete;
                        ~WorldRenderModule();

    void                destroy( RenderDevice& renderDevice );
	void                loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

	LightPassOutput     addPrimitiveLightPass( FrameGraph& frameGraph, ResHandle_t perSceneBuffer, ResHandle_t depthPrepassBuffer, Material::RenderScenario scenario, Image* iblDiffuse, Image* iblSpecular );

	ResHandle_t			addDepthPrepass( FrameGraph& frameGraph );

	void				setDefaultBrdfDfgLut( Image* brdfDfgLut );

private:
	// Buffer used to store picking infos on the GPU.
	Buffer*				pickingBuffer;

	// Staging buffer used to read back picking results from the GPU.
	Buffer*				pickingReadbackBuffer;

	// Frame index at which the latest picking request has been done.
	u32					pickingFrameIndex;

	// Index of the latest picked entity.
	u32					pickedEntityId;

	// True if result readback is available; false otherwise.
	bool				isResultAvailable;

	// Default BRDF DFG LUT. Not really a great design (especially if we want to support multiple BRDF later) so it'll
	// most likely be refactored in the future.
	Image*				brdfDfgLut;

private:
	void				clearPickingBuffer( FrameGraph& frameGraph );
};
