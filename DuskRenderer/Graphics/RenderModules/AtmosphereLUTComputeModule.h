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

constexpr f64 kLengthUnitInMeters = 1000.0;
constexpr f64 kBottomRadius = 6360000.0;
constexpr f64 kSunAngularRadius = 0.00935 / 2.0;

class AtmosphereLUTComputeModule 
{
public:
    DUSK_INLINE dkVec3f getSkySpectralRadianceToLuminance() const { return SKY_SPECTRAL_RADIANCE_TO_LUMINANCE; }
    DUSK_INLINE dkVec3f getSunSpectralRadianceToLuminance() const { return SUN_SPECTRAL_RADIANCE_TO_LUMINANCE; }

    DUSK_INLINE Image* getTransmittanceLut() const { return transmittance; }
    DUSK_INLINE Image* getScatteringLut() const { return scattering[0]; }
    DUSK_INLINE Image* getIrradianceLut() const { return irradiance[0]; }
    DUSK_INLINE Image* getMieScatteringLut() const { return deltaMieScattering; }

    DUSK_INLINE const AtmosphereParameters& getAtmosphereParameters() const { return properties.AtmosphereParams; }

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
    Image*                      scattering[2];

    // Irradiance LUT (computed at runtime). This is double buffered to do
    // the scattering accumulation (the last step of the LUT calculation).
    Image*                      irradiance[2];

    // Temporary textures are persistent to avoid unnecessary reallocation and to allow amortized LUT recomputing.
    Image*                      deltaIrradiance;

    Image*                      deltaRayleighScattering;

    Image*                      deltaMieScattering;

    Image*                      deltaScatteringDensity;
    
    AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties properties;

    dkVec3f SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
    dkVec3f SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;

private:
    // Perform a precompute iteration. The actual precomputations depend on whether we want to store precomputed
    // irradiance or illuminance values.
    void                        precomputeIteration( FrameGraph& frameGraph, const dkVec3f& lambdas, const u32 num_scattering_orders, const bool enableBlending = false );
};
