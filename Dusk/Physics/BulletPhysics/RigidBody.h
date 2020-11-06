/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#ifdef DUSK_USE_BULLET
class btRigidBody;
struct btDefaultMotionState;
class btCollisionShape;

struct NativeRigidBody
{
    btRigidBody*            RigidBodyInstance;
    btDefaultMotionState*   BodyMotionState;
    btCollisionShape*       CollisionShape;
    
    NativeRigidBody()
        : RigidBodyInstance( nullptr )
        , BodyMotionState( nullptr )
        , CollisionShape( nullptr )
    {

    }
};
#endif