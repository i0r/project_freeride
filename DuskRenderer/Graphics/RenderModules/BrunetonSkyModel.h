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

// AtmosphereRendering
//  
//  triggerLUTRecompute();  
//
//  
//  renderAtmosphere( Graph, RT, ZBuffer );
//  {
//   if ( needToRecomputeLUT || !isLUTRecomputeDone() )
//   |
//      if ( useAmortizedRecompute )
//          switch( recomputeStep )
//          case TransmittanceRecompute:
//          (etc)
//      else
//          doAllTheStepAtOnce();
//   |
//   
//   renderSky();
//   applyAtmosphericFog();
//  }

#include "AtmosphereLUTComputeModule.h"

class AtmosphereRenderModule 
{
public:
                                AtmosphereRenderModule();
                                AtmosphereRenderModule( AtmosphereRenderModule& ) = delete;
                                AtmosphereRenderModule& operator = ( AtmosphereRenderModule& ) = delete;
                                ~AtmosphereRenderModule();

    // Render world atmosphere (sky + atmospheric fog). If you want to skip the sky/fog, you can use the functions below.
    ResHandle_t                 renderAtmosphere( FrameGraph& frameGraph, ResHandle_t renderTarget, ResHandle_t depthBuffer );

    // Render sky only. Should happen after the geometry render pass (to take advantage of the early z). Does not include
    // atmospheric fog.
    ResHandle_t                 renderSky( FrameGraph& frameGraph, ResHandle_t renderTarget, ResHandle_t depthBuffer );

    // Render atmospheric fog. Will be applied on anything which has been written to the depth buffer, which is why the fog
    // should be applied after geometry rendeing.
    ResHandle_t                 renderAtmoshpericFog( FrameGraph& frameGraph, ResHandle_t renderTarget, ResHandle_t depthBuffer );

    // Release resources allocated by this RenderModule.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    // Trigger LUTs rebuild for this render module. If forceImmediateRecompute is true, the recomputing steps will be done
    // in a single frame (which might hang low-end devices). Otherwise, the module will use an amortized recompute over
    // several frames.
    void                        triggerLutRecompute( const bool forceImmediateRecompute = false );

private:
    enum class RecomputeStep {
        // Recompute all LUTs during the same frame.
        CompleteRecompute
    };

    // An object describing a single compute job to do (a single LUT update step).
    struct RecomputeJob {
        RecomputeStep   Step;

        RecomputeJob()
            : Step( RecomputeStep::CompleteRecompute )
        {

        }
    };

private:
    // RenderModule used for LUT compute. Can be used as a standfree module; there is just no point
    // splitting it from the atmosphere rendering module right now (might change if we want to precompute
    // atmosphere LUTs offline).
    AtmosphereLUTComputeModule  lutComputeModule;

    // Queue of lut compute job to do.
    std::queue<RecomputeJob>    lutComputeJobTodo;
};

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
