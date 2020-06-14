/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#ifndef __ATMOSPHERE_H__
#define __ATMOSPHERE_H__ 1

#ifdef __cplusplus
#include "HLSLCppInterop.h"

#include <Maths/Helpers.h>
static const float PI = dk::maths::PI<float>();
#else
#include <Shared.hlsli>
#endif

#define TRANSMITTANCE_TEXTURE_WIDTH 256
#define TRANSMITTANCE_TEXTURE_HEIGHT 64

#define SCATTERING_TEXTURE_R_SIZE 32
#define SCATTERING_TEXTURE_MU_SIZE 128
#define SCATTERING_TEXTURE_MU_S_SIZE 32
#define SCATTERING_TEXTURE_NU_SIZE 8

#define IRRADIANCE_TEXTURE_WIDTH 64
#define IRRADIANCE_TEXTURE_HEIGHT 16

#define IN(x) const in x
#define OUT(x) out x
#define TEMPLATE(x)
#define TEMPLATE_ARGUMENT(x)
#define assert(x)

#define COMBINED_SCATTERING_TEXTURES

#define Length float
#define Wavelength float
#define Angle float
#define SolidAngle float
#define Power float
#define LuminousPower float
#define Number float
#define InverseLength float
#define Area float
#define Volume float
#define NumberDensity float
#define Irradiance float
#define Radiance float
#define SpectralPower float
#define SpectralIrradiance float
#define SpectralRadiance float
#define SpectralRadianceDensity float
#define ScatteringCoefficient float
#define InverseSolidAngle float
#define LuminousIntensity float
#define Illuminance float
#define AbstractSpectrum float3
#define DimensionlessSpectrum float3
#define PowerSpectrum float3
#define IrradianceSpectrum float3
#define RadianceSpectrum float3
#define RadianceDensitySpectrum float3
#define ScatteringSpectrum float3
#define Position float3
#define Direction float3
#define Luminance3 float3
#define Illuminance3 float3
#define TransmittanceTexture Texture2D
#define AbstractScatteringTexture Texture3D
#define ReducedScatteringTexture Texture3D
#define ScatteringTexture Texture3D
#define ScatteringDensityTexture Texture3D
#define IrradianceTexture Texture2D

static const Length m = 1.0;
static const Wavelength nm = 1.0;
static const Angle rad = 1.0;
static const SolidAngle sr = 1.0;
static const Power watt = 1.0;
static const LuminousPower lm = 1.0;
static const Length km = 1000.0 * m;
static const Area m2 = m * m;
static const Volume m3 = m * m * m;
static const Angle pi = PI * rad;
static const Angle deg = pi / 180.0f;
static const Irradiance watt_per_square_meter = watt / m2;
static const Radiance watt_per_square_meter_per_sr = watt / (m2 * sr);
static const SpectralIrradiance watt_per_square_meter_per_nm = watt / (m2 * nm);
static const SpectralRadiance watt_per_square_meter_per_sr_per_nm =
    watt / (m2 * sr * nm);
static const SpectralRadianceDensity watt_per_cubic_meter_per_sr_per_nm =
    watt / (m3 * sr * nm);
static const LuminousIntensity cd = lm / sr;
static const LuminousIntensity kcd = 1000.0 * cd;
static const float cd_per_square_meter = cd / m2;
static const float kcd_per_square_meter = kcd / m2;

struct DensityProfileLayer {
    Length width;
    Number exp_term;
    InverseLength exp_scale;
    InverseLength linear_term;
    Number constant_term;
};

struct DensityProfile {
    DensityProfileLayer layers[2];
};

struct AtmosphereParameters {
    DensityProfile rayleigh_density;
    DensityProfile mie_density;
    DensityProfile absorption_density;
    Angle sun_angular_radius;
    Length top_radius;
    
    IrradianceSpectrum solar_irradiance;
    Number mie_phase_function_g;

    ScatteringSpectrum rayleigh_scattering;
    uint                PADDING2;

    ScatteringSpectrum  mie_scattering;
    uint                PADDING;

    ScatteringSpectrum mie_extinction;
    uint                PADDING3;

    ScatteringSpectrum absorption_extinction;
    Length bottom_radius;

    DimensionlessSpectrum ground_albedo;
    Number mu_s_min;
};
DUSK_IS_MEMORY_ALIGNED_STATIC( AtmosphereParameters, 16 );
#endif
