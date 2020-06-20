/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EntityNameRegister.h"

EntityNameRegister::EntityNameRegister( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , registerCapacity( 0ull )
    , registerData( nullptr )
    , names( nullptr )
{

}

EntityNameRegister::~EntityNameRegister()
{

}

void EntityNameRegister::create( const size_t entityCount )
{

}

void EntityNameRegister::setName( const Entity& entity, const dkChar_t* name )
{

}

dkChar_t* EntityNameRegister::getName( const Entity& entity ) const
{

}
