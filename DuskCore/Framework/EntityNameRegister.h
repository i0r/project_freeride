/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <vector>
#include <deque>

#include "Entity.h"

class EntityNameRegister
{
public:
                    EntityNameRegister( BaseAllocator* allocator );
                    ~EntityNameRegister();

    // Create the register with a given entityCount capacity.
    void            create( const size_t entityCount );

    // Set the name of a given entity. Name length must be between 1 and Entity::MAX_NAME_LENGTH characters.
    void            setName( const Entity& entity, const dkChar_t* name );

    // Return the name of a given entity. Return null if the entity is invalid or does not exist.
    dkChar_t*       getName( const Entity& entity ) const;

private:
    // The memory allocator owning this instance.
    BaseAllocator*  memoryAllocator;

    // The maximum capacity of the register.
    size_t          registerCapacity;

    // Raw pointer to the memory chunk allocated.
    void*           registerData;

    // Raw pointer reinterpreted as an array of dkChar_t[Entity::MAX_NAME_LENGTH].
    dkChar_t**      names;
};
