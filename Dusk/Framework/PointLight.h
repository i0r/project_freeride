/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class Model;
struct Entity;

#include "ComponentDatabase.h"
#include "Graphics/ShaderHeaders/Light.h"

class PointLightDatabase : public ComponentDatabase
{
public:
    // Return a reference to the light data for a given component instance.
	DUSK_INLINE PointLightGPU& getLightData( const Instance instance ) const { return instanceData.PointLight[instance.getIndex()]; }

public:
            PointLightDatabase( BaseAllocator* allocator );
            ~PointLightDatabase();

    // Create an instance of this database with a given entry count 'dbCapacity'.
    void    create( const size_t dbCapacity );

    // Allocate a component for a given entity.
    void    allocateComponent( Entity& entity );

private:
    struct InstanceData {
        PointLightGPU*  PointLight;
        Entity*         Owner;
    };

private:
    InstanceData        instanceData;
};
