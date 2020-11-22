/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>

#ifndef DUSK_USE_PHYSICS
#include "Physics/DynamicsWorld.h"
#include "Physics/RigidBody.h"

DynamicsWorld::DynamicsWorld( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , nativeObject( nullptr )
{

}

DynamicsWorld::~DynamicsWorld()
{

}

void DynamicsWorld::tick( const f32 deltaTime )
{

}

void DynamicsWorld::registerRigidBody( RigidBody& rigidBody )
{

}
#endif
