/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

#include "Graphics/FrameGraph.h"

// TODO Define me as a world entity (to make volume editable in the editor).
struct ParticipatingMedia
{
    enum class VolumeType
    {
        Constant,
        Box,
        Sphere
    };

    dkVec3f     ScatterColor;

    f32         Density;

    Image*      ScatterMask;

    Image*      EmissivityMask;

    dkVec3f     Absorption;

    f32         Anisotropy;

    dkVec3f     Emissivity;

    VolumeType  Type;
};

class VolumetricLightModule 
{
public:
                                VolumetricLightModule();
                                VolumetricLightModule( VolumetricLightModule& ) = delete;
                                VolumetricLightModule& operator = ( VolumetricLightModule& ) = delete;
                                ~VolumetricLightModule();

    void                        addFroxelDataFetchPass( FrameGraph& frameGraph, const u32 width, const u32 height );

    void                        addFroxelScatteringPass( FrameGraph& frameGraph, const u32 width, const u32 height, FGHandle clusterList, FGHandle itemList );

    void                        addIntegrationPass( FrameGraph& frameGraph, const u32 width, const u32 height );

    FGHandle                    addResolvePass( FrameGraph& frameGraph, FGHandle colorBuffer, FGHandle depthBuffer, const u32 width, const u32 height );

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the hard drive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

private:
    FGHandle                    froxelsData[4];

    FGHandle                    vbuffers[2];

    FGHandle                    integratedVbuffers[2];

private:
    DUSK_INLINE FGHandle&       getScatteringFroxelTexHandle() { return froxelsData[0]; }
    DUSK_INLINE FGHandle&       getExtinctionFroxelTexHandle() { return froxelsData[1]; }
    DUSK_INLINE FGHandle&       getEmissionTexHandle() { return froxelsData[2]; }
    DUSK_INLINE FGHandle&       getPhaseTexHandle() { return froxelsData[3]; }
};
