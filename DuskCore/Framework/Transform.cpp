/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Transform.h"

#include "Entity.h"

static constexpr size_t SINGLE_ENTRY_SIZE = sizeof( dkVec3f ) * 2 + sizeof( dkQuatf ) + sizeof( Entity ) + 2 * sizeof( dkMat4x4f ) + 4 * sizeof( Instance );

TransformDatabase::TransformDatabase( BaseAllocator* allocator )
    : ComponentDatabase( allocator )
{

}

TransformDatabase::~TransformDatabase()
{

}

void TransformDatabase::create( const size_t dbCapacity )
{
    // Do the database memory allocation.
    allocateMemoryChunk( SINGLE_ENTRY_SIZE, dbCapacity );

    // Assign each component offset from the memory chunk we have allocated (we don't want to interleave the data for
    // cache coherency).
    instanceData.Position = static_cast< dkVec3f* >( databaseBuffer.Data );
    instanceData.Rotation = reinterpret_cast< dkQuatf* >( instanceData.Position + dbCapacity );
    instanceData.Scale = reinterpret_cast< dkVec3f* >( instanceData.Rotation + dbCapacity );
    instanceData.Owner = reinterpret_cast< Entity* >( instanceData.Scale + dbCapacity );
    instanceData.Local = reinterpret_cast< dkMat4x4f* >( instanceData.Owner + dbCapacity );
    instanceData.World = instanceData.Local + dbCapacity;
    instanceData.Parent = reinterpret_cast< Instance* >( instanceData.World + dbCapacity );
    instanceData.FirstChild = instanceData.Parent + dbCapacity;
    instanceData.NextSibling = instanceData.FirstChild + dbCapacity;
    instanceData.PrevSibling = instanceData.NextSibling + dbCapacity;
}

void TransformDatabase::allocateComponent( Entity& entity )
{
    // Update buffer infos.
    Instance instance = Instance( databaseBuffer.AllocationCount );
    ++databaseBuffer.AllocationCount;
    databaseBuffer.MemoryUsed += SINGLE_ENTRY_SIZE;

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    instanceData.Position[instance.getIndex()] = dkVec3f::Zero;
    instanceData.Rotation[instance.getIndex()] = dkQuatf::Identity;
    instanceData.Scale[instance.getIndex()] = dkVec3f( 1.0f, 1.0f, 1.0f );
    instanceData.Owner[instance.getIndex()] = entity;
    instanceData.Local[instance.getIndex()] = dkMat4x4f::Identity;
    instanceData.World[instance.getIndex()] = dkMat4x4f::Identity;
    instanceData.Parent[instance.getIndex()] = Instance();
    instanceData.FirstChild[instance.getIndex()] = Instance();
    instanceData.NextSibling[instance.getIndex()] = Instance();
    instanceData.PrevSibling[instance.getIndex()] = Instance();
}

void TransformDatabase::setLocal( Instance i, const dkMat4x4f& m )
{
    instanceData.Local[i.getIndex()] = m;

    Instance parent = instanceData.Parent[i.getIndex()];
    dkMat4x4f parentModel = parent.isValid() ? instanceData.World[parent.getIndex()] : dkMat4x4f::Identity;
    applyParentMatrixRecurse( parentModel, i );
}

void TransformDatabase::applyParentMatrixRecurse( const dkMat4x4f& parent, Instance i )
{
    instanceData.World[i.getIndex()] = instanceData.Local[i.getIndex()] * parent;

    Instance child = instanceData.FirstChild[i.getIndex()];
    while ( child.isValid() ) {
        applyParentMatrixRecurse( instanceData.World[i.getIndex()], child );
        child = instanceData.NextSibling[child.getIndex()];
    }
}
