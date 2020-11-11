/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class RigidBody;
class BaseAllocator;

class DynamicsWorld 
{
public:
    struct NativeObject;

public:
                                            DynamicsWorld( BaseAllocator* allocator );
                                            DynamicsWorld( DynamicsWorld& ) = delete;
                                            ~DynamicsWorld();

    // Should be called whenever the simulation is ticked.
    void                                    tick( const f32 deltaTime );

    // Register a rigid body to the Dynamics world. Only required for dynamic entities.
    // Once the entity is removed/unstreamed, the rigid body should be unregistered from the world to avoid unnecessary
    // state updates.
    void                                    registerRigidBody( RigidBody& rigidBody );

private:
    // The memory allocator owning this instance.
    BaseAllocator*                          memoryAllocator;

    // Opaque structure holding API specific objects (defined in the body impl.).
    NativeObject*                           nativeObject;
};
