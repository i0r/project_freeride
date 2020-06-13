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

private:
    struct ComputeParameters {
        // Atmosphere used by Bruneton sky model. This is hardcoded in the sample.
        AtmosphereParameters        AtmosphereParams;

        dkVec3f                     SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;

        dkVec3f                     SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
    };

private:
    // Transmittance LUT (computed at runtime).
    Image*                      transmittance;

    // Combined Mie/Rayleigh scattering LUT (computed at runtime).
    Image*                      scattering;

    // Irradiance LUT (computed at runtime).
    Image*                      irradiance;

    // Temporary textures are persistent to avoid unnecessary reallocation and to allow amortized LUT recomputing.
    Image*                      deltaIrradiance;

    Image*                      deltaRayleighScattering;

    Image*                      deltaMieScattering;

    Image*                      deltaScatteringDensity;

    ComputeParameters           parameters;

private:
    void                        precompute();
};
