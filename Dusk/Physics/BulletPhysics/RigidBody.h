/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#ifdef DUSK_USE_BULLET
class btRigidBody;
struct btDefaultMotionState;
class btCollisionObject;

struct NativeRigidBody
{
    btRigidBody*            RigidBodyInstance;
    btDefaultMotionState*   BodyMotionState;
    btCollisionObject*      CollisionShape;

    NativeRigidBody()
        : RigidBodyInstance( nullptr )
        , BodyMotionState( nullptr )
        , CollisionShape( nullptr )
    {

    }
};
#endif