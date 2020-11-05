/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_USE_BULLET
#include "RigidBody.h"

#include "Physics/RigidBody.h"

RigidBody::RigidBody( BaseAllocator* allocator, const f32 massInKg, const dkVec3f& positionWorldSpace, const dkQuatf& orientation )
    : memoryAllocator( allocator )
    , nativeObject( dk::core::allocate<NativeRigidBody>( allocator ) )
    , bodyMassInKg( massInKg )
{
    const btQuaternion btMotionStateRotation = btQuaternion( orientation.x, orientation.y, orientation.z, orientation.w );
    const btVector3 btMotionStateTranslation = btVector3( positionWorldSpace.x, positionWorldSpace.y, positionWorldSpace.z );
    const btTransform btMotionStateTransform = btTransform( btMotionStateRotation, btMotionStateTranslation );

    nativeObject->BodyMotionState = dk::core::allocate<btDefaultMotionState>( memoryAllocator, btMotionStateTransform );
    
    btVector3 localInertia( 0, 0, 0 );

    /*if ( bodyMassInKg > 0.0f ) {
        nativeObject->CollisionShape->calculateLocalInertia( bodyMassInKg, localInertia );
    }*/

    btRigidBody::btRigidBodyConstructionInfo rigidBodyConstructionInfos( 
        bodyMassInKg, 
        nativeObject->BodyMotionState, 
        nativeObject->CollisionShape,
        localInertia 
    );

    nativeObject = dk::core::allocate<btRigidBody>( memoryAllocator, rigidBodyConstructionInfos );
    nativeObject->setActivationState( ACTIVE_TAG );
    nativeObject->setRestitution( 0.0f );
    nativeObject->setFriction( 0.0f );
    nativeObject->setRollingFriction( 0.0f );
}

RigidBody::~RigidBody()
{

}

void RigidBody::keepAlive()
{
    nativeObject->RigidBodyInstance->setActivationState( DISABLE_DEACTIVATION );
}

void RigidBody::setSimulationState( const bool enableSimulation )
{
    nativeObject->RigidBodyInstance->setActivationState( ( enableSimulation ) ? ACTIVE_TAG : ISLAND_SLEEPING );
}

dkVec3f RigidBody::getAngularVelocity() const
{
    const btVector3& angularVelocity = nativeObject->RigidBodyInstance->getAngularVelocity();
    return dkVec3f( angularVelocity.getX(), angularVelocity.getY(), angularVelocity.getZ() );
}

dkVec3f RigidBody::getLinearVelocity() const
{
    const btVector3& linearVelocity = nativeObject->RigidBodyInstance->getLinearVelocity();
    return dkVec3f( linearVelocity.getX(), linearVelocity.getY(), linearVelocity.getZ() );
}

dkVec3f RigidBody::getWorldPosition() const
{
    const btTransform& transform = nativeObject->RigidBodyInstance->getWorldTransform();
    const btVector3& worldPosition = transform.getOrigin();

    return dkVec3f( worldPosition.getX(), worldPosition.getY(), worldPosition.getZ() );
}

dkQuatf RigidBody::getRotation() const
{
    const btTransform& transform = nativeObject->RigidBodyInstance->getWorldTransform();
    const btQuaternion& orientation = transform.getRotation();

    return dkQuatf( orientation.getX(), orientation.getY(), orientation.getZ(), orientation.getW() );
}

dkVec3f RigidBody::getVelocityInLocalPoint( const dkVec3f& localPoint ) const
{
    btVector3 velocity = nativeObject->RigidBodyInstance->getVelocityInLocalPoint( btVector3( localPoint.x, localPoint.y, localPoint.z ) );
    return dkVec3f( velocity.getX(), velocity.getY(), velocity.getZ() );
}

void RigidBody::applyTorque( const dkVec3f& torque )
{
    btVector3 btTorque( torque.x, torque.y, torque.z );
    nativeObject->RigidBodyInstance->applyTorque( btTorque );
}

void RigidBody::applyForceCentral( const dkVec3f& force )
{
    btVector3 btForce( force.x, force.y, force.z );
    nativeObject->RigidBodyInstance->applyCentralForce( btForce );
}
#endif