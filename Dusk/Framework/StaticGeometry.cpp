/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "StaticGeometry.h"

#include "Entity.h"

constexpr size_t STATIC_GEOM_SINGLE_ENTRY_SIZE = sizeof( Model* ) + sizeof( Entity );

StaticGeometryDatabase::StaticGeometryDatabase( BaseAllocator* allocator )
    : ComponentDatabase( allocator )
{

}

StaticGeometryDatabase::~StaticGeometryDatabase()
{

}

void StaticGeometryDatabase::create( const size_t dbCapacity )
{
    // Do the database memory allocation.
    allocateMemoryChunk( STATIC_GEOM_SINGLE_ENTRY_SIZE, dbCapacity );

    // Assign each component offset from the memory chunk we have allocated (we don't want to interleave the data for
    // cache coherency).
    instanceData.ModelResource = static_cast< Model** >( databaseBuffer.Data );
    instanceData.Owner = reinterpret_cast< Entity* >( instanceData.ModelResource + dbCapacity );
}

void StaticGeometryDatabase::allocateComponent( Entity& entity )
{
	// Update buffer infos.
	Instance instance;
	if ( !freeInstances.empty() ) {
		instance = freeInstances.front();
		freeInstances.pop();
	} else {
		instance = Instance( databaseBuffer.AllocationCount );
		++databaseBuffer.AllocationCount;
		databaseBuffer.MemoryUsed += STATIC_GEOM_SINGLE_ENTRY_SIZE;
	}

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    const size_t instanceIndex = instance.getIndex();
    instanceData.ModelResource[instanceIndex] = nullptr;
    instanceData.Owner[instanceIndex] = entity;
}
