/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#ifndef __LIGHTS_DATA_H__
#define __LIGHTS_DATA_H__ 1

#ifdef __cplusplus
#include "HLSLCppInterop.h"
#include <vector>
#else
#define DUSK_IS_MEMORY_ALIGNED_STATIC( x, y )

// TODO Rename this header (which became somehow a garbage field)...
#include "Atmosphere.h"
#endif

// Header shared between CPU and GPU. 
// It should only contain shading-critical attributes (higher level stuff
// should be stored in scene nodes or in the editor).
// 
// CPU types should be typedef'd to their matching GPU type; CPU includes must be kept within the '#ifdef __cplusplus'
// guards.

// Lights Constants
#define MAX_POINT_LIGHT_COUNT               64
#define MAX_SPOT_LIGHT_COUNT                127
#define MAX_DIRECTIONAL_LIGHT_COUNT         1

#define MAX_LOCAL_IBL_PROBE_COUNT           31
#define MAX_GLOBAL_IBL_PROBE_COUNT          1

#define MAX_IBL_PROBE_COUNT                 ( MAX_LOCAL_IBL_PROBE_COUNT + MAX_GLOBAL_IBL_PROBE_COUNT )

#define LINE_RENDERING_MAX_LINE_COUNT       256

#define BRDF_LUT_SIZE 128
#define PROBE_FACE_SIZE 256
#define PROBE_FILTERED_MIP_COUNT 5

#define CSM_MAX_DEPTH 250.0f
#define CSM_SLICE_DIMENSION 2048
#define CSM_SLICE_COUNT 4

#define SSR_MAX_MIP_LEVEL 9
#define SSR_MAX_PREFILTER_LEVEL 7

struct LineInfos 
{
    // Line start (in screen space units).
    float2 P0;

    // Line end (in screen space units).
    float2 P1;

    // Line color (with explicit alpha opacity).
	float3 Color;

    // Line width (in screen space units).
	float     Width;
};
DUSK_IS_MEMORY_ALIGNED_STATIC( LineInfos, 16 );


// A distant source light that is infinitely far away (e.g. the sun).
struct DirectionalLightGPU 
{
    // RGB Color for this light (in linear space).
    float3 ColorLinearSpace;

    // Light Illuminance (in lux). Should be physically correct. Daylight sun is ~111.000lux and night ~<1.0lux.
    float IlluminanceInLux;

    // Normalized light direction. This should be calculated at higher level with user-friendly units/coordinate system
    // (e.g. polar coordinates).
    float3 NormalizedDirection;

    // Angular measurement describing how large a distant light source (with a circular shape) appears from a given 
    // point of view (in radians).
    float AngularRadius;

    float2 SphericalCoordinates;

    // TODO Could be converted to a bitfield in case we get too many flags (but I doubt it'll be the case).

    // Override ColorLinearSpace with the current solar radiance of the atmosphere (physically based).
    uint    UseSolarRadiance;

#ifdef __cplusplus
    uint    PADDING[1];
#endif
};
DUSK_IS_MEMORY_ALIGNED_STATIC( DirectionalLightGPU, 16 );


// A punctual light source (working like a bulb light). Not meant to be physically correct.
struct PointLightGPU
{
    // RGB Color for this light (in linear space).
    float3 ColorLinearSpace;

    // Light Power (in lux). Should be physically correct.
    float PowerInLux;

    // Position of the light (in world space units).
    float3 WorldPosition;

    // Light radius (in world space units).
    float WorldRadius;
};
DUSK_IS_MEMORY_ALIGNED_STATIC( PointLightGPU, 16 );


// A punctual light emitting light in a cone shape.
struct SpotLightGPU
{
    // RGB Color for this light (in linear space).
    float3 ColorLinearSpace;

    // Light Power (in lux). Should be physically correct.
    float PowerInLux;

    // Position of the light (in world space units).
    float3 WorldPosition;

    // Light radius (in world space units).
    float WorldRadius;

    // Normalized light direction.
    float3 NormalizedDirection;

    // The maximum angle between the light direction and the light to pixel vector for pixels that are affected by
    // this light.
    float   Cutoff;

    // Start of the penumbra (falloff is applied for any pixel between Cutoff and OuterCutoff).
    float   OuterCutoff;

    // The inverse of the cosinus of this spotlight cone.
    float   InverseCosCone;

    uint     __PADDING__[2];
};
DUSK_IS_MEMORY_ALIGNED_STATIC( SpotLightGPU, 16 );


// Image-based light reflection probe.
struct IBLProbeGPU
{
    // Position of the probe (in world space units).
    float3 WorldPosition;

    // Probe radius (in world space units).
    float WorldRadius;

    // Index in the streamed cubemap array for this probe.
    int StreamingArrayIndex;

    uint __PADDING__[3];

    // Inverse model matrix of the OOBB for this probe (required for parallax correction).
    // TODO We could probably rebuild last matrix row on the GPU...
    float4x4  InverseModelMatrix;
};
DUSK_IS_MEMORY_ALIGNED_STATIC( IBLProbeGPU, 16 );

// GPU data structure updated per scene/streaming update.
struct PerSceneBufferData
{
    DirectionalLightGPU SunLight;

    float3              ClustersScale;
    float               SceneAABBMinX;

    float3              ClustersInverseScale;
    float               SceneAABBMinY;

    float3              ClustersBias;
    float               SceneAABBMinZ;

    float3              SceneAABBMax;
    uint                PointLightCount;

    PointLightGPU       PointLights[MAX_POINT_LIGHT_COUNT];
    //IBLProbeGPU       IBLProbes[MAX_IBL_PROBE_COUNT];
};
DUSK_IS_MEMORY_ALIGNED_STATIC( PerSceneBufferData, 16 );

struct SmallBatchDrawConstants
{
    float4x4  ModelMatrix;
    uint      MeshEntryIndex;
    uint      PADDING[3];
};

struct SmallBatchData
{
    uint    MeshIndex;
    uint    IndexOffset;
    uint    FaceCount;
    uint    OutputIndexOffset;
    uint    DrawIndex;
    uint    DrawBatchStart;
};

struct MeshCluster 
{
    float3     AABBMin;
    float3     AABBMax;
    float3     ConeCenter;
    float3     ConeAxis;
    float      ConeAngleCosine;
};

struct MeshConstants
{
    uint    VertexCount;
    uint    FaceCount;
    uint    IndexOffset;
    uint    VertexOffset;
};

// Structure holding informations for GPU-driven scene submit.
struct DrawCall
{
    float3      SphereCenter;
    float       SphereRadius;
    float4x4    ModelMatrix;
    uint        MeshEntryIndex;

#ifdef __cplusplus
    uint        __PADDING__[3];
#endif
};

// Entry of a single mesh in the large shared vertex/index buffer.
struct GPUShadowBatchInfos
{
	uint VertexBufferOffset;
    uint VertexBufferCount;
    uint IndiceBufferOffset;
    uint IndiceBufferCount;
};

struct CSMSliceInfos
{
    float4  RenderingMatrix[4];
    float4  CascadePlanes[6];
    float4  CascadeOffsets;
    float4  CascadeScales; 
    float   CascadeSplits;
};

static const int CLUSTER_X = 16;
static const int CLUSTER_Y = 8;
static const int CLUSTER_Z = 24;
static const int CLUSTER_NEAR = 2;
#endif
