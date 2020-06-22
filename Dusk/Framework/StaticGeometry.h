/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class Model;
struct Entity;

#include "ComponentDatabase.h"

class StaticGeometryDatabase : public ComponentDatabase
{
public:
	DUSK_INLINE const Model* getModel( const Instance instance ) const { return instanceData.ModelResource[instance.getIndex()]; }
    DUSK_INLINE void setModel( const Instance instance, Model* model ) { instanceData.ModelResource[instance.getIndex()] = model; }

public:
            StaticGeometryDatabase( BaseAllocator* allocator );
            ~StaticGeometryDatabase();

    // Create an instance of this database with a given entry count 'dbCapacity'.
    void    create( const size_t dbCapacity );

    // Allocate a component for a given entity.
    void    allocateComponent( Entity& entity );

private:
    struct InstanceData {
        Model**         ModelResource;
        Entity*         Owner;
    };

private:
    InstanceData        instanceData;
};
