/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EnvironmentProbe.h"

#include "Entity.h"

constexpr size_t ENVPROBE_SINGLE_ENTRY_SIZE = sizeof( f32 ) + sizeof( dkMat4x4f ) + sizeof( i32 ) + sizeof( u32 );

EnvironmentProbeDatabase::EnvironmentProbeDatabase( BaseAllocator* allocator )
    : ComponentDatabase( allocator )
{

}

EnvironmentProbeDatabase::~EnvironmentProbeDatabase()
{

}

void EnvironmentProbeDatabase::create( const size_t dbCapacity )
{
    // Do the database memory allocation.
    allocateMemoryChunk( ENVPROBE_SINGLE_ENTRY_SIZE, dbCapacity );

    // Assign each component offset from the memory chunk we have allocated (we don't want to interleave the data for
    // cache coherency).
    instanceData.Radius = static_cast< f32* >( databaseBuffer.Data );
    instanceData.InverseModelMatrix = reinterpret_cast< dkMat4x4f* >( instanceData.Radius + dbCapacity );
    instanceData.ArrayIndex = reinterpret_cast< i32* >( instanceData.InverseModelMatrix + dbCapacity );
    instanceData.DirtyFlags = reinterpret_cast< u32* >( instanceData.ArrayIndex + dbCapacity );
}

void EnvironmentProbeDatabase::allocateComponent( Entity& entity )
{
    // Update buffer infos.
    Instance instance;
    if ( !freeInstances.empty() ) {
        instance = freeInstances.front();
        freeInstances.pop();
    } else {
		instance = Instance( databaseBuffer.AllocationCount );
		++databaseBuffer.AllocationCount;
		databaseBuffer.MemoryUsed += ENVPROBE_SINGLE_ENTRY_SIZE;
    }

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    const size_t instanceIndex = instance.getIndex();
    instanceData.Radius[instanceIndex] = 1.0f;
    instanceData.InverseModelMatrix[instanceIndex] = dkMat4x4f::Identity;
    instanceData.ArrayIndex[instanceIndex] = -1;
    instanceData.DirtyFlags[instanceIndex] = 0;
}

void EnvironmentProbeDatabase::update( const f32 deltaTime )
{
    for ( size_t idx = 0; idx < databaseBuffer.AllocationCount; idx++ ) {

    }
}
