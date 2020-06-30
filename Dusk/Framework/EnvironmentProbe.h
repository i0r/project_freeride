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

class EnvironmentProbeDatabase : public ComponentDatabase
{
public:
            EnvironmentProbeDatabase( BaseAllocator* allocator );
            ~EnvironmentProbeDatabase();

    // Create an instance of this database with a given entry count 'dbCapacity'.
    void    create( const size_t dbCapacity );

    // Allocate a component for a given entity.
    void    allocateComponent( Entity& entity );

    void    update( const f32 deltaTime );

private:
    struct InstanceData {
        // Probe Bounding Sphere radius.
        f32*        Radius;

        // Probe OOBB inverse matrix (required for parallax correction).
        dkMat4x4f*  InverseModelMatrix;

        // Index in the streaming probe array (-1 if the probe is not streamed).
        i32*        ProbeArrayIndex;
    };

private:
    InstanceData            instanceData;
};
