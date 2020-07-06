/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/

class BaseAllocator;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;

class DynamicsWorld 
{
public:
                                            DynamicsWorld( BaseAllocator* allocator );
                                            DynamicsWorld( DynamicsWorld& ) = delete;
                                            ~DynamicsWorld();

    void                                    tick( const f32 deltaTime );

private:
    BaseAllocator*                          memoryAllocator;
    btBroadphaseInterface*                  broadphase;
    btDefaultCollisionConfiguration*        collisionConfiguration;
    btCollisionDispatcher*                  dispatcher;
    btSequentialImpulseConstraintSolver*    solver;
    btDiscreteDynamicsWorld*                dynamicsWorld;
};
