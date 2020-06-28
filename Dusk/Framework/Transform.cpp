/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Transform.h"

#include "Entity.h"
#include "Maths/MatrixTransformations.h"

constexpr size_t TRANSFORM_SINGLE_ENTRY_SIZE = sizeof( dkVec3f ) * 2 + sizeof( dkQuatf ) + sizeof( Entity ) + 2 * sizeof( dkMat4x4f ) + 4 * sizeof( Instance );

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
    allocateMemoryChunk( TRANSFORM_SINGLE_ENTRY_SIZE, dbCapacity );

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
    Instance instance;
    if ( !freeInstances.empty() ) {
        instance = freeInstances.front();
        freeInstances.pop();
    } else {
		instance = Instance( databaseBuffer.AllocationCount );
		++databaseBuffer.AllocationCount;
		databaseBuffer.MemoryUsed += TRANSFORM_SINGLE_ENTRY_SIZE;
    }

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    const size_t instanceIndex = instance.getIndex();
    instanceData.Position[instanceIndex] = dkVec3f::Zero;
    instanceData.Rotation[instanceIndex] = dkQuatf::Identity;
    instanceData.Scale[instanceIndex] = dkVec3f( 1.0f, 1.0f, 1.0f );
    instanceData.Owner[instanceIndex] = entity;
    instanceData.Local[instanceIndex] = dkMat4x4f::Identity;
    instanceData.World[instanceIndex] = dkMat4x4f::Identity;
    instanceData.Parent[instanceIndex] = Instance();
    instanceData.FirstChild[instanceIndex] = Instance();
    instanceData.NextSibling[instanceIndex] = Instance();
    instanceData.PrevSibling[instanceIndex] = Instance();
}

void TransformDatabase::setLocal( Instance i, const dkMat4x4f& m )
{
    instanceData.Local[i.getIndex()] = m;

    Instance parent = instanceData.Parent[i.getIndex()];
    dkMat4x4f parentModel = parent.isValid() ? instanceData.World[parent.getIndex()] : dkMat4x4f::Identity;
    applyParentMatrixRecurse( parentModel, i );
}

void TransformDatabase::update( const f32 deltaTime )
{
    // TODO: Improvements (multithread the database update; mark instance as dirty to avoid unecessary updates; etc.).
    for ( size_t idx = 0; idx < databaseBuffer.AllocationCount; idx++ ) {
		dkMat4x4f translationMatrix = dk::maths::MakeTranslationMat( instanceData.Position[idx] );
		dkMat4x4f rotationMatrix = instanceData.Rotation[idx].toMat4x4();
		dkMat4x4f scaleMatrix = dk::maths::MakeScaleMat( instanceData.Scale[idx] );

        dkMat4x4f modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        setLocal( Instance( idx ), modelMatrix );
    }
}

TransformDatabase::EdInstanceData TransformDatabase::getEditorInstanceData( const Instance instance )
{
    size_t instanceIndex = instance.getIndex();

    EdInstanceData editorData;
    editorData.Position = &instanceData.Position[instanceIndex];
    editorData.Rotation = &instanceData.Rotation[instanceIndex];
    editorData.Scale = &instanceData.Scale[instanceIndex];
    editorData.Local = &instanceData.Local[instanceIndex];
    editorData.World = &instanceData.World[instanceIndex];

    return editorData;
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
