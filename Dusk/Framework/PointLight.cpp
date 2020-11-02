/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "PointLight.h"

#include "Entity.h"

#include "Graphics/LightingConstants.h"

constexpr size_t POINT_LIGHT_SINGLE_ENTRY_SIZE = sizeof( PointLightGPU ) + sizeof( Entity );

PointLightDatabase::PointLightDatabase( BaseAllocator* allocator )
    : ComponentDatabase( allocator )
{

}

PointLightDatabase::~PointLightDatabase()
{

}

void PointLightDatabase::create( const size_t dbCapacity )
{
    // Do the database memory allocation.
    allocateMemoryChunk( POINT_LIGHT_SINGLE_ENTRY_SIZE, dbCapacity );

    // Assign each component offset from the memory chunk we have allocated (we don't want to interleave the data for
    // cache coherency).
    instanceData.PointLight = static_cast< PointLightGPU* >( databaseBuffer.Data );
    instanceData.Owner = reinterpret_cast< Entity* >( instanceData.PointLight + dbCapacity );
}

void PointLightDatabase::allocateComponent( Entity& entity )
{
	// Update buffer infos.
	Instance instance;
	if ( !freeInstances.empty() ) {
		instance = freeInstances.front();
		freeInstances.pop();
	} else {
		instance = Instance( databaseBuffer.AllocationCount );
		++databaseBuffer.AllocationCount;
		databaseBuffer.MemoryUsed += POINT_LIGHT_SINGLE_ENTRY_SIZE;
	}

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    const size_t instanceIndex = instance.getIndex();
    instanceData.PointLight[instanceIndex] = PointLightGPU{ dk::graphics::MercuryVaporBulb.Color, 1600.0f, dkVec3f::Zero, 2.0f };
    instanceData.Owner[instanceIndex] = entity;
}
