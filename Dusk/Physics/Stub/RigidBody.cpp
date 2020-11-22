/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>

#ifndef DUSK_USE_PHYSICS
#include "Physics/RigidBody.h"

RigidBody::RigidBody( BaseAllocator* allocator, const f32 massInKg )
    : memoryAllocator( allocator )
    , nativeObject( nullptr )
    , bodyMassInKg( massInKg )
{

}

RigidBody::~RigidBody()
{

}

void RigidBody::createWithBoxCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const dkVec3f& boxHalfExtents )
{

}

void RigidBody::createWithSphereCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32 sphereRadius )
{

}

void RigidBody::createWithPlaneCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const dkVec3f& planeNormal, const f32 planeHeight )
{

}

void RigidBody::createWithCylinderCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32 cylinderRadius, const f32 cylinderDepth )
{

}

void RigidBody::createWithConvexHullCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32* hullVertices, const i32 vertexCount )
{

}

void RigidBody::keepAlive()
{

}

void RigidBody::setSimulationState( const bool enableSimulation )
{

}

dkVec3f RigidBody::getAngularVelocity() const
{
    return dkVec3f::Zero;
}

dkVec3f RigidBody::getLinearVelocity() const
{
    return dkVec3f::Zero;
}

dkVec3f RigidBody::getWorldPosition() const
{
    return dkVec3f::Zero;
}

dkQuatf RigidBody::getRotation() const
{
    return dkQuatf::Identity;
}

dkVec3f RigidBody::getVelocityInLocalPoint( const dkVec3f& localPoint ) const
{
    return dkVec3f::Zero;
}

void RigidBody::applyTorque( const dkVec3f& torque )
{

}

void RigidBody::applyForceCentral( const dkVec3f& force )
{

}
#endif
