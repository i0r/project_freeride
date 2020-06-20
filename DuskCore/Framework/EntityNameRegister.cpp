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
    dk::core::free( memoryAllocator, registerData );
    names = nullptr;
}

void EntityNameRegister::create( const size_t entityCount )
{
	registerCapacity = entityCount;

	registerData = dk::core::allocateArray<u8>( memoryAllocator, registerCapacity * sizeof( dkChar_t ) * Entity::MAX_NAME_LENGTH );

	names = reinterpret_cast< const dkChar_t** >( registerData );
}

void EntityNameRegister::setName( const Entity& entity, const dkChar_t* name )
{
    names[entity.extractIndex()] = name;

    dkStringHash_t nameHashcode = dk::core::CRC32( name );
    nameHashmap[nameHashcode] = entity;
}

const dkChar_t* EntityNameRegister::getName( const Entity& entity ) const
{
    return names[entity.extractIndex()];
}

bool EntityNameRegister::exist( const dkStringHash_t hashcode ) const
{
    return ( nameHashmap.find( hashcode ) != nameHashmap.end() );
}
