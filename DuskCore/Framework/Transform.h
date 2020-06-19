/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
struct Entity;

#include <Maths/Vector.h>
#include <Maths/Quaternion.h>

#include "ComponentDatabase.h"

class TransformDatabase : public ComponentDatabase
{
public:
    DUSK_INLINE const dkMat4x4f&    getWorldMatrix( const Instance instance ) const { return instanceData.World[instance.getIndex()]; }
    DUSK_INLINE const dkMat4x4f&    getLocalMatrix( const Instance instance ) const { return instanceData.Local[instance.getIndex()]; }
    DUSK_INLINE dkMat4x4f&          referenceToLocalMatrix( const Instance instance ) { return instanceData.Local[instance.getIndex()]; }

public:
            TransformDatabase( BaseAllocator* allocator );
            ~TransformDatabase();

    // Create an instance of this database with a given entry count 'dbCapacity'.
    void    create( const size_t dbCapacity );

    // Allocate a component for a given entity.
    void    allocateComponent( Entity& entity );

    // Sets the local matrix f
    void    setLocal( Instance i, const dkMat4x4f& m );

private:
    struct InstanceData {
        dkVec3f*        Position;
        dkQuatf*        Rotation;
        dkVec3f*        Scale;
        Entity*         Owner;
        dkMat4x4f*      Local;
        dkMat4x4f*      World;
        Instance*       Parent;
        Instance*       FirstChild;
        Instance*       NextSibling;
        Instance*       PrevSibling;
    };

private:
    InstanceData        instanceData;

private:
    void    applyParentMatrixRecurse( const dkMat4x4f& parent, Instance i );
};
