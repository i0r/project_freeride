/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <unordered_map>

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
    const dkChar_t* getName( const Entity& entity ) const;

    // Return true if the hashcode exist; false otherwise.
    bool            exist( const dkStringHash_t hashcode ) const;

private:
    // The memory allocator owning this instance.
    BaseAllocator*  memoryAllocator;

    // The maximum capacity of the register.
    size_t          registerCapacity;

    // Raw pointer to the memory chunk allocated.
    void*           registerData;

    // Raw pointer reinterpreted as an array of dkChar_t[Entity::MAX_NAME_LENGTH].
    const dkChar_t**      names;

    // Hashmap for quick name lookup. Key is the entity name (as a 32bits hashcode);
    // value is the associated Entity.
    std::unordered_map<dkStringHash_t, Entity> nameHashmap;
};
