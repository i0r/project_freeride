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
    enum eProbeDirtyFlags {
        // The probe gbuffer needs to be recaptured (all six faces).
        PROBE_DIRTY_FLAG_NEED_RECAPTURE = 1 << 1,

        // The probe captures needs to be convoluted (both diffuse and specular).
        PROBE_DIRTY_FLAG_NEED_CONVOLUTION = 2 << 1,

        // The probe needs to be relighted (using previously captured gbuffer).
        PROBE_DIRTY_FLAG_NEED_RELIGHT = 3 << 1,
    };

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
        i32*        ArrayIndex;

        // Bits toggled if the probe has to be recaptured/relighted/convoluted/etc. (see eProbeDirtyFlags)
        u32*        DirtyFlags;
    };

private:
    InstanceData            instanceData;
};
