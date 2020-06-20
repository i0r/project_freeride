/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "World.h"

#include "Transform.h"

static constexpr size_t MAX_ENTITY_COUNT = 10000;

World::World( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , entityDatabase( dk::core::allocate<EntityNameRegister>( memoryAllocator ) )
    , transformDatabase( dk::core::allocate<TransformDatabase>( memoryAllocator, memoryAllocator ) )
{

}

World::~World()
{

}

void World::create()
{
    transformDatabase->create( MAX_ENTITY_COUNT );
}

Entity World::allocateEmptyEntity()
{
    return entityDatabase.allocateEntity();
}
