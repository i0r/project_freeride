#ifndef __LIGHTS_DATA_H__
#define __LIGHTS_DATA_H__ 1

// Lights Constants
#define MAX_POINT_LIGHT_COUNT               32
#define MAX_SPOT_LIGHT_COUNT                256
#define MAX_DIRECTIONAL_LIGHT_COUNT         1

#define MAX_LOCAL_IBL_PROBE_COUNT   31
#define MAX_GLOBAL_IBL_PROBE_COUNT  1

#define MAX_IBL_PROBE_COUNT                 ( MAX_LOCAL_IBL_PROBE_COUNT + MAX_GLOBAL_IBL_PROBE_COUNT )
#define IBL_PROBE_DIMENSION                 256

struct DirectionalLight
{
    float4      SunColorAndAngularRadius;
    float4      SunDirectionAndIlluminanceInLux;
    float4      SunReservedData;
};

struct PointLight
{
    float4 PositionAndRadius;
    float4 ColorAndPowerInLux;
};

struct SpotLight
{
    float4  PositionAndRadius;
    float4  ColorAndPowerInLux;
    float4  LightDirectionAndFallOffRadius;
    float   Radius;
    float   InvCosConeDifference;
    float2  EXPLICIT_PADDING;
};

struct SphereLight
{
    float4 PositionAndRadius;
    float4 ColorAndPowerInLux;
};

struct DiscLight
{
    float4 PositionAndRadius;
    float4 ColorAndPowerInLux;
    float3 PlaneNormal;
};

struct RectangleLight
{
    float4  PositionAndRadius;
    float4  ColorAndPowerInLux;
    float3  PlaneNormal;
    float   Width;
    float3  UpVector;
    float   Height;
    float3  LeftVector;
    uint    IsTubeLight;
};

struct IBLProbe
{
    float4 PositionAndRadius;
    float4x4 InverseModelMatrix;
    uint Index;
    
    uint Flags; // IsCaptured; IsDynamic; IsFallbackProbe; UnusedFlags (1 byte per flag) 
    uint CaptureFrequencyPadded; // Frequency (16bits) / Padding(16bits)
};
#endif
