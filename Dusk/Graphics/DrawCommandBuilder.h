/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Maths/Matrix.h>

class BaseAllocator;
class LinearAllocator;
class WorldRenderer;
class Model;
class FrameGraph;
struct CameraData;
struct LODBatch;
class Material;
struct Mesh;

class DrawCommandBuilder
{
public:
#if DUSK_DEVBUILD
	// Return the number of primitive culled for the Geometry RenderPass for a given submitted camera (index 0 is usually
	// the main camera of the application).
	DUSK_INLINE const u32 getCulledGeometryPrimitiveCount( const u32 cameraIdx = 0u ) const 
	{ 
		DUSK_DEV_ASSERT( cameraIdx < MAX_SIMULTANEOUS_VIEWPORT_COUNT&& cameraIdx >= 0u, "Index out of bounds!" );
		return culledGeometryPrimitiveCount[cameraIdx]; 
	}
#endif

public:
						DrawCommandBuilder( BaseAllocator* allocator );
						DrawCommandBuilder( DrawCommandBuilder& ) = delete;
						DrawCommandBuilder& operator = ( DrawCommandBuilder& ) = delete;
						~DrawCommandBuilder();

	// Add a world camera to render. A camera should be added to the list only if 
	// it require to render the active world geometry.
    void    			addWorldCameraToRender( CameraData* cameraData );

	// Add a static model instance to the world. If the model has already been added
	// to the world, the model will automatically use instantied draw (no action/
	// batching is required by the user).
	void				addStaticModelInstance( const Model* model, const dkMat4x4f& modelMatrix, const u32 entityIndex = 0 );

	// Build render queues for each type of render scenario and enqueued cameras.
	// This call will also update/stream the light grid entities.
	void				prepareAndDispatchCommands( WorldRenderer* worldRenderer );

private:
    static constexpr size_t MAX_SIMULTANEOUS_VIEWPORT_COUNT = 8;

private:
	// The memory allocator owning this instance.
	BaseAllocator*		memoryAllocator;

	// Allocator used to allocate local copies of incoming cameras.
    LinearAllocator*	cameraToRenderAllocator;

	// Allocator used to allocate local copies of incoming models.
	LinearAllocator*	staticModelsToRender;

	// Allocator used to allocate instance data to render batched models/geometry.
	LinearAllocator*	instanceDataAllocator;

	// Allocator used to allocate instance data for GPU driven draw call submit.
	LinearAllocator*	gpuInstanceDataAllocator;

#if DUSK_DEVBUILD
	// The number of primitive geometry culled (either by occlusion culling or frustum culling).
	u32					culledGeometryPrimitiveCount[MAX_SIMULTANEOUS_VIEWPORT_COUNT];
#endif

private:
	// Reset entities allocators (camera, models, etc.). Should be called once the cmds are built and we don't longer need
	// the transistent data.
	void				resetAllocators();

	// Build Draw Commands for the different layers and viewports layers of a given camera.
	void                buildGeometryDrawCmds( WorldRenderer* worldRenderer, const CameraData* camera, const u8 cameraIdx );

	// Build instances informations to generate shadow draw commands on the GPU.
	void				buildShadowGPUDrivenCullCmds( WorldRenderer* worldRenderer, const CameraData* camera );
};
