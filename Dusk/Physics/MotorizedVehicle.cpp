/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "MotorizedVehicle.h"

#include "RigidBody.h"

#if DUSK_USE_BULLET
#include "ThirdParty/bullet3/src/btBulletDynamicsCommon.h"
#endif

static constexpr dkVec3f WHEEL_POSITION_OFFSET[MotorizedVehiclePhysics::MAX_WHEEL_COUNT] = {
    dkVec3f( -0.5f, 1.0f, -0.5f ), // Front Left
    dkVec3f( +0.5f, 1.0f, -0.5f ), // Front Right
    dkVec3f( -0.5f, 1.0f, +0.5f ), // Rear  Left
    dkVec3f( +0.5f, 1.0f, +0.5f ), // Rear  Right
    dkVec3f( -0.5f, 1.0f, +0.5f ), // Rear  Left    (Custom)
    dkVec3f( +0.5f, 1.0f, +0.5f ), // Rear  Right   (Custom)
};

MotorizedVehiclePhysics::MotorizedVehiclePhysics( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , velocity( 0, 0, 0 )
    , forceAccumulator( 0, 0, 0 )
    , torqueAccumulator( 0, 0, 0 )
    , steer( 0.0f )
    , position( 0, 0, 0 )
    , engineSpeed( 0.0f )
    , engineTorque( 0.0f )
    , currentGearIndex( 0 )
    , inputSteering( 0.0f )
    , inputBraking( 0.0f )
    , inputThrottle( 0.0f )
    , torqueCurve{ 1.0f, 0.2f }
    , maxTorque( 4000.0f )
    , drag( 200.0f )
    , compressionDrag( 200.0f )
    , maxAngularVelocity( 120.0f )
    , limiter( 200.0f )
    , limiterTime( 0.1f )
    , currentLimitation( 0.0f )
{

}

void MotorizedVehiclePhysics::create( const VehicleParameters& parameters, const dkVec3f& positionWorldSpace, const dkQuatf& orientationWorldSpace )
{
    vehicleParameters = parameters;

    chassisRigidBody = dk::core::allocate<RigidBody>( memoryAllocator, memoryAllocator, vehicleParameters.TotalMass );
    chassisRigidBody->createWithBoxCollider( positionWorldSpace, orientationWorldSpace, dkVec3f( 8.0f, 0.25, 4.0f ) );
    chassisRigidBody->keepAlive();

    //// Build chassis collider 
    //auto convexHull = new CollisionShapeConvexHull( hullData->Vertices, hullData->VertexCount );
    //auto collisionShape = new CollisionShapeCompound();
    //collisionShape->AddChildShape( convexHull, parameters.CenterOfMassPosition );

    // Since this is a raycast-based vehicle, wheels don't exist in the physics world
    for ( u32 wheelIdx = 0; wheelIdx < vehicleParameters.TiresParameters.WheelCount; wheelIdx++ ) {
        VehicleWheel& wheel = wheels[wheelIdx];

        wheel.BasePosition = dkVec3f( vehicleParameters.WheelDistanceLateral, 0.0f, vehicleParameters.WheelDistanceLongitudinal ) * WHEEL_POSITION_OFFSET[wheelIdx];
        wheel.BaseOrientation = dkQuatf( 0, 0, 0, 1 );
        wheel.OrientationMatrix = wheel.BaseOrientation.toMat3x3();

        wheel.SuspensionRestLength = 0.2f;
        wheel.SuspensionForce = 15000.0f;
        wheel.SuspensionDampingFactor = 5000.0f;
        wheel.SuspensionExponent = 1.5f;

        wheel.Position = positionWorldSpace + wheel.BasePosition;
        wheel.Orientation = wheel.BaseOrientation;
        wheel.CurrentSuspensionLength = wheel.SuspensionRestLength;

        wheel.MaxBrakeTorque = 2000.0f;
        wheel.DragCoefficient = 0.4f;
        wheel.MomentOfInertia = 8.0f;
        wheel.FricitionCoefficient = 1.0f;
        wheel.LostGripSlidingFactor = 0.8f;
        wheel.WheelTiltEdgeFactor = 0.2f;
        wheel.SuspensionRestLength = 0.2f;
        wheel.SteerRate = 1.5f;
        wheel.SteerLock = 0.0f;

        wheel.PatchMass = 500.0f;
        wheel.PatchRadius = 0.1f;
        wheel.PatchSlipingFactor = 0.1f;

        wheel.PatchTensionTorque = 0.0f;
        wheel.FinalTorque = 0.0f;
        wheel.AngularVelocity = 0.0f;
        wheel.AxialAngle = 0.0f;
        wheel.GripState = 0.0f;
        wheel.SteerAngle = 0.0f;

        if ( wheelIdx <= 1 ) {
            wheel.SteerLock = 0.6f;
        } else {
            wheel.FricitionCoefficient = 1.0f;
            wheel.SuspensionForce = 9001.0f;
            wheel.SuspensionDampingFactor = 3000.0f;
            wheel.SuspensionExponent = 1.0f;
            wheel.SteerRate = 1.8f;
        }

        wheel.PatchSpringRate = wheel.PatchMass * 60.0f;
        wheel.PatchCriticalDampingCoefficient = pow( 4.0f * wheel.PatchMass * wheel.PatchSpringRate, 0.5f );
        wheel.PatchAveragePosition = dkVec3f( 0.0f );
        wheel.PatchAverageVelocity = dkVec3f( 0.0f );
    }
}

void MotorizedVehiclePhysics::preStepUpdate( const f32 frameTime, btDynamicsWorld* dynamicsWorld )
{
    // Update chassis simulation
    dkVec3f angularVelocity = chassisRigidBody->getAngularVelocity();
    dkVec3f linearVelocity = chassisRigidBody->getLinearVelocity();

    f32 inputSteer = inputSteering / ( 1 + ( ( pow( Max( abs( linearVelocity.z ) - vehicleParameters.DecayThreshold, 0.0f ), 0.9f ) ) * vehicleParameters.DecayControl ) );

    f32 slideSteer = 0.0f;
    dkVec2f slideVector = dkVec2f( linearVelocity.x, linearVelocity.z );
    f32 slideVelocity = slideVector.length();

    if ( slideVector.y > 0.0f ) {
        slideSteer = dk::maths::angle( slideVector, dkVec2f( 0, 1 ) ) * ( static_cast< f32 >( slideVector.x > 0.0f ) - 0.5f ) * 2.0f;
    } else if ( slideVector.y < 0.0f ) {
        slideSteer = dk::maths::angle( slideVector, dkVec2f( 0, -1 ) ) * ( static_cast< f32 >( slideVector.x > 0.0f ) - 0.5f ) * 2.0f;
    }

    slideSteer = slideSteer * ( ( 1 - ( 1 / ( 1 + slideVelocity ) ) ) * vehicleParameters.SlideControl );
    slideSteer *= static_cast<f32>( abs( slideVector.y ) > 6.0f );

    f32 spinSteer = angularVelocity.y * vehicleParameters.SlideControl;
    steer = dk::maths::clamp( inputSteer + slideSteer + spinSteer, -1.0f, 1.0f );

    dkVec3f bpos = chassisRigidBody->getWorldPosition();
    dkMat3x3f bmat = chassisRigidBody->getRotation().toMat3x3();

    auto Towards = [&]( f32 current, f32 target, f32 amount ) {
        if ( current > target ) {
            return current - Min( abs( target - current ), amount );
        } else if ( current < target ) {
            return current + Min( abs( target - current ), amount );
        }

        return current;
    };

    auto VecTrack = [&]( const dkVec3f& v0, const dkVec3f& v1, int lockAxis = 1, int trackAxis = 2 ) {
        if ( lockAxis == trackAxis ) {
            return dkMat3x3f::Identity;
        }

        auto lock = v0.normalize();
        auto track = v1.normalize();

        if ( abs( dkVec3f::dot( lock, track ) ) == 1.0f ) {
            return dkMat3x3f::Identity;
        }

        auto other = dkVec3f::cross( track, lock ).normalize();
        track = dkVec3f::cross( lock, other );

        auto otherAxis = 3 - ( lockAxis + trackAxis );

        if ( !( ( lockAxis - 1 ) % 3 == trackAxis ) ) {
            other = -other;
        }

        dkMat3x3f r = dkMat3x3f( 0.0f );
        r[lockAxis] = lock;
        r[trackAxis] = track;
        r[otherAxis] = other;

        return r;
    };

    auto veclineproject = [&]( const dkVec3f& vec, const dkVec3f& nor ) {
        dkVec3f n = nor.normalize();
        return nor * dkVec3f::dot( nor, vec );
    };

    auto vecplaneproject = [&]( const dkVec3f& vec, const dkVec3f& nor ) {
        dkVec3f n = nor.normalize();
        return vec - veclineproject( vec, n );
    };

    auto vecToLine = [&]( dkVec3f& vec, dkVec3f& nor ) {
        return veclineproject( vec, nor ) - vec;
    };

    auto limit = [&]( f32 value, f32 lower, f32 upper ) {
        return Max( Min( value, upper ), lower );
    };

    auto lerp = [&]( f32 p0, f32 p1, f32 t ) {
        return p0 + ( p1 - p0 ) * limit( t, 0.0, 1.0 );
    };

    auto SafeNormalize = [&]( dkVec3f& vectorToNormalize ) {
        // For some reason, glm allows null vector normalization (which leads to a division by zero)
        // So we need to check if the vector length is null to avoid evilish NaN
        vectorToNormalize = ( vectorToNormalize.length() == 0.0f ) ? dkVec3f::Zero : vectorToNormalize.normalize();
        return vectorToNormalize;
    };

    //engineTorque = torqueCurve.GetTorque( glm::max( engineSoeed, vehicleParameters.EngineParameters.IdleSpeed ) );

#if DUSK_USE_BULLET
    for ( auto& wheel : wheels ) {
        // Do steering
        wheel.SteerAngle = Towards( wheel.SteerAngle, wheel.SteerLock * steer, wheel.SteerRate * frameTime );

        // Generate wheel matrix and apply steering
        auto wmat = bmat * wheel.OrientationMatrix * dkQuatf( 0, 0, 0, 1 ).rotate( wheel.SteerAngle, dkVec3f( 0, 1, 0 ) ).toMat3x3();

        // Ray coordinates
        auto p0 = wheel.Position;
        auto depth = ( bmat * wheel.OrientationMatrix )[1] * ( wheel.SuspensionRestLength + vehicleParameters.TiresParameters.WheelRadius );
        auto p1 = p0 - depth;

        // TODO This is Bullet3 specific; should be abstracted.
        btVector3 btP0 = btVector3( p0.x, p0.y, p0.z );
        btVector3 btP1 = btVector3( p1.x, p1.y, p1.z );

        btCollisionWorld::ClosestRayResultCallback rayResult( btP0, btP1 );
        dynamicsWorld->rayTest( btP0, btP1, rayResult );

        if ( rayResult.hasHit() ) {
            // Retrieve hit infos and compute common terms for the rest of the update
            auto hob = rayResult.m_collisionObject;
            auto hitNormal = dkVec3f( rayResult.m_hitNormalWorld.getX(), rayResult.m_hitNormalWorld.getY(), rayResult.m_hitNormalWorld.getZ() ).normalize();
            auto hmat = VecTrack( hitNormal, wmat[2] );

            auto hpos = dkVec3f( rayResult.m_hitPointWorld.getX(), rayResult.m_hitPointWorld.getY(), rayResult.m_hitPointWorld.getZ() );

            f32 flatness = dkVec3f::dot( hmat[1], wmat[1] );

            auto hitPositionBody = hpos - bpos;

            auto btHitObjectWorldPosition = hob->getWorldTransform().getOrigin();
            auto hitObjectWorldPosition = dkVec3f( btHitObjectWorldPosition.getX(), btHitObjectWorldPosition.getY(), btHitObjectWorldPosition.getZ() );
            auto hitOriginBody = hpos - hitObjectWorldPosition;

            btVector3 hpos_hob = btVector3( hitOriginBody.x, hitOriginBody.y, hitOriginBody.z );

            auto rigidBodyHitObject = btRigidBody::upcast( hob );

            if ( rigidBodyHitObject && !rigidBodyHitObject->hasContactResponse() ) {
                continue;
            }

            btVector3 hitObjVelocity = rigidBodyHitObject->getVelocityInLocalPoint( hpos_hob );
            dkVec3f hvel = chassisRigidBody->getVelocityInLocalPoint( hitPositionBody ) - dkVec3f( hitObjVelocity.getX(), hitObjVelocity.getY(), hitObjVelocity.getZ() );
            
            // Update suspension length based on raycast's result with the rest of the world
            wheel.CurrentSuspensionLength = Max( ( p0 - hpos ).length() - vehicleParameters.TiresParameters.WheelRadius, 0.0f );

            f32 suspensionFac = 1.0f - wheel.CurrentSuspensionLength / wheel.SuspensionRestLength;
            f32 suspensionSpringForce = pow( suspensionFac, wheel.SuspensionExponent ) * wheel.SuspensionForce;
            suspensionSpringForce -= dkVec3f::dot( hvel, hmat[1] ) * wheel.SuspensionDampingFactor;

            // Update per-wheel traction
            auto tp_pos = wheel.PatchAveragePosition;
            tp_pos -= hvel * frameTime;
            f32 rollDistance = wheel.AngularVelocity * vehicleParameters.TiresParameters.WheelRadius * frameTime;
            f32 rollFactor = abs( rollDistance ) / ( wheel.PatchRadius * 2.0f );
            tp_pos += hmat[2] * rollDistance;

            tp_pos = vecplaneproject( tp_pos, hmat[1] );
            tp_pos += vecToLine( tp_pos, hmat[2] ) * ( 1.0f - 1.0f / ( 1.0f + rollFactor ) ) * wheel.PatchSlipingFactor;

            auto fNormal = Max( suspensionSpringForce, 0.0f );
            auto fTraction = wheel.FricitionCoefficient * fNormal * lerp( wheel.WheelTiltEdgeFactor, 1.0f, flatness );

            auto tp_max = fTraction / wheel.PatchSpringRate;
            tp_pos = SafeNormalize( tp_pos ) * ( Min( tp_max, tp_pos.length() ) );

            wheel.PatchAveragePosition = SafeNormalize( wheel.PatchAveragePosition ) * Min( tp_max, wheel.PatchAveragePosition.length() );

            wheel.PatchAverageVelocity = ( tp_pos - wheel.PatchAveragePosition ) / frameTime;
            wheel.PatchAveragePosition = tp_pos;

            auto fPatch = wheel.PatchAveragePosition * wheel.PatchSpringRate + wheel.PatchAverageVelocity * wheel.PatchCriticalDampingCoefficient / ( 1.0f + rollFactor );

            if ( fPatch.length() > fTraction ) {
                fPatch = fPatch.normalize() * ( fTraction * wheel.LostGripSlidingFactor );
            }

            if ( fTraction != 0.0f ) {
                wheel.GripState = limit( 1.0f - ( wheel.PatchAveragePosition.length() * wheel.PatchSpringRate ) / fTraction, 0.0f, 1.0f );
            } else {
                wheel.GripState = 0.0f;
            }

            wheel.PatchTensionTorque = -( dkVec3f::dot( fPatch, hmat[2] ) * vehicleParameters.TiresParameters.WheelRadius );

            auto force = hmat[1] * fNormal + fPatch;

            forceAccumulator += force;
            torqueAccumulator += dkVec3f::cross( hitPositionBody, force );
        } else {
            wheel.CurrentSuspensionLength = wheel.SuspensionRestLength;
            wheel.PatchAveragePosition = { 0, 0, 0 };
            wheel.PatchAverageVelocity = { 0, 0, 0 };
            wheel.PatchTensionTorque = 0.0f;
        }
    }

    // Front Axle
    for ( auto& wheel : wheels ) {
        auto torque = wheel.PatchTensionTorque;
        torque -= wheel.AngularVelocity * wheel.DragCoefficient;

        torque += getTorque( wheel.AngularVelocity ) / vehicleParameters.TiresParameters.WheelCount;
        torque = Towards( torque, -( wheel.AngularVelocity * wheel.MomentOfInertia ) / frameTime, wheel.MaxBrakeTorque * inputBraking );

        wheel.FinalTorque = torque;
    }

    // Rear Axle
    f32 torque = 0.0f;
    f32 patchDragSum = 0.0f;
    for ( auto& wheel : wheels ) {
        torque += wheel.PatchTensionTorque;
        patchDragSum += ( wheel.AngularVelocity * wheel.DragCoefficient );
    }

    torque -= patchDragSum;

    if ( vehicleParameters.TransmissionParameters.TransmissionType == VehicleTransmissionParameters::TRANSMISSION_RWD ) {
        torque += getTorque( wheels[2].AngularVelocity );
    } else if ( vehicleParameters.TransmissionParameters.TransmissionType == VehicleTransmissionParameters::TRANSMISSION_FWD ) {
        torque += getTorque( wheels[0].AngularVelocity );
    } else if ( vehicleParameters.TransmissionParameters.TransmissionType == VehicleTransmissionParameters::TRANSMISSION_AWD ) {
        torque += getTorque( wheels[0].AngularVelocity );
    }

    f32 brake = 0.0f;
    f32 momentum = 0.0f;
    f32 momenSum = 0.0f;
    for ( auto& wheel : wheels ) {
        brake += wheel.MaxBrakeTorque * inputBraking;
        momentum += wheel.AngularVelocity * wheel.MomentOfInertia;
        momenSum += wheel.MomentOfInertia;
    }

    momentum = -momentum;
    torque = Towards( torque, momentum / frameTime, brake );

    for ( auto& wheel : wheels ) {
        wheel.FinalTorque = torque * ( wheel.MomentOfInertia / momenSum );
    }
#endif

 /*   f32 liftForce = 0.0f;
    f32 weight = vehicleParameters.TotalMass - 9.80665f;
    f32 verticalForce = weight - liftForce;
*/
    //auto normalizedVelocity = ( glm::length( velocity ) == 0.0f ) ? velocity : glm::normalize( velocity );
    //forceAccumulator += ( dkVec3f( 0, -1, 0 ) * 9.80665f * vehicleParameters.TotalMass );

    //// Rolling Resistance
    //auto rollingResistanceForce = -vehicleParameters.TiresParameters.RollingResistanceCoefficient * verticalForce * normalizedVelocity;
    //forceAccumulator += rollingResistanceForce;

    //// Drag force
    //f32 speed = glm::length( velocity );
    //auto dragForce = -( 0.5f * vehicleParameters.DragCoefficient * vehicleParameters.FrontalArea * speed * 1.225f ) * velocity;
    //forceAccumulator += dragForce;
}

void MotorizedVehiclePhysics::postStepUpdate( const f32 frameTime )
{
    velocity += ( forceAccumulator / vehicleParameters.TotalMass ) * frameTime;
    position += velocity * frameTime;

    // Physics Steppin'
    // Apply wheel forces to hull
    chassisRigidBody->applyForceCentral( forceAccumulator );
    chassisRigidBody->applyTorque( torqueAccumulator );

    auto bpos = chassisRigidBody->getWorldPosition();
    auto bmat = chassisRigidBody->getRotation().toMat3x3();

    f32 currentGearRatio = vehicleParameters.TransmissionParameters.GearRatio.at( currentGearIndex );
    f32 G = currentGearRatio * vehicleParameters.TransmissionParameters.FinalDriveRatio;

    for ( auto& wheel : wheels ) {
        wheel.AngularVelocity += ( wheel.FinalTorque / wheel.MomentOfInertia )/* / G*/ * frameTime;
        
        wheel.AxialAngle += wheel.AngularVelocity * frameTime;

        // Update wheel position according to suspension length
        dkVec3f updatedWheelPosition = wheel.BasePosition - wheel.OrientationMatrix[1] * wheel.CurrentSuspensionLength;
        updatedWheelPosition = updatedWheelPosition * bmat + bpos;

        // Update wheel rotation
        wheel.AxialAngle = std::fmod( wheel.AxialAngle, 2.0f * dk::maths::PI<f32>() );
        auto wheelSteer = dkQuatf( 0, 0, 0, 1 ).rotate( wheel.SteerAngle, dkVec3f( 0, 1, 0 ) ).toMat3x3();
        auto wheelSpin = dkQuatf( 0, 0, 0, 1 ).rotate( -wheel.AxialAngle, dkVec3f( 1, 0, 0 ) ).toMat3x3();

        auto updatedWheelOrientation = wheel.OrientationMatrix * wheelSteer * wheelSpin;

        // Update wheel transform
        wheel.Position = updatedWheelPosition;
        wheel.Orientation = dkQuatf( bmat * updatedWheelOrientation );

        //wheel.SpeedInKmh = wheel.AngularVelocity * vehicleParameters.TiresParameters.WheelRadius * -3.60f;

        //f32 mph = wheel.SpeedInKmh * 0.62137119f;
    }

    // Reset accumulators
    forceAccumulator.x = forceAccumulator.y = forceAccumulator.z = 0.0f;
    torqueAccumulator.x = torqueAccumulator.y = torqueAccumulator.z = 0.0f;
/*
    inputSteering = 0.0f;
    inputThrottle = 0.0f;
    inputBraking = 0.0f;*/
}

void MotorizedVehiclePhysics::setSteeringState( const f32 steeringState )
{
    inputSteering = steeringState;
}

void MotorizedVehiclePhysics::setThrottleState( const f32 throttleState )
{
    inputThrottle = throttleState;
}

void MotorizedVehiclePhysics::setBrakingState( const f32 brakeValue )
{
    inputBraking = brakeValue;
}

f32 MotorizedVehiclePhysics::getTorque( const f32 angularVelocity )
{
    if ( abs( angularVelocity ) > limiter ) {
        currentLimitation = limiterTime;
    }

    f32 motorSpeed = angularVelocity / maxAngularVelocity;

    auto lerp1 = [&]( const f32 p0, const f32 p1, const f32 t ) { return p0 + ( p1 - p0 ) * t; };
    auto multilerp = [&]( const std::vector<f32>& torqueCurve, const f32 time ) {
        if ( torqueCurve.size() == 0 ) {
            return 0.0f;
        } else if ( torqueCurve.size() == 1 ) {
            return torqueCurve.at( 0 );
        }

        f32 t = dk::maths::clamp( abs( time ), 0.0f, 1.0f );
        if ( t == 1.0f ) {
            return torqueCurve.back();
        } else if ( t == 0.0f ) {
            return torqueCurve.front();
        }

        t *= ( torqueCurve.size() - 1 );
        int i0 = static_cast< i32 >( t );
        int i1 = i0 + 1;

        return lerp1( torqueCurve[i0], torqueCurve[i1], t - i0 );
    };

    f32 torque = 0.0f;
    if ( currentLimitation <= 0.0f ) {
        torque = multilerp( torqueCurve, abs( motorSpeed ) ) * maxTorque * -inputThrottle;
        torque -= drag * motorSpeed;
        torque -= compressionDrag * motorSpeed * ( 1.0f - abs( -inputThrottle ) );
    }

    return torque;
}
