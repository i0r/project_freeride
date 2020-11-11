/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class MotorizedVehiclePhysics;
class TransformDatabase;

struct Entity;

#include <Maths/Vector.h>
#include <Maths/Quaternion.h>

#include "ComponentDatabase.h"

class VehicleDatabase : public ComponentDatabase
{
public:
            VehicleDatabase( BaseAllocator* allocator );
            ~VehicleDatabase();

    // Create an instance of this database with a given entry count 'dbCapacity'.
    void    create( const size_t dbCapacity );

    // Allocate a component for a given entity.
    void    allocateComponent( Entity& entity );

    void    update( const f32 deltaTime, TransformDatabase* transformDatabase );

private:
    struct InstanceData {
        Entity*                     Owner;
        Entity*                     VehicleWheelsEntity;
        MotorizedVehiclePhysics**   VehiclePhysics;
    };

private:
    InstanceData        instanceData;
};
