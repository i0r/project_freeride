/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once
class ShaderCache;
class GraphicsAssetCache;

struct Image;
struct PipelineState;

#include <Graphics/FrameGraph.h>

class BrunetonSkyRenderModule
{
public:
                                BrunetonSkyRenderModule();
                                BrunetonSkyRenderModule( BrunetonSkyRenderModule& ) = delete;
                                BrunetonSkyRenderModule& operator = ( BrunetonSkyRenderModule& ) = delete;
                                ~BrunetonSkyRenderModule();


    ResHandle_t                 renderSky( FrameGraph& frameGraph, ResHandle_t renderTarget, ResHandle_t depthBuffer, const bool renderSunDisk = true, const bool useAutomaticExposure = true );

    void                        destroy( RenderDevice& renderDevice );
    void                        loadCachedResources( RenderDevice& renderDevice, ShaderCache& shaderCache, GraphicsAssetCache& graphicsAssetCache );

    void                        setSunSphericalPosition( const f32 verticalAngleRads, const f32 horizontalAngleRads );
    dkVec2f                     getSunSphericalPosition();

    void                        setLookUpTables( Image* transmittance, Image* scattering, Image* irradiance );

private:
    PipelineState*              skyRenderPso[5];
    PipelineState*              skyRenderNoSunFixedExposurePso[5];

    Image*                      transmittanceTexture;
    Image*                      scatteringTexture;
    Image*                      irradianceTexture;

    f32                         sunVerticalAngle;
    f32                         sunHorizontalAngle;
    f32                         sunAngularRadius;

    struct {
        dkVec3f                 EarthCenter;
        f32                     SunSizeX;
        dkVec3f                 SunDirection;
        f32                     SunSizeY;
    } parameters;

private:
    PipelineState*              getPipelineStatePermutation( const u32 samplerCount, const bool renderSunDisk, const bool useAutomaticExposure );
};
