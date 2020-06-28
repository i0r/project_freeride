/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
struct Entity;

#include <unordered_map>
#include <queue>

struct Instance
{
public:
    static constexpr size_t INVALID_INDEX = -1;

public:
    // Return true if this instance is valid; false otherwise.
    DUSK_INLINE bool isValid() const { return index != INVALID_INDEX; }

    // Return this instance index (read only).
    DUSK_INLINE size_t getIndex() const { return index; }

public:
    Instance( size_t i = INVALID_INDEX )
        : index( i )
    {

    }

private:
    size_t index;
};

class ComponentDatabase
{
public:
                ComponentDatabase( BaseAllocator* allocator );
                ~ComponentDatabase();

    // Return the instance associated to a given entity.
    Instance    lookup( const Entity& e ) const;

    // Return true if the given entity has a component from this database; false otherwise.
    bool        hasComponent( const Entity& e ) const;

    // Remove the component attached to the given entity (does nothing if the given entity
    // don't have a component attached).
    void        removeComponent( const Entity& e );

protected:
    struct MemoryBuffer {
        // The number of instance allocated by the database.
        size_t AllocationCount;

        // The capacity of this database (in component count).
        size_t Capacity;

        // The current memory usage for this database (in bytes).
        size_t MemoryUsed;

        // The raw pointer to the allocated memory chunk (owned by memoryAllocator).
        void* Data;
    };

protected:
    // An array of references to the entities that are using this component.
    std::vector<Entity*> entities;

    // Hashmap for quick entity to instance lookup. The key is the entity index, the value
    // its linked instance.
    std::unordered_map<size_t, Instance> entityToInstanceMap;

    // A structure describing informations related to the memory used by this database.
    MemoryBuffer databaseBuffer;

    // The allocator owning this database.
    BaseAllocator* memoryAllocator;

	std::queue<Instance>    freeInstances;

protected:
    // Allocate the "raw" memory chunk used to allocate the entries of the database.
    // singleComponentSize is the size of a single component (in bytes) and componentCount
    // is the maximum number of component allocable from this database.
    void allocateMemoryChunk( const size_t singleComponentSize, const size_t componentCount );
};
