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

	registerData = dk::core::allocateArray<u8>( memoryAllocator, registerCapacity * sizeof( char ) * Entity::MAX_NAME_LENGTH );
    
    memset( registerData, '\0', registerCapacity * sizeof( char ) * Entity::MAX_NAME_LENGTH );

	names = reinterpret_cast< char* >( registerData );
}

void EntityNameRegister::setName( const Entity& entity, const char* name )
{
    strcpy( &names[entity.extractIndex() * Entity::MAX_NAME_LENGTH], name );

    dkStringHash_t nameHashcode = dk::core::CRC32( name );
    nameHashmap[nameHashcode] = entity;
}

char* EntityNameRegister::getNameBuffer( const Entity& entity )
{
	return &names[entity.extractIndex() * Entity::MAX_NAME_LENGTH];
}

const char* EntityNameRegister::getName( const Entity& entity ) const
{
	return &names[entity.extractIndex() * Entity::MAX_NAME_LENGTH];
}

bool EntityNameRegister::exist( const dkStringHash_t hashcode ) const
{
    return ( nameHashmap.find( hashcode ) != nameHashmap.end() );
}

void EntityNameRegister::releaseEntityName( const Entity& entity )
{
    const char* entityName = getName( entity );
    if ( entityName == nullptr ) {
        return;
    }

	dkStringHash_t nameHashcode = dk::core::CRC32( entityName );
    nameHashmap.erase( nameHashcode );

    memset( &names[entity.extractIndex() * Entity::MAX_NAME_LENGTH], '\0', sizeof( char ) * Entity::MAX_NAME_LENGTH );
}

const std::unordered_map<dkStringHash_t, Entity>& EntityNameRegister::getRegisterHashmap() const
{
    return nameHashmap;
}
