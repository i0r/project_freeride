/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct Buffer;
struct Texture;
struct PipelineState;

class RenderDevice;
class CommandList;
class FrameGraph;
class ShaderCache;
class GraphicsAssetCache;

#include "Shared.h"
#include "FrameGraph.h"
#include "ShaderHeaders/Light.h"

#include <Maths/Vector.h>

class LightGrid
{
public:
    struct Data {
        FGHandle	LightClusters;
        FGHandle	ItemList;
        FGHandle	PerSceneBuffer;

        Data()
            : LightClusters( FGHandle::Invalid )
            , ItemList( FGHandle::Invalid )
            , PerSceneBuffer( FGHandle::Invalid )
        {

        }
    };

public:
                                    LightGrid( BaseAllocator* allocator );
                                    LightGrid( LightGrid& ) = default;
                                    LightGrid& operator = ( LightGrid& ) = default;
                                    ~LightGrid();

    // Update light clusters for a given frame graph. Assign light entities to clusters
    // and upload light entities streamed for the current frame.                            
    Data                            updateClusters( FrameGraph& frameGraph );

    // Update current streaming scene AABB (in world units).
    void                            setSceneBounds( const dkVec3f& aabbMax, const dkVec3f& aabbMin );

    // Return a pointer to the main directional light GPU data.
    DirectionalLightGPU*            getDirectionalLightData();

//    uint32_t                        allocatePointLightData( const PointLightData&& lightData );
//    uint32_t                        allocateLocalIBLProbeData( const IBLProbeData&& probeData );
//
//    void                            releasePointLight( const uint32_t pointLightIndex );
//    void                            releaseIBLProbe( const uint32_t iblProbeIndex );
//
//    DirectionalLightData*           updateDirectionalLightData( const DirectionalLightData&& lightData );
//    IBLProbeData*                   updateGlobalIBLProbeData( const IBLProbeData&& probeData );
//
//    const DirectionalLightData*     getDirectionalLightData() const;
//    const IBLProbeData*             getGlobalIBLProbeData() const;
//
//    PointLightData*                 getPointLightData( const uint32_t pointLightIndex );
//    IBLProbeData*                   getIBLProbeData( const uint32_t iblProbeIndex );
//
//#if DUSK_DEVBUILD
//    DirectionalLightData&           getDirectionalLightDataRW();
//#endif

private:
    // GPU data structure updated per scene/streaming update.
    struct PerSceneBufferData {
        DirectionalLightGPU SunLight;

        dkVec3f             ClustersScale;
        f32                 SceneAABBMinX;

        dkVec3f             ClustersInverseScale;
        f32                 SceneAABBMinY;

        dkVec3f             ClustersBias;
        f32                 SceneAABBMinZ;

        dkVec3f             SceneAABBMax;
        u32                 __PADDING__;


        //PointLightGPU       PointLights[MAX_POINT_LIGHT_COUNT];
        //IBLProbeGPU         IBLProbes[MAX_IBL_PROBE_COUNT];
    };
    DUSK_IS_MEMORY_ALIGNED_STATIC( PerSceneBufferData, 16 );

private:
    // Memory allocator owning this instance.
    BaseAllocator*                  memoryAllocator;

    // PerScene constant buffer CPU data.
    PerSceneBufferData              perSceneBufferData;

private:
    // Recalculate clusters infos.
    void                            updateClustersInfos();
};
