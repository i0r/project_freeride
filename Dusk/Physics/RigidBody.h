/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "Maths/Vector.h"
#include "Maths/Quaternion.h"

class BaseAllocator;
struct NativeRigidBody;

class RigidBody 
{
public:
    // Return a reference to the opaque structure of this body. Shouldn't be used outside API implementation (for regular
    // physics operations you should use the API defined below).
    DUSK_INLINE NativeRigidBody* getNativeObject() const { return nativeObject; }

public:
                    RigidBody( BaseAllocator* allocator, const f32 massInKg );
                    ~RigidBody();

    // Create and instantiate this rigid body with a box collider.
    void            createWithBoxCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const dkVec3f& boxHalfExtents );

    // Create and instantiate this rigid body with a sphere collider.
    void            createWithSphereCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32 sphereRadius );

    // Create and instantiate this rigid body with a plane collider. Note that the plane is implicitly infinite,
    void            createWithPlaneCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const dkVec3f& planeNormal, const f32 planeHeight );

    // Create and instantiate this rigid body with a cylinder collider.
    void            createWithCylinderCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32 cylinderRadius, const f32 cylinderDepth );

    // Create and instantiate this rigid body with a convex hull collider. The vertices provided as the scalar array 'hullVertices'
    // must contain position attributes only.
    void            createWithConvexHullCollider( const dkVec3f& positionWorldSpace, const dkQuatf& orientation, const f32* hullVertices, const i32 vertexCount );

    // Disable simulation deactivation for this body. Shouldn't be used anywhere outside vehicle simulation.
    // To re-enable deactivation, you should simply call 'setSimulationState'.
    void            keepAlive();

    // Set simulation state for this body. If enableSimulation is true, this rigid body will be included in the simulation
    // until it is inactive (and will implicitly be skipped until it is reactivated by the simulation or the user). 
    // Otherwise if enableSimulation is false the simulation will be explicitly skipped and the body
    // won't wake up.
    void            setSimulationState( const bool enableSimulation );

    // Return the angular velocity of this rigid body.
    dkVec3f         getAngularVelocity() const;

    // Return the linear velocity of this rigid body.
    dkVec3f         getLinearVelocity() const;
    
    // Return the position of this rigid body (in the dynamics world).
    dkVec3f         getWorldPosition() const;

    // Return the orientation of this rigid body (as a quaternion).
    dkQuatf         getRotation() const;

    // Return velocity for a given relative local point.
    dkVec3f         getVelocityInLocalPoint( const dkVec3f& localPoint ) const;

    // Apply torque to this rigid body.
    void            applyTorque( const dkVec3f& torque );

    // Apply a force at the center of mass of this rigid body.
    void            applyForceCentral( const dkVec3f& force );

private:
    // The allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // Opaque object holding API-specific handle/objects (defined in the body implementation).
    NativeRigidBody*    nativeObject;

    // Mass of the object (in kilograms).
    f32                 bodyMassInKg;
};
