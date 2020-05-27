/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "RenderWorld.h"

#include <Core/Allocators/PoolAllocator.h>

#include <Graphics/DrawCommandBuilder.h>
#include <Graphics/ModelBuilder.h>
#include <Graphics/Model.h>

RenderWorld::RenderWorld( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , modelAllocator( dk::core::allocate<PoolAllocator>( 
        allocator, 
        sizeof( Model ), 
        static_cast<u8>( alignof( Model ) ), 
        sizeof( Model ) * RenderWorld::MAX_MODEL_COUNT, 
        allocator->allocate( sizeof( Model )* RenderWorld::MAX_MODEL_COUNT ) 
    ) )
    , modelCount( 0 )
{
    memset( modelList, 0, sizeof( Model* ) * MAX_MODEL_COUNT );
}

RenderWorld::~RenderWorld()
{
    dk::core::free( memoryAllocator, modelAllocator );
}

Model* RenderWorld::addAndCommitParsedDynamicModel( RenderDevice* renderDevice, ParsedModel& parsedModel )
{
    Model* model = dk::core::allocate<Model>( modelAllocator, memoryAllocator, DUSK_STRING( "RenderWorldModel" ) );

    dk::graphics::BuildParsedModel( model, memoryAllocator, renderDevice, parsedModel );

    modelList[modelCount++] = model;
    
    return model;
}

void RenderWorld::collectEntities( DrawCommandBuilder* drawCmdBuilder )
{

}

dkMat4x4f* RenderWorld::allocateModelInstance( const Model* commitedModel )
{
    for ( ModelInstance& instance : modelInstances ) {
        if ( instance.ModelResource == commitedModel ) {
            instance.InstanceMatrices.push_back( dkMat4x4f::Identity );
            return &instance.InstanceMatrices.back();
        }
    }

    for ( i32 i = 0; i < modelCount; i++ ) {
        if ( modelList[i] == commitedModel ) {
            ModelInstance instance;
            instance.ModelResource = modelList[i];
            instance.InstanceMatrices.push_back( dkMat4x4f::Identity );
            return &instance.InstanceMatrices.back();
        }
    }

    DUSK_LOG_WARN( "Trying to allocate model instance with uncommited model '%s'\n", commitedModel->getName() );

    return nullptr;
}
