/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once 

#include <vector>

#include "Maths/Helpers.h"
#include "Maths/Matrix.h"
#include "Maths/Quaternion.h"

struct ConvexHullLoadData;
class RigidBody;

struct VehicleEngineParameters {
    f32       IdleRPM;    // rpm
    f32       RedLineRPM; // rpm
    f32       PeakRPM; // rpm
    f32       IdleMaxTorque;  // newton/meters
    f32       PeakMaxTorque;  // newton/meters
    f32       PeakPower;  // kw
    f32       IdleSpeed;
    f32       RedLineSpeed;
};

struct VehicleTransmissionParameters {
    u32               GearCount; // Does not include reverse gear
    f32               ReverseGearRatio;
    f32               FinalDriveRatio;
    f32               TransmissionEfficiency;
    f32               DriveTrainInertia;
    std::vector<f32>  GearRatio;

    enum {
        GEARBOX_UNKNOWN = 0,
        GEARBOX_MANUAL = 1,
        GEARBOX_SEQUENTIAL,
        GEARBOX_AUTOMATIC
    } GearboxType;

    enum {
        TRANSMISSION_UNKNOWN = 0,
        TRANSMISSION_RWD = 1,
        TRANSMISSION_FWD,
        TRANSMISSION_AWD,
    } TransmissionType;
};

struct VehicleTiresParameters {
    f32               WheelRadius; // m
    f32               FrictionCoefficient;
    f32               RollingResistanceCoefficient;
    f32               BrakingTorque;
    f32               MomentOfInertia;
    f32               LostGripSlidingFactor;
    f32               WheelTiltEdgeFactor;
    f32               SteerRate;
    f32               SteerLock;
    u32               WheelCount;
};

struct VehicleParameters {
    f32       TotalMass;                  // kg
    f32       WheelDistanceLongitudinal;  // m
    f32       WheelDistanceLateral;       // m
    f32       DragCoefficient;
    dkVec3f   CenterOfMassPosition;       // m
    dkVec3f   ChassisDimension;           // m
    f32       FrontalArea;                // Chassis Height * Chassis Length

    VehicleEngineParameters         EngineParameters;
    VehicleTransmissionParameters   TransmissionParameters;
    VehicleTiresParameters          TiresParameters;

    // Driving Aids
    bool            IsABSAvailable;
    bool            IsESPAvailable;

    f32           DecayControl;   // Lessen steering as speed increases (0 = no effect)
    f32           DecayThreshold; // Start to lessen steering at this speed (m/s)
    f32           SlideControl; // Steer against sideways sliding
};

struct VehicleWheel 
{
    f32   SpeedInKmh;

    f32   MaxBrakeTorque;
    f32   DragCoefficient;
    f32   MomentOfInertia;

    f32   FricitionCoefficient;

    f32   LostGripSlidingFactor;
    f32   WheelTiltEdgeFactor;

    f32   SteerRate;
    f32   SteerLock;

    f32   SuspensionRestLength;
    f32   SuspensionForce;
    f32   SuspensionDampingFactor;
    f32   SuspensionExponent;

    f32   PatchMass;
    f32   PatchRadius;
    f32   PatchSlipingFactor;

    f32   PatchTensionTorque;
    f32   FinalTorque;
    f32   AngularVelocity;
    f32   AxialAngle;
    f32   GripState;
    f32   SteerAngle;
    f32   CurrentSuspensionLength;

    f32       PatchSpringRate;
    f32       PatchCriticalDampingCoefficient;
    dkVec3f   PatchAveragePosition;
    dkVec3f   PatchAverageVelocity;

    dkMat3x3f OrientationMatrix;

    dkVec3f   HeightVelocity;

    dkQuatf   BaseOrientation;
    dkVec3f   BasePosition;

    dkQuatf   Orientation;
    dkVec3f   Position;
};

class MotorizedVehiclePhysics
{
public:
    RigidBody*          getChassisRigidBody() const { return chassisRigidBody; }
    i32                 getWheelCount() const { return vehicleParameters.TiresParameters.WheelCount; }
    const VehicleWheel& getWheelByIndex( const i32 wheelIndex ) const { return wheels[wheelIndex]; }

public:
    static constexpr i32 MAX_WHEEL_COUNT = 6;

public:
                    MotorizedVehiclePhysics( BaseAllocator* allocator );
                    MotorizedVehiclePhysics( MotorizedVehiclePhysics& vehicle ) = default;
                    ~MotorizedVehiclePhysics() = default;

    void            create( const VehicleParameters& parameters, const dkVec3f& positionWorldSpace, const dkQuatf& orientationWorldSpace );
    void            preStepUpdate( const f32 frameTime, class btDynamicsWorld* dynamicsWorld );
    void            postStepUpdate( const f32 frameTime );

    void            setSteeringState( const f32 steerValue );
    void            setThrottleState( const f32 gasValue );
    void            setBrakingState( const f32 brakeValue );

private:
    BaseAllocator* memoryAllocator;

    // Chassis
    RigidBody*                    chassisRigidBody;

    VehicleWheel                  wheels[MAX_WHEEL_COUNT];

    VehicleParameters             vehicleParameters;

    dkVec3f                       velocity;
    dkVec3f                       forceAccumulator;
    dkVec3f                       torqueAccumulator;
    f32                           steer;
    dkVec3f                       position;

    f32                           engineSpeed;
    f32                           engineTorque;
    u32                           currentGearIndex;

    f32                           inputSteering;
    f32                           inputThrottle;
    f32                           inputBraking;


    std::vector<f32>  torqueCurve;
    f32               maxTorque;
    f32               drag;
    f32               compressionDrag;
    f32               maxAngularVelocity;
    f32               limiter;
    f32               limiterTime;
    f32               currentLimitation;

private:
    f32               getTorque( const f32 angularVelocity );
};
