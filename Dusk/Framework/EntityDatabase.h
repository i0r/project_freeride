/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <vector>
#include <deque>

#include "Entity.h"

class EntityDatabase
{
public:
            EntityDatabase();
            ~EntityDatabase();

    // Allocate an entity and return it.
    Entity  allocateEntity();

    // Release the given entity to make it reusable.
    void    releaseEntity( const Entity entity );

    // Return true if the given entity is still alive and valid; false otherwise.
    bool    isEntityAlive( const Entity entity ) const;

    // Reset and invalidate every entity which have been allocated from this manager.
    void    reset();

private:
    // TODO Custom allocation/container scheme to avoid expensive realloc at runtime.
    std::vector<u32>    generationArray;
    std::deque<u32>     freeIndices;
};
