/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Vehicle.h"

#include "Entity.h"
#include "Transform.h"

#include "Maths/Quaternion.h"
#include "Maths/Helpers.h"

#include "Physics/MotorizedVehicle.h"

constexpr size_t VEHICLE_SINGLE_ENTRY_SIZE = sizeof( Entity ) + sizeof( Entity ) * MotorizedVehiclePhysics::MAX_WHEEL_COUNT + sizeof( MotorizedVehiclePhysics* );

VehicleDatabase::VehicleDatabase( BaseAllocator* allocator )
    : ComponentDatabase( allocator )
{

}

VehicleDatabase::~VehicleDatabase()
{

}

void VehicleDatabase::create( const size_t dbCapacity )
{
    // Do the database memory allocation.
    allocateMemoryChunk( VEHICLE_SINGLE_ENTRY_SIZE, dbCapacity );

    // Assign each component offset from the memory chunk we have allocated (we don't want to interleave the data for
    // cache coherency).
    instanceData.Owner = static_cast< Entity* >( databaseBuffer.Data );
    instanceData.VehicleWheelsEntity = reinterpret_cast< Entity* >( instanceData.Owner + dbCapacity );
    instanceData.VehiclePhysics = reinterpret_cast< MotorizedVehiclePhysics** >( instanceData.VehicleWheelsEntity + dbCapacity );
}

void VehicleDatabase::allocateComponent( Entity& entity )
{
    // Update buffer infos.
    Instance instance;
    if ( !freeInstances.empty() ) {
        instance = freeInstances.front();
        freeInstances.pop();
    } else {
		instance = Instance( databaseBuffer.AllocationCount );
		++databaseBuffer.AllocationCount;
		databaseBuffer.MemoryUsed += VEHICLE_SINGLE_ENTRY_SIZE;
    }

    entityToInstanceMap[entity.extractIndex()] = instance;

    // Initialize this instance components.
    const size_t instanceIndex = instance.getIndex();
    instanceData.Owner[instanceIndex] = entity;
    instanceData.VehiclePhysics[instanceIndex] = nullptr;

    for ( size_t instanceOffset = instanceIndex * MotorizedVehiclePhysics::MAX_WHEEL_COUNT; instanceOffset < ( instanceIndex + 1 ) * MotorizedVehiclePhysics::MAX_WHEEL_COUNT; instanceOffset++ ) {
        instanceData.VehicleWheelsEntity[instanceOffset] = Entity();
    }
}

void VehicleDatabase::update( const f32 deltaTime, TransformDatabase* transformDatabase )
{
    // Update vehicle logic (should be logic ONLY; physics updates should stay in the physics subsystems).
    i32 wheelIdx = 0;
    for ( size_t idx = 0; idx < databaseBuffer.AllocationCount; idx++ ) {
        i32 vehicleWheelCount = instanceData.VehiclePhysics[idx]->getWheelCount();

        // Update wheels entities.
        for ( i32 i = 0; i < vehicleWheelCount; i++ ) {
            const VehicleWheel& wheelInfos = instanceData.VehiclePhysics[idx]->getWheelByIndex( i );

            Instance wheelTransform = transformDatabase->lookup( instanceData.VehicleWheelsEntity[wheelIdx + i] );
            transformDatabase->setPosition( wheelTransform, wheelInfos.Position );

            dkQuatf wheelRotation = wheelInfos.Orientation;

            // Vehicle wheels are instantiated with the same transform, which is why we need to rotate the wheels on 
            // the other side of the car (so that the rim is located on the outside).
            if ( ( i % 2 ) != 0 ) {
                wheelRotation.rotate( dk::maths::radians( 180.0f ), dkVec3f( 0.0f, 1.0f, 0.0f ) );
            }

            transformDatabase->setRotation( wheelTransform, wheelRotation );
        }

        wheelIdx += vehicleWheelCount;
    }
}
