/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct Entity
{
private:
    // The identifier of this entity. Invalid if equal to INVALID_ID.
    u32             identifier;

public:
    static constexpr u32 INDEX_BITS = 22;
    static constexpr u32 INDEX_MASK = ( 1 << INDEX_BITS ) - 1;

    static constexpr u32 GENERATION_BITS = 8;
    static constexpr u32 GENERATION_MASK = ( 1 << GENERATION_BITS ) - 1;

    static constexpr u32 INVALID_ID = ~0;

    static constexpr u32 MAX_NAME_LENGTH = 256;

public:
            Entity( const u32 id = INVALID_ID, const u32 generation = INVALID_ID ) : identifier( id | generation << GENERATION_BITS ) {}

    // Return true if the entity has been allocated correctly and is valid; false otherwise.
    bool    isValid() const { return identifier != INVALID_ID; }

    // Extract the index of this entity from its identifier and return it.
    u32     extractIndex() const { return identifier & INDEX_MASK; }
    
    // Extract the generation index from its identifier and return it.
    u32     extractGenerationIndex() const { return ( identifier >> INDEX_BITS ) & GENERATION_MASK; }

    u32     getIdentifier() const { return identifier; }

    void    setIdentifier( const u32 id ) { identifier = id; }

    bool operator == ( const Entity& r )
    {
        return ( identifier == r.identifier );
    }

    bool operator < ( const Entity& r )
    {
        return ( identifier < r.identifier );
    }
};
