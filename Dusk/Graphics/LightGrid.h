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

    // Add a point light to the light grid for the current frame.
    u32                             addPointLightData( const PointLightGPU&& lightData );

private:
    // Memory allocator owning this instance.
    BaseAllocator*                  memoryAllocator;

    // PerScene constant buffer CPU data.
    PerSceneBufferData              perSceneBufferData;

private:
    // Recalculate clusters infos.
    void                            updateClustersInfos();
};
