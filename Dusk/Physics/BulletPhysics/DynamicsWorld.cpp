/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_USE_BULLET
#include "Physics/DynamicsWorld.h"
#include "Physics/RigidBody.h"

#include "RigidBody.h"

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/BroadphaseCollision/btDbvtBroadphase.h"

struct DynamicsWorld::NativeObject 
{
    btBroadphaseInterface*                  broadphase;
    btDefaultCollisionConfiguration*        collisionConfiguration;
    btCollisionDispatcher*                  dispatcher;
    btSequentialImpulseConstraintSolver*    solver;
    btDiscreteDynamicsWorld*                dynamicsWorld;
};

DynamicsWorld::DynamicsWorld( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , nativeObject( dk::core::allocate<DynamicsWorld::NativeObject>( allocator ) )
{
    nativeObject->broadphase = dk::core::allocate<btDbvtBroadphase>( allocator );
    nativeObject->collisionConfiguration = dk::core::allocate<btDefaultCollisionConfiguration>( allocator );
    nativeObject->dispatcher = dk::core::allocate<btCollisionDispatcher>( allocator, nativeObject->collisionConfiguration );
    nativeObject->solver = dk::core::allocate<btSequentialImpulseConstraintSolver>( allocator );
    nativeObject->dynamicsWorld = dk::core::allocate<btDiscreteDynamicsWorld>( allocator, 
                                                                               nativeObject->dispatcher, 
                                                                               nativeObject->broadphase, 
                                                                               nativeObject->solver, 
                                                                               nativeObject->collisionConfiguration );

    // Set default world gravity.
    nativeObject->dynamicsWorld->setGravity( btVector3( 0.0f, -9.80665f, 0.0f ) );
}

DynamicsWorld::~DynamicsWorld()
{
    dk::core::free( memoryAllocator, nativeObject->broadphase );
    dk::core::free( memoryAllocator, nativeObject->collisionConfiguration );
    dk::core::free( memoryAllocator, nativeObject->dispatcher );
    dk::core::free( memoryAllocator, nativeObject->solver );
    dk::core::free( memoryAllocator, nativeObject->dynamicsWorld );
}

void DynamicsWorld::tick( const f32 deltaTime )
{
    nativeObject->dynamicsWorld->stepSimulation( deltaTime, 0, deltaTime );
}

void DynamicsWorld::registerRigidBody( RigidBody& rigidBody )
{
    nativeObject->dynamicsWorld->addRigidBody( rigidBody.getNativeObject()->RigidBodyInstance );
}
#endif
