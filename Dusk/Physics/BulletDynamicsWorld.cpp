/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "BulletDynamicsWorld.h"

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/BroadphaseCollision/btDbvtBroadphase.h"

DynamicsWorld::DynamicsWorld( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , broadphase( dk::core::allocate<btDbvtBroadphase>( allocator ) )
    , collisionConfiguration( dk::core::allocate<btDefaultCollisionConfiguration>( allocator ) )
    , dispatcher( dk::core::allocate<btCollisionDispatcher>( allocator, collisionConfiguration ) )
    , solver( dk::core::allocate<btSequentialImpulseConstraintSolver>( allocator ) )
    , dynamicsWorld( dk::core::allocate<btDiscreteDynamicsWorld>( allocator, dispatcher, broadphase, solver, collisionConfiguration ) )
{
    dynamicsWorld->setGravity( btVector3( 0.0f, -9.80665f, 0.0f ) );
}

DynamicsWorld::~DynamicsWorld()
{
    dk::core::free( memoryAllocator, broadphase );
    dk::core::free( memoryAllocator, collisionConfiguration );
    dk::core::free( memoryAllocator, dispatcher );
    dk::core::free( memoryAllocator, solver );
    dk::core::free( memoryAllocator, dynamicsWorld );
}

void DynamicsWorld::tick( const f32 deltaTime )
{
    dynamicsWorld->stepSimulation( deltaTime, 0, deltaTime );
}
