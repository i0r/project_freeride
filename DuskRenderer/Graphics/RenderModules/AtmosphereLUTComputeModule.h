/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class FrameGraph;
class GraphicsAssetCache;
class RenderDevice;

struct Image;

#include <Graphics/RenderPasses/Headers/Atmosphere.h>
#include "Generated/AtmosphereLUTCompute.generated.h"

class AtmosphereLUTComputeModule 
{
public:
                                AtmosphereLUTComputeModule();
                                AtmosphereLUTComputeModule( AtmosphereLUTComputeModule& ) = delete;
                                AtmosphereLUTComputeModule& operator = ( AtmosphereLUTComputeModule& ) = delete;
                                ~AtmosphereLUTComputeModule();

    // Release resources allocated by this RenderModule.
    void                        destroy( RenderDevice& renderDevice );

    // Load cached resource from the harddrive and pre-allocate resources for this module.
    void                        loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache );

    // Compute atmosphere scattering LUTs. 
    void                        precomputePipelineResources( FrameGraph& frameGraph );
        
private:
    // Transmittance LUT (computed at runtime).
    Image*                      transmittance;

    // Combined Mie/Rayleigh scattering LUT (computed at runtime).
    Image*                      scattering;

    // Irradiance LUT (computed at runtime). This is double buffered to do
    // the scattering accumulation (the last step of the LUT calculation).
    Image*                      irradiance[2];

    // Temporary textures are persistent to avoid unnecessary reallocation and to allow amortized LUT recomputing.
    Image*                      deltaIrradiance;

    Image*                      deltaRayleighScattering;

    Image*                      deltaMieScattering;

    Image*                      deltaScatteringDensity;
    
    AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties properties;

private:
    // Perform a precompute iteration. The actual precomputations depend on whether we want to store precomputed
    // irradiance or illuminance values.
    void                        precomputeIteration( FrameGraph& frameGraph, const dkVec3f& lambdas, const u32 num_scattering_orders, const bool enableBlending = false );
};
