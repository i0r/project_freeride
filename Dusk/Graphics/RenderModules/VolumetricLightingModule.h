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
    static constexpr f32 ScatterScale = 0.00692f;
    static constexpr f32 AbsorptScale = 0.00077f;

    enum class VolumeType
    {
        Constant,
        Box,
        Sphere
    };

    enum class BlendType
    {
        Additive,
        AlphaBlended
    };

    dkVec3f     Albedo; // [0..1] * Scale

    f32         Absorption; // [0..1] * Scale

    f32         Phase; // [0..1]

    VolumeType  Type;
    
    BlendType   Blending;
};

class VolumetricLightModule 
{
public:
                                VolumetricLightModule();
                                VolumetricLightModule( VolumetricLightModule& ) = delete;
                                VolumetricLightModule& operator = ( VolumetricLightModule& ) = delete;
                                ~VolumetricLightModule();

    FGHandle                    addVolumeScatteringPass( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, const u32 width, const u32 height );

    // Release and destroy persistent resources created by this module.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the hard drive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );
};
