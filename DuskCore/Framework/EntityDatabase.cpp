/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EntityNameRegister.h"

EntityNameRegister::EntityNameRegister()
{

}

EntityNameRegister::~EntityNameRegister()
{

}

Entity EntityNameRegister::allocateEntity()
{
    constexpr u32 MINIMUM_FREE_INDICES = 1024u;

    u32 entityIndex = Entity::INVALID_ID;
    if ( freeIndices.size() > MINIMUM_FREE_INDICES ) {
        entityIndex = freeIndices.front();
        freeIndices.pop_front();
    } else {
        entityIndex = static_cast< u32 >( generationArray.size() );
        generationArray.push_back( 0 );
    }

    return Entity( entityIndex, generationArray[entityIndex] );
}

void EntityNameRegister::releaseEntity( const Entity entity )
{
    const u32 extractedIndex = entity.extractIndex();
    ++generationArray[extractedIndex];
    freeIndices.push_back( extractedIndex );
}

bool EntityNameRegister::isEntityAlive( const Entity entity ) const
{
    const u32 extractedIndex = entity.extractIndex();
    const u32 extractedGeneration = entity.extractGenerationIndex();

    return ( generationArray[extractedIndex] == extractedGeneration );
}

void EntityNameRegister::reset()
{
    generationArray.clear();
    freeIndices.clear();
}
