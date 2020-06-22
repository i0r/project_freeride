/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Model;
class DrawCommandBuilder;
class RenderDevice;
class PoolAllocator;
class GraphicsAssetCache;

#include <Parsing/GeometryParser.h>

#include <Maths/Matrix.h>

// The RenderWorld is responsible for render entity management (allocation/streaming/update/etc.).
class RenderWorld 
{
public:
    static constexpr i32 MAX_MODEL_COUNT = 1024;

public:
    struct ModelInstance 
    {
        // The Resource used to render this model instance.
        const Model*            ModelResource;

        // Array of instance model matrix (if the array is empty the model is skipped).
        std::vector<dkMat4x4f>  InstanceMatrices;
    };

public:
                    RenderWorld( BaseAllocator* allocator );
                    RenderWorld( RenderWorld& ) = default;
                    ~RenderWorld();

    // Add a Model to the RenderWorld and immediately upload its data to the GPU.
    // This should be used in an editor context only.
    Model*          addAndCommitParsedDynamicModel( RenderDevice* renderDevice, ParsedModel& parsedModel, GraphicsAssetCache* graphicsAssetCache );

    // Allocate an instance of a given model. Return a pointer to its model matrix if the model exists
    // and has already been commited to the world; nil otherwise.
    dkMat4x4f*      allocateModelInstance( const Model* commitedModel );

private:
    // The memory allocator owning this instance.
    BaseAllocator*  memoryAllocator;

    // Allocator for WorldModel
    PoolAllocator*  modelAllocator;

    // List of active model in the world (allocated by modelAllocator).
    // Note that this is a resource list; not a list of instance in the world!
    Model*          modelList[MAX_MODEL_COUNT];

    // Allocated model count (resource; not instance).
    i32             modelCount;

    // It's shit atm but it just werks.
    // Array of model instances.
    std::vector<ModelInstance>  modelInstances;
};
