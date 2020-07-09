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
	// The memory allocator owning this instance.
	BaseAllocator*		memoryAllocator;

	// Allocator used to allocate local copies of incoming cameras.
    LinearAllocator*	cameraToRenderAllocator;

	// Allocator used to allocate local copies of incoming models.
	LinearAllocator*	staticModelsToRender;

	// Allocator used to allocate instance data to render batched models/geometry.
	LinearAllocator*	instanceDataAllocator;

private:
	// Reset entities allocators (camera, models, etc.). Should be called once the cmds are built and we don't longer need
	// the transistent data.
	void				resetAllocators();

	void                buildGeometryDrawCmds( WorldRenderer* worldRenderer, const CameraData* camera, const u8 cameraIdx, const u8 layer, const u8 viewportLayer ); 

	void				addCommand( WorldRenderer* worldRenderer, const LODBatch& batch, const u8 cameraIdx, const Material* material, const Mesh& mesh );
};
