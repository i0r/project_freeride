/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "ComponentDatabase.h"

#include "Entity.h"

ComponentDatabase::ComponentDatabase( BaseAllocator* allocator )
    : memoryAllocator( allocator )
{
    databaseBuffer.AllocationCount = 0;
    databaseBuffer.Capacity = 0;
    databaseBuffer.MemoryUsed = 0;
    databaseBuffer.Data = nullptr;
}

ComponentDatabase::~ComponentDatabase()
{
    if ( databaseBuffer.Data != nullptr ) {
        dk::core::freeArray( memoryAllocator, reinterpret_cast< u8* >( databaseBuffer.Data ) );
    }
}

Instance ComponentDatabase::lookup( const Entity& entity ) const
{
    return entityToInstanceMap.at( static_cast< size_t >( entity.extractIndex() ) );
}

bool ComponentDatabase::hasComponent( const Entity& e ) const
{
    return entityToInstanceMap.find( static_cast< size_t >( entity.extractIndex() ) ) != entityToInstanceMap.end();
}

void ComponentDatabase::allocateMemoryChunk( const size_t singleComponentSize, const size_t componentCount )
{
    const size_t allocationSize = singleComponentSize * componentCount;

    databaseBuffer.AllocationCount = 0;
    databaseBuffer.Capacity = componentCount;
    databaseBuffer.MemoryUsed = 0;
    databaseBuffer.Data = dk::core::allocateArray<u8>( memoryAllocator, allocationSize );
}
