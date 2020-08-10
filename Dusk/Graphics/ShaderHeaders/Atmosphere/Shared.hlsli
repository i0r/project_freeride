/**
* Copyright (c) 2017 Eric Bruneton
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holders nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef __ATMOSPHERE_H__
#define __ATMOSPHERE_H__ 1

#define RADIANCE_API_ENABLED
sampler ScatteringSampler : register( s0 );

#define IN(x) in x
#define OUT(x) out x
#define TEMPLATE(x)
#define TEMPLATE_ARGUMENT(x)
#define assert(x)
static const int TransmittanceLUT_WIDTH = 256;
static const int TransmittanceLUT_HEIGHT = 64;
static const int g__ScatteringTexture_R_SIZE = 32;
static const int g__ScatteringTexture_MU_SIZE = 128;
static const int g__ScatteringTexture_MU_S_SIZE = 32;
static const int g__ScatteringTexture_NU_SIZE = 8;
static const int g__IrradianceTexture_WIDTH = 64;
static const int g__IrradianceTexture_HEIGHT = 16;
#define COMBINED_g__ScatteringTextureS
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

#define Luminance float

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

#define Abstract_ScatteringTexture Texture3D

#define Reduced_ScatteringTexture Texture3D

#define ScatteringTexture Texture3D

#define ScatteringDensityTexture Texture3D

#define IrradianceTexture Texture2D

static const Length m = 1.0;

static const Wavelength nm = 1.0;

static const Angle rad = 1.0;

static const SolidAngle sr = 1.0;

static const Power watt = 1.0;

static const LuminousPower lm = 1.0;

static const float  BRUNETON_PI = 3.1415926535897932384626433f;

static const Length km = 1000.0 * m;

static const Area m2 = m * m;

static const Volume m3 = m * m * m;

static const Angle pi = BRUNETON_PI * rad;

static const Angle deg = pi / 180.0;

static const Irradiance watt_per_square_meter = watt / m2;

static const Radiance watt_per_square_meter_per_sr = watt / (m2 * sr);

static const SpectralIrradiance watt_per_square_meter_per_nm = watt / (m2 * nm);

static const SpectralRadiance watt_per_square_meter_per_sr_per_nm =

    watt / (m2 * sr * nm);

static const SpectralRadianceDensity watt_per_cubic_meter_per_sr_per_nm =

    watt / (m3 * sr * nm);

static const LuminousIntensity cd = lm / sr;

static const LuminousIntensity kcd = 1000.0 * cd;

static const Luminance cd_per_square_meter = cd / m2;

static const Luminance kcd_per_square_meter = kcd / m2;

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
  IrradianceSpectrum solar_irradiance;
  Angle sun_angular_radius;
  Length bottom_radius;
  Length top_radius;
  DensityProfile rayleigh_density;
  ScatteringSpectrum rayleigh_scattering;
  DensityProfile mie_density;
  ScatteringSpectrum mie_scattering;
  ScatteringSpectrum mie_extinction;
  Number mie_phase_function_g;
  DensityProfile absorption_density;
  ScatteringSpectrum absorption_extinction;
  DimensionlessSpectrum ground_albedo;
  Number mu_s_min;
};

static AtmosphereParameters ATMOSPHERE = {
    float3(1.474000,1.850400,1.911980),
    0.004675f,
    6360.000000f,
    6420.000000f,
    {{
        {0.000000f,0.000000f,0.000000f,0.000000f,0.000000f},
        {0.000000f,1.000000f,-0.125000f,0.000000f,0.000000f}
    }},
    float3(0.005802f,0.013558f,0.033100f),
    {{
        {0.000000f,0.000000f,0.000000f,0.000000f,0.000000f},
        {0.000000,1.000000,-0.833333,0.000000,0.000000}
    }},
    float3(0.003996f,0.003996f,0.003996f),
    float3(0.004440f,0.004440f,0.004440f),
    0.800000f,
    {{
        {25.000000,0.000000,0.000000,0.066667,-0.666667},
        {0.000000,0.000000,0.000000,-0.066667,2.666667}
    }},
    float3(0.000650,0.001881,0.000085),
    float3(0.100000,0.100000,0.100000),
    -0.207912f
};

static const float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(114978.174611,71304.279011,65310.638861);
static const float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(98246.116523,69951.160897,66475.353074);

Number ClampCosine(Number mu) {

  return clamp(mu, Number(-1.0), Number(1.0));

}

Length ClampDistance(Length d) {

  return max(d, 0.0 * m);

}

Length ClampRadius(IN(AtmosphereParameters) atmosphere, Length r) {

  return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);

}

Length SafeSqrt(Area a) {

  return sqrt(max(a, 0.0 * m2));

}

Length DistanceToTopAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu) {

  assert(r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  Area discriminant = r * r * (mu * mu - 1.0) +

      atmosphere.top_radius * atmosphere.top_radius;

  return ClampDistance(-r * mu + SafeSqrt(discriminant));

}

Length DistanceToBottomAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu) {

  assert(r >= atmosphere.bottom_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  Area discriminant = r * r * (mu * mu - 1.0) +

      atmosphere.bottom_radius * atmosphere.bottom_radius;

  return ClampDistance(-r * mu - SafeSqrt(discriminant));

}

bool RayIntersectsGround(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu) {

  assert(r >= atmosphere.bottom_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  return mu < 0.0 && r * r * (mu * mu - 1.0) +

      atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0 * m2;

}

Number GetLayerDensity(IN(DensityProfileLayer) layer, Length altitude) {

  Number density = layer.exp_term * exp(layer.exp_scale * altitude) +

      layer.linear_term * altitude + layer.constant_term;

  return clamp(density, Number(0.0), Number(1.0));

}

Number GetProfileDensity(IN(DensityProfile) profile, Length altitude) {

  return altitude < profile.layers[0].width ?

      GetLayerDensity(profile.layers[0], altitude) :

      GetLayerDensity(profile.layers[1], altitude);

}

Length ComputeOpticalLengthToTopAtmosphereBoundary(

    IN(AtmosphereParameters) atmosphere, IN(DensityProfile) profile,

    Length r, Number mu) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  static const int SAMPLE_COUNT = 500;

  Length dx =

      DistanceToTopAtmosphereBoundary(atmosphere, r, mu) / Number(SAMPLE_COUNT);

  Length result = 0.0 * m;

  for (int i = 0; i <= SAMPLE_COUNT; ++i) {

    Length d_i = Number(i) * dx;

    Length r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);

    Number y_i = GetProfileDensity(profile, r_i - atmosphere.bottom_radius);

    Number weight_i = i == 0 || i == SAMPLE_COUNT ? 0.5 : 1.0;

    result += y_i * weight_i * dx;

  }

  return result;

}

DimensionlessSpectrum ComputeTransmittanceToTopAtmosphereBoundary(

    IN(AtmosphereParameters) atmosphere, Length r, Number mu) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  return exp(-(

      atmosphere.rayleigh_scattering *

          ComputeOpticalLengthToTopAtmosphereBoundary(

              atmosphere, atmosphere.rayleigh_density, r, mu) +

      atmosphere.mie_extinction *

          ComputeOpticalLengthToTopAtmosphereBoundary(

              atmosphere, atmosphere.mie_density, r, mu) +

      atmosphere.absorption_extinction *

          ComputeOpticalLengthToTopAtmosphereBoundary(

              atmosphere, atmosphere.absorption_density, r, mu)));

}

Number GetTextureCoordFromUnitRange(Number x, int texture_size) {

  return 0.5 / Number(texture_size) + x * (1.0 - 1.0 / Number(texture_size));

}

Number GetUnitRangeFromTextureCoord(Number u, int texture_size) {

  return (u - 0.5 / Number(texture_size)) / (1.0 - 1.0 / Number(texture_size));

}

float2 Get_TransmittanceTextureUvFromRMu(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -

      atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length rho =

      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);

  Length d_min = atmosphere.top_radius - r;

  Length d_max = rho + H;

  Number x_mu = (d - d_min) / (d_max - d_min);

  Number x_r = rho / H;

  return float2(GetTextureCoordFromUnitRange(x_mu, TransmittanceLUT_WIDTH),

              GetTextureCoordFromUnitRange(x_r, TransmittanceLUT_HEIGHT));

}

void GetRMuFrom_TransmittanceTextureUv(IN(AtmosphereParameters) atmosphere,

    IN(float2) uv, OUT(Length) r, OUT(Number) mu) {

  assert(uv.x >= 0.0 && uv.x <= 1.0);

  assert(uv.y >= 0.0 && uv.y <= 1.0);

  Number x_mu = GetUnitRangeFromTextureCoord(uv.x, TransmittanceLUT_WIDTH);

  Number x_r = GetUnitRangeFromTextureCoord(uv.y, TransmittanceLUT_HEIGHT);

  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -

      atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length rho = H * x_r;

  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length d_min = atmosphere.top_radius - r;

  Length d_max = rho + H;

  Length d = d_min + x_mu * (d_max - d_min);

  mu = d == 0.0 * m ? Number(1.0) : (H * H - rho * rho - d * d) / (2.0 * r * d);

  mu = ClampCosine(mu);

}

DimensionlessSpectrum ComputeTransmittanceToTopAtmosphereBoundaryTexture(

    IN(AtmosphereParameters) atmosphere, IN(float2) gl_frag_coord) {

  static const float2 TransmittanceLUT_SIZE =

      float2(TransmittanceLUT_WIDTH, TransmittanceLUT_HEIGHT);

  Length r;

  Number mu;

  GetRMuFrom_TransmittanceTextureUv(

      atmosphere, gl_frag_coord / TransmittanceLUT_SIZE, r, mu);

  return ComputeTransmittanceToTopAtmosphereBoundary(atmosphere, r, mu);

}

DimensionlessSpectrum GetTransmittanceToTopAtmosphereBoundary(
    AtmosphereParameters atmosphere,
    Texture2D TransmittanceLUT,
    Length r, 
    Number mu) {

  float2 uv = Get_TransmittanceTextureUvFromRMu(atmosphere, r, mu);

  return DimensionlessSpectrum(TransmittanceLUT.Sample(ScatteringSampler, uv).rgb);

}

DimensionlessSpectrum GetTransmittance(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    Length r, Number mu, Length d, bool ray_r_mu_intersects_ground) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  assert(d >= 0.0 * m);

  Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));

  Number mu_d = ClampCosine((r * mu + d) / r_d);

  if (ray_r_mu_intersects_ground) {

    return min(

        GetTransmittanceToTopAtmosphereBoundary(

            atmosphere, TransmittanceLUT, r_d, -mu_d) /

        GetTransmittanceToTopAtmosphereBoundary(

            atmosphere, TransmittanceLUT, r, -mu),

        DimensionlessSpectrum(1.0, 1.0, 1.0 ));

  } else {

    return min(

        GetTransmittanceToTopAtmosphereBoundary(

            atmosphere, TransmittanceLUT, r, mu) /

        GetTransmittanceToTopAtmosphereBoundary(

            atmosphere, TransmittanceLUT, r_d, mu_d),

        DimensionlessSpectrum(1.0, 1.0, 1.0));

  }

}

DimensionlessSpectrum GetTransmittanceToSun(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    Length r, Number mu_s) {

  Number sin_theta_h = atmosphere.bottom_radius / r;

  Number cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));

  return GetTransmittanceToTopAtmosphereBoundary(

          atmosphere, TransmittanceLUT, r, mu_s) *

      smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,

                 sin_theta_h * atmosphere.sun_angular_radius / rad,

                 mu_s - cos_theta_h);

}

void ComputeSingleScatteringIntegrand(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    Length r, Number mu, Number mu_s, Number nu, Length d,

    bool ray_r_mu_intersects_ground,

    OUT(DimensionlessSpectrum) rayleigh, OUT(DimensionlessSpectrum) mie) {

  Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));

  Number mu_s_d = ClampCosine((r * mu_s + d * nu) / r_d);

  DimensionlessSpectrum transmittance =

      GetTransmittance(

          atmosphere, TransmittanceLUT, r, mu, d,

          ray_r_mu_intersects_ground) *

      GetTransmittanceToSun(

          atmosphere, TransmittanceLUT, r_d, mu_s_d);

  rayleigh = transmittance * GetProfileDensity(

     atmosphere.rayleigh_density, r_d - atmosphere.bottom_radius);

  mie = transmittance * GetProfileDensity(

      atmosphere.mie_density, r_d - atmosphere.bottom_radius);

}

Length DistanceToNearestAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu, bool ray_r_mu_intersects_ground) {

  if (ray_r_mu_intersects_ground) {

    return DistanceToBottomAtmosphereBoundary(atmosphere, r, mu);

  } else {

    return DistanceToTopAtmosphereBoundary(atmosphere, r, mu);

  }

}

void ComputeSingleScattering(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground,

    OUT(IrradianceSpectrum) rayleigh, OUT(IrradianceSpectrum) mie) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  assert(nu >= -1.0 && nu <= 1.0);

  static const int SAMPLE_COUNT = 50;

  Length dx =

      DistanceToNearestAtmosphereBoundary(atmosphere, r, mu,

          ray_r_mu_intersects_ground) / Number(SAMPLE_COUNT);

  DimensionlessSpectrum rayleigh_sum = DimensionlessSpectrum(0.0, 0.0, 0.0);

  DimensionlessSpectrum mie_sum = DimensionlessSpectrum(0.0, 0.0, 0.0);

  for (int i = 0; i <= SAMPLE_COUNT; ++i) {

    Length d_i = Number(i) * dx;

    DimensionlessSpectrum rayleigh_i;

    DimensionlessSpectrum mie_i;

    ComputeSingleScatteringIntegrand(atmosphere, TransmittanceLUT,

        r, mu, mu_s, nu, d_i, ray_r_mu_intersects_ground, rayleigh_i, mie_i);

    Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;

    rayleigh_sum += rayleigh_i * weight_i;

    mie_sum += mie_i * weight_i;

  }

  rayleigh = rayleigh_sum * dx * atmosphere.solar_irradiance *

      atmosphere.rayleigh_scattering;

  mie = mie_sum * dx * atmosphere.solar_irradiance * atmosphere.mie_scattering;

}

InverseSolidAngle RayleighPhaseFunction(Number nu) {

  InverseSolidAngle k = 3.0 / (16.0 * BRUNETON_PI * sr);

  return k * (1.0 + nu * nu);

}

InverseSolidAngle MiePhaseFunction(Number g, Number nu) {

  InverseSolidAngle k = 3.0 / (8.0 * BRUNETON_PI * sr) * (1.0 - g * g) / (2.0 + g * g);

  return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);

}

float4 Get_ScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  assert(nu >= -1.0 && nu <= 1.0);

  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -

      atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length rho =

      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);

  Number u_r = GetTextureCoordFromUnitRange(rho / H, g__ScatteringTexture_R_SIZE);

  Length r_mu = r * mu;

  Area discriminant =

      r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;

  Number u_mu;

  if (ray_r_mu_intersects_ground) {

    Length d = -r_mu - SafeSqrt(discriminant);

    Length d_min = r - atmosphere.bottom_radius;

    Length d_max = rho;

    u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :

        (d - d_min) / (d_max - d_min), g__ScatteringTexture_MU_SIZE / 2);

  } else {

    Length d = -r_mu + SafeSqrt(discriminant + H * H);

    Length d_min = atmosphere.top_radius - r;

    Length d_max = rho + H;

    u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(

        (d - d_min) / (d_max - d_min), g__ScatteringTexture_MU_SIZE / 2);

  }

  Length d = DistanceToTopAtmosphereBoundary(

      atmosphere, atmosphere.bottom_radius, mu_s);

  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;

  Length d_max = H;

  Number a = (d - d_min) / (d_max - d_min);

  Number A =

      -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);

  Number u_mu_s = GetTextureCoordFromUnitRange(

      max(1.0 - a / A, 0.0) / (1.0 + a), g__ScatteringTexture_MU_S_SIZE);

  Number u_nu = (nu + 1.0) / 2.0;

  return float4(u_nu, u_mu_s, u_mu, u_r);

}

void GetRMuMuSNuFrom_ScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,

    IN(float4) uvwz, OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s,

    OUT(Number) nu, OUT(bool) ray_r_mu_intersects_ground) {

  assert(uvwz.x >= 0.0 && uvwz.x <= 1.0);

  assert(uvwz.y >= 0.0 && uvwz.y <= 1.0);

  assert(uvwz.z >= 0.0 && uvwz.z <= 1.0);

  assert(uvwz.w >= 0.0 && uvwz.w <= 1.0);

  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -

      atmosphere.bottom_radius * atmosphere.bottom_radius);

  Length rho =

      H * GetUnitRangeFromTextureCoord(uvwz.w, g__ScatteringTexture_R_SIZE);

  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);

  if (uvwz.z < 0.5) {

    Length d_min = r - atmosphere.bottom_radius;

    Length d_max = rho;

    Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(

        1.0 - 2.0 * uvwz.z, g__ScatteringTexture_MU_SIZE / 2);

    mu = d == 0.0 * m ? Number(-1.0) :

        ClampCosine(-(rho * rho + d * d) / (2.0 * r * d));

    ray_r_mu_intersects_ground = true;

  } else {

    Length d_min = atmosphere.top_radius - r;

    Length d_max = rho + H;

    Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(

        2.0 * uvwz.z - 1.0, g__ScatteringTexture_MU_SIZE / 2);

    mu = d == 0.0 * m ? Number(1.0) :

        ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));

    ray_r_mu_intersects_ground = false;

  }

  Number x_mu_s =

      GetUnitRangeFromTextureCoord(uvwz.y, g__ScatteringTexture_MU_S_SIZE);

  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;

  Length d_max = H;

  Number A =

      -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);

  Number a = (A - x_mu_s * A) / (1.0 + x_mu_s * A);

  Length d = d_min + min(a, A) * (d_max - d_min);

  mu_s = d == 0.0 * m ? Number(1.0) :

     ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));

  nu = ClampCosine(uvwz.x * 2.0 - 1.0);

}

void GetRMuMuSNuFrom_ScatteringTextureFragCoord(

    IN(AtmosphereParameters) atmosphere, IN(float3) gl_frag_coord,

    OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s, OUT(Number) nu,

    OUT(bool) ray_r_mu_intersects_ground) {

  static const float4 g__ScatteringTexture_SIZE = float4(

      g__ScatteringTexture_NU_SIZE - 1,

      g__ScatteringTexture_MU_S_SIZE,

      g__ScatteringTexture_MU_SIZE,

      g__ScatteringTexture_R_SIZE);

  Number frag_coord_nu =

      floor(gl_frag_coord.x / Number(g__ScatteringTexture_MU_S_SIZE));

  Number frag_coord_mu_s =

      fmod(gl_frag_coord.x, Number(g__ScatteringTexture_MU_S_SIZE));

  float4 uvwz =

      float4(frag_coord_nu, frag_coord_mu_s, gl_frag_coord.y, gl_frag_coord.z) /

          g__ScatteringTexture_SIZE;

  GetRMuMuSNuFrom_ScatteringTextureUvwz(

      atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  nu = clamp(nu, mu * mu_s - sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)),

      mu * mu_s + sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)));

}

void ComputeSingle_ScatteringTexture(IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT, IN(float3) gl_frag_coord,

    OUT(IrradianceSpectrum) rayleigh, OUT(IrradianceSpectrum) mie) {

  Length r = 0;

  Number mu = 0;

  Number mu_s = 0;

  Number nu = 0;

  bool ray_r_mu_intersects_ground = false;

  GetRMuMuSNuFrom_ScatteringTextureFragCoord(atmosphere, gl_frag_coord,

      r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  ComputeSingleScattering(atmosphere, TransmittanceLUT,

      r, mu, mu_s, nu, ray_r_mu_intersects_ground, rayleigh, mie);

}

TEMPLATE(AbstractSpectrum)

AbstractSpectrum GetScattering(

    IN(AtmosphereParameters) atmosphere,

    IN(Abstract_ScatteringTexture TEMPLATE_ARGUMENT(AbstractSpectrum))

        g__ScatteringTexture,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground) {

  float4 uvwz = Get_ScatteringTextureUvwzFromRMuMuSNu(

      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  Number tex_coord_x = uvwz.x * Number(g__ScatteringTexture_NU_SIZE - 1);

  Number tex_x = floor(tex_coord_x);

  Number lerp = tex_coord_x - tex_x;

  float3 uvw0 = float3((tex_x + uvwz.y) / Number(g__ScatteringTexture_NU_SIZE),

      uvwz.z, uvwz.w);

  float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(g__ScatteringTexture_NU_SIZE),

      uvwz.z, uvwz.w);

  return AbstractSpectrum(g__ScatteringTexture.Sample(ScatteringSampler, uvw0).rgb * (1.0 - lerp) +

      g__ScatteringTexture.Sample(ScatteringSampler, uvw1).rgb * lerp);

}

RadianceSpectrum GetScattering(

    IN(AtmosphereParameters) atmosphere,

    IN(Reduced_ScatteringTexture) single_rayleigh_g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    IN(ScatteringTexture) multiple_g__ScatteringTexture,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground,

    int scattering_order) {

  if (scattering_order == 1) {

    IrradianceSpectrum rayleigh = GetScattering(

        atmosphere, single_rayleigh_g__ScatteringTexture, r, mu, mu_s, nu,

        ray_r_mu_intersects_ground);

    IrradianceSpectrum mie = GetScattering(

        atmosphere, g__SingleMie_ScatteringTexture, r, mu, mu_s, nu,

        ray_r_mu_intersects_ground);

    return rayleigh * RayleighPhaseFunction(nu) +

        mie * MiePhaseFunction(atmosphere.mie_phase_function_g, nu);

  } else {

    return GetScattering(

        atmosphere, multiple_g__ScatteringTexture, r, mu, mu_s, nu,

        ray_r_mu_intersects_ground);

  }

}

IrradianceSpectrum GetIrradiance(

    IN(AtmosphereParameters) atmosphere,

    IN(IrradianceTexture) g__IrradianceTexture,

    Length r, Number mu_s);

RadianceDensitySpectrum ComputeScatteringDensity(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(Reduced_ScatteringTexture) single_rayleigh_g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    IN(ScatteringTexture) multiple_g__ScatteringTexture,

    IN(IrradianceTexture) g__IrradianceTexture,

    Length r, Number mu, Number mu_s, Number nu, int scattering_order) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  assert(nu >= -1.0 && nu <= 1.0);

  assert(scattering_order >= 2);

  float3 zenith_direction = float3(0.0, 0.0, 1.0);

  float3 omega = float3(sqrt(1.0 - mu * mu), 0.0, mu);

  Number sun_dir_x = omega.x == 0.0 ? 0.0 : (nu - mu * mu_s) / omega.x;

  Number sun_dir_y = sqrt(max(1.0 - sun_dir_x * sun_dir_x - mu_s * mu_s, 0.0));

  float3 omega_s = float3(sun_dir_x, sun_dir_y, mu_s);

  static const int SAMPLE_COUNT = 16;

  static const Angle dphi = pi / Number(SAMPLE_COUNT);

  static const Angle dtheta = pi / Number(SAMPLE_COUNT);

  RadianceDensitySpectrum rayleigh_mie =

      0.0 * watt_per_cubic_meter_per_sr_per_nm;

  for (int l = 0; l < SAMPLE_COUNT; ++l) {

    Angle theta = (Number(l) + 0.5) * dtheta;

    Number cos_theta = cos(theta);

    Number sin_theta = sin(theta);

    bool ray_r_theta_intersects_ground =

        RayIntersectsGround(atmosphere, r, cos_theta);

    Length distance_to_ground = 0.0 * m;

    DimensionlessSpectrum transmittance_to_ground = DimensionlessSpectrum(0.0, 0.0, 0.0);

    DimensionlessSpectrum ground_albedo = DimensionlessSpectrum(0.0, 0.0, 0.0);

    if (ray_r_theta_intersects_ground) {

      distance_to_ground =

          DistanceToBottomAtmosphereBoundary(atmosphere, r, cos_theta);

      transmittance_to_ground =

          GetTransmittance(atmosphere, TransmittanceLUT, r, cos_theta,

              distance_to_ground, true /* ray_intersects_ground */);

      ground_albedo = atmosphere.ground_albedo;

    }

    for (int m = 0; m < 2 * SAMPLE_COUNT; ++m) {

      Angle phi = (Number(m) + 0.5) * dphi;

      float3 omega_i =

          float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

      SolidAngle domega_i = (dtheta / rad) * (dphi / rad) * sin(theta) * sr;

      Number nu1 = dot(omega_s, omega_i);

      RadianceSpectrum incident_radiance = GetScattering(atmosphere,

          single_rayleigh_g__ScatteringTexture, g__SingleMie_ScatteringTexture,

          multiple_g__ScatteringTexture, r, omega_i.z, mu_s, nu1,

          ray_r_theta_intersects_ground, scattering_order - 1);

      float3 ground_normal =

          normalize(zenith_direction * r + omega_i * distance_to_ground);

      IrradianceSpectrum ground_irradiance = GetIrradiance(

          atmosphere, g__IrradianceTexture, atmosphere.bottom_radius,

          dot(ground_normal, omega_s));

      incident_radiance += transmittance_to_ground *

          ground_albedo * (1.0 / (BRUNETON_PI * sr)) * ground_irradiance;

      Number nu2 = dot(omega, omega_i);

      Number rayleigh_density = GetProfileDensity(

          atmosphere.rayleigh_density, r - atmosphere.bottom_radius);

      Number mie_density = GetProfileDensity(

          atmosphere.mie_density, r - atmosphere.bottom_radius);

      rayleigh_mie += incident_radiance * (

          atmosphere.rayleigh_scattering * rayleigh_density *

              RayleighPhaseFunction(nu2) +

          atmosphere.mie_scattering * mie_density *

              MiePhaseFunction(atmosphere.mie_phase_function_g, nu2)) *

          domega_i;

    }

  }

  return rayleigh_mie;

}

RadianceSpectrum ComputeMultipleScattering(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(ScatteringDensityTexture) scattering_density_texture,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu >= -1.0 && mu <= 1.0);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  assert(nu >= -1.0 && nu <= 1.0);

  static const int SAMPLE_COUNT = 50;

  Length dx =

      DistanceToNearestAtmosphereBoundary(

          atmosphere, r, mu, ray_r_mu_intersects_ground) /

              Number(SAMPLE_COUNT);

  RadianceSpectrum rayleigh_mie_sum =

      0.0 * watt_per_square_meter_per_sr_per_nm;

  for (int i = 0; i <= SAMPLE_COUNT; ++i) {

    Length d_i = Number(i) * dx;

    Length r_i =

        ClampRadius(atmosphere, sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r));

    Number mu_i = ClampCosine((r * mu + d_i) / r_i);

    Number mu_s_i = ClampCosine((r * mu_s + d_i * nu) / r_i);

    RadianceSpectrum rayleigh_mie_i =

        GetScattering(

            atmosphere, scattering_density_texture, r_i, mu_i, mu_s_i, nu,

            ray_r_mu_intersects_ground) *

        GetTransmittance(

            atmosphere, TransmittanceLUT, r, mu, d_i,

            ray_r_mu_intersects_ground) *

        dx;

    Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;

    rayleigh_mie_sum += rayleigh_mie_i * weight_i;

  }

  return rayleigh_mie_sum;

}

RadianceDensitySpectrum ComputeScatteringDensityTexture(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(Reduced_ScatteringTexture) single_rayleigh_g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    IN(ScatteringTexture) multiple_g__ScatteringTexture,

    IN(IrradianceTexture) g__IrradianceTexture,

    IN(float3) gl_frag_coord, int scattering_order) {

  Length r;

  Number mu;

  Number mu_s;

  Number nu;

  bool ray_r_mu_intersects_ground;

  GetRMuMuSNuFrom_ScatteringTextureFragCoord(atmosphere, gl_frag_coord,

      r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  return ComputeScatteringDensity(atmosphere, TransmittanceLUT,

      single_rayleigh_g__ScatteringTexture, g__SingleMie_ScatteringTexture,

      multiple_g__ScatteringTexture, g__IrradianceTexture, r, mu, mu_s, nu,

      scattering_order);

}

RadianceSpectrum ComputeMultiple_ScatteringTexture(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(ScatteringDensityTexture) scattering_density_texture,

    IN(float3) gl_frag_coord, OUT(Number) nu) {

  Length r;

  Number mu;

  Number mu_s;

  bool ray_r_mu_intersects_ground;

  GetRMuMuSNuFrom_ScatteringTextureFragCoord(atmosphere, gl_frag_coord,

      r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  return ComputeMultipleScattering(atmosphere, TransmittanceLUT,

      scattering_density_texture, r, mu, mu_s, nu,

      ray_r_mu_intersects_ground);

}

IrradianceSpectrum ComputeDirectIrradiance(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    Length r, Number mu_s) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  Number alpha_s = atmosphere.sun_angular_radius / rad;

  Number average_cosine_factor =

    mu_s < -alpha_s ? 0.0 : (mu_s > alpha_s ? mu_s :

        (mu_s + alpha_s) * (mu_s + alpha_s) / (4.0 * alpha_s));

  return atmosphere.solar_irradiance *

      GetTransmittanceToTopAtmosphereBoundary(

          atmosphere, TransmittanceLUT, r, mu_s) * average_cosine_factor;

}

IrradianceSpectrum ComputeIndirectIrradiance(

    IN(AtmosphereParameters) atmosphere,

    IN(Reduced_ScatteringTexture) single_rayleigh_g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    IN(ScatteringTexture) multiple_g__ScatteringTexture,

    Length r, Number mu_s, int scattering_order) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  assert(scattering_order >= 1);

  static const int SAMPLE_COUNT = 32;

  static const Angle dphi = pi / Number(SAMPLE_COUNT);

  static const Angle dtheta = pi / Number(SAMPLE_COUNT);

  IrradianceSpectrum result =

      0.0 * watt_per_square_meter_per_nm;

  float3 omega_s = float3(sqrt(1.0 - mu_s * mu_s), 0.0, mu_s);

  for (int j = 0; j < SAMPLE_COUNT / 2; ++j) {

    Angle theta = (Number(j) + 0.5) * dtheta;

    for (int i = 0; i < 2 * SAMPLE_COUNT; ++i) {

      Angle phi = (Number(i) + 0.5) * dphi;

      float3 omega =

          float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));

      SolidAngle domega = (dtheta / rad) * (dphi / rad) * sin(theta) * sr;

      Number nu = dot(omega, omega_s);

      result += GetScattering(atmosphere, single_rayleigh_g__ScatteringTexture,

          g__SingleMie_ScatteringTexture, multiple_g__ScatteringTexture,

          r, omega.z, mu_s, nu, false /* ray_r_theta_intersects_ground */,

          scattering_order) *

              omega.z * domega;

    }

  }

  return result;

}

float2 Get_IrradianceTextureUvFromRMuS(IN(AtmosphereParameters) atmosphere,

    Length r, Number mu_s) {

  assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);

  assert(mu_s >= -1.0 && mu_s <= 1.0);

  Number x_r = (r - atmosphere.bottom_radius) /

      (atmosphere.top_radius - atmosphere.bottom_radius);

  Number x_mu_s = mu_s * 0.5 + 0.5;

  return float2(GetTextureCoordFromUnitRange(x_mu_s, g__IrradianceTexture_WIDTH),

              GetTextureCoordFromUnitRange(x_r, g__IrradianceTexture_HEIGHT));

}

void GetRMuSFrom_IrradianceTextureUv(IN(AtmosphereParameters) atmosphere,

    IN(float2) uv, OUT(Length) r, OUT(Number) mu_s) {

  assert(uv.x >= 0.0 && uv.x <= 1.0);

  assert(uv.y >= 0.0 && uv.y <= 1.0);

  Number x_mu_s = GetUnitRangeFromTextureCoord(uv.x, g__IrradianceTexture_WIDTH);

  Number x_r = GetUnitRangeFromTextureCoord(uv.y, g__IrradianceTexture_HEIGHT);

  r = atmosphere.bottom_radius +

      x_r * (atmosphere.top_radius - atmosphere.bottom_radius);

  mu_s = ClampCosine(2.0 * x_mu_s - 1.0);

}

static const float2 g__IrradianceTexture_SIZE =

    float2(g__IrradianceTexture_WIDTH, g__IrradianceTexture_HEIGHT);

IrradianceSpectrum ComputeDirect_IrradianceTexture(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(float2) gl_frag_coord) {

  Length r;

  Number mu_s;

  GetRMuSFrom_IrradianceTextureUv(

      atmosphere, gl_frag_coord / g__IrradianceTexture_SIZE, r, mu_s);

  return ComputeDirectIrradiance(atmosphere, TransmittanceLUT, r, mu_s);

}

IrradianceSpectrum ComputeIndirect_IrradianceTexture(

    IN(AtmosphereParameters) atmosphere,

    IN(Reduced_ScatteringTexture) single_rayleigh_g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    IN(ScatteringTexture) multiple_g__ScatteringTexture,

    IN(float2) gl_frag_coord, int scattering_order) {

  Length r;

  Number mu_s;

  GetRMuSFrom_IrradianceTextureUv(

      atmosphere, gl_frag_coord / g__IrradianceTexture_SIZE, r, mu_s);

  return ComputeIndirectIrradiance(atmosphere,

      single_rayleigh_g__ScatteringTexture, g__SingleMie_ScatteringTexture,

      multiple_g__ScatteringTexture, r, mu_s, scattering_order);

}

IrradianceSpectrum GetIrradiance(

    IN(AtmosphereParameters) atmosphere,

    IN(IrradianceTexture) g__IrradianceTexture,

    Length r, Number mu_s) {

  float2 uv = Get_IrradianceTextureUvFromRMuS(atmosphere, r, mu_s);

  return IrradianceSpectrum(g__IrradianceTexture.Sample(ScatteringSampler, uv).rgb);

}

#ifdef COMBINED_g__ScatteringTextureS

float3 GetExtrapolatedSingleMieScattering(

    IN(AtmosphereParameters) atmosphere, IN(float4) scattering) {

  if (scattering.r == 0.0) {

    return float3(0.0, 0.0, 0.0);

  }

  return scattering.rgb * scattering.a / scattering.r *

	    (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *

	    (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);

}

#endif

IrradianceSpectrum GetCombinedScattering(

    IN(AtmosphereParameters) atmosphere,

    IN(Reduced_ScatteringTexture) g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    Length r, Number mu, Number mu_s, Number nu,

    bool ray_r_mu_intersects_ground,

    OUT(IrradianceSpectrum) single_mie_scattering) {

  float4 uvwz = Get_ScatteringTextureUvwzFromRMuMuSNu(

      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);

  Number tex_coord_x = uvwz.x * Number(g__ScatteringTexture_NU_SIZE - 1);

  Number tex_x = floor(tex_coord_x);

  Number lerp = tex_coord_x - tex_x;

  float3 uvw0 = float3((tex_x + uvwz.y) / Number(g__ScatteringTexture_NU_SIZE),

      uvwz.z, uvwz.w);

  float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(g__ScatteringTexture_NU_SIZE),

      uvwz.z, uvwz.w);

#ifdef COMBINED_g__ScatteringTextureS

  float4 combined_scattering =

      g__ScatteringTexture.Sample(ScatteringSampler, uvw0) * (1.0 - lerp) +

      g__ScatteringTexture.Sample(ScatteringSampler, uvw1) * lerp;

  IrradianceSpectrum scattering = combined_scattering;

  single_mie_scattering =

      GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);

#else

  IrradianceSpectrum scattering = IrradianceSpectrum(

      g__ScatteringTexture.Sample(ScatteringSampler, uvw0).rgb * (1.0 - lerp) +

      g__ScatteringTexture.Sample(ScatteringSampler, uvw1).rgb * lerp);

  single_mie_scattering = IrradianceSpectrum(

      g__SingleMie_ScatteringTexture.Sample(ScatteringSampler, uvw0).rgb * (1.0 - lerp) +

      g__SingleMie_ScatteringTexture.Sample(ScatteringSampler, uvw1).rgb * lerp);

#endif

  return scattering;

}

RadianceSpectrum GetSkyRadiance(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(Reduced_ScatteringTexture) g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    Position camera, IN(Direction) view_ray, Length shadow_length,

    IN(Direction) sun_direction, OUT(DimensionlessSpectrum) transmittance) {

  Length r = length(camera);

  Length rmu = dot(camera, view_ray);

  Length distance_to_top_atmosphere_boundary = -rmu -

      sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);

  if (distance_to_top_atmosphere_boundary > 0.0 * m) {

    camera = camera + view_ray * distance_to_top_atmosphere_boundary;

    r = atmosphere.top_radius;

    rmu += distance_to_top_atmosphere_boundary;

  } else if (r > atmosphere.top_radius) {

    transmittance = DimensionlessSpectrum(1.0, 1.0, 1.0);

    return 0.0 * watt_per_square_meter_per_sr_per_nm;

  }

  Number mu = rmu / r;

  Number mu_s = dot(camera, sun_direction) / r;

  Number nu = dot(view_ray, sun_direction);

  bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);

  transmittance = ray_r_mu_intersects_ground ? DimensionlessSpectrum(0.0, 0.0, 0.0) :

      GetTransmittanceToTopAtmosphereBoundary(

          atmosphere, TransmittanceLUT, r, mu);

  IrradianceSpectrum single_mie_scattering;

  IrradianceSpectrum scattering;

  if (shadow_length == 0.0 * m) {

    scattering = GetCombinedScattering(

        atmosphere, g__ScatteringTexture, g__SingleMie_ScatteringTexture,

        r, mu, mu_s, nu, ray_r_mu_intersects_ground,

        single_mie_scattering);

  } else {

    Length d = shadow_length;

    Length r_p =

        ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));

    Number mu_p = (r * mu + d) / r_p;

    Number mu_s_p = (r * mu_s + d * nu) / r_p;

    scattering = GetCombinedScattering(

        atmosphere, g__ScatteringTexture, g__SingleMie_ScatteringTexture,

        r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,

        single_mie_scattering);

    DimensionlessSpectrum shadow_transmittance =

        GetTransmittance(atmosphere, TransmittanceLUT,

            r, mu, shadow_length, ray_r_mu_intersects_ground);

    scattering = scattering * shadow_transmittance;

    single_mie_scattering = single_mie_scattering * shadow_transmittance;

  }

  return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *

      MiePhaseFunction(atmosphere.mie_phase_function_g, nu);

}

RadianceSpectrum GetSkyRadianceToPoint(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(Reduced_ScatteringTexture) g__ScatteringTexture,

    IN(Reduced_ScatteringTexture) g__SingleMie_ScatteringTexture,

    Position camera, IN(Position) p, Length shadow_length,

    IN(Direction) sun_direction, OUT(DimensionlessSpectrum) transmittance) {

  Direction view_ray = normalize(p - camera);

  Length r = length(camera);

  Length rmu = dot(camera, view_ray);

  Length distance_to_top_atmosphere_boundary = -rmu -

      sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);

  if (distance_to_top_atmosphere_boundary > 0.0 * m) {

    camera = camera + view_ray * distance_to_top_atmosphere_boundary;

    r = atmosphere.top_radius;

    rmu += distance_to_top_atmosphere_boundary;

  }

  Number mu = rmu / r;

  Number mu_s = dot(camera, sun_direction) / r;

  Number nu = dot(view_ray, sun_direction);

  Length d = length(p - camera);

  bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);

  transmittance = GetTransmittance(atmosphere, TransmittanceLUT,

      r, mu, d, ray_r_mu_intersects_ground);

  IrradianceSpectrum single_mie_scattering;

  IrradianceSpectrum scattering = GetCombinedScattering(

      atmosphere, g__ScatteringTexture, g__SingleMie_ScatteringTexture,

      r, mu, mu_s, nu, ray_r_mu_intersects_ground,

      single_mie_scattering);

  d = max(d - shadow_length, 0.0 * m);

  Length r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));

  Number mu_p = (r * mu + d) / r_p;

  Number mu_s_p = (r * mu_s + d * nu) / r_p;

  IrradianceSpectrum single_mie_scattering_p;

  IrradianceSpectrum scattering_p = GetCombinedScattering(

      atmosphere, g__ScatteringTexture, g__SingleMie_ScatteringTexture,

      r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,

      single_mie_scattering_p);

  DimensionlessSpectrum shadow_transmittance = transmittance;

  if (shadow_length > 0.0 * m) {

    shadow_transmittance = GetTransmittance(atmosphere, TransmittanceLUT,

        r, mu, d, ray_r_mu_intersects_ground);

  }

  scattering = scattering - shadow_transmittance * scattering_p;

  single_mie_scattering =

      single_mie_scattering - shadow_transmittance * single_mie_scattering_p;

#ifdef COMBINED_g__ScatteringTextureS

  single_mie_scattering = GetExtrapolatedSingleMieScattering(

      atmosphere, float4(scattering, single_mie_scattering.r));

#endif

  single_mie_scattering = single_mie_scattering *

      smoothstep(Number(0.0), Number(0.01), mu_s);

  return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *

      MiePhaseFunction(atmosphere.mie_phase_function_g, nu);

}

IrradianceSpectrum GetSunAndSkyIrradiance(

    IN(AtmosphereParameters) atmosphere,

    IN(TransmittanceTexture) TransmittanceLUT,

    IN(IrradianceTexture) g__IrradianceTexture,

    IN(Position) p, IN(Direction) normal, IN(Direction) sun_direction,

    OUT(IrradianceSpectrum) sky_irradiance) {

  Length r = length(p);

  Number mu_s = dot(p, sun_direction) / r;

  sky_irradiance = GetIrradiance(atmosphere, g__IrradianceTexture, r, mu_s) *

      (1.0 + dot(normal, p) / r) * 0.5;

  return atmosphere.solar_irradiance *

      GetTransmittanceToSun(

          atmosphere, TransmittanceLUT, r, mu_s) *

      max(dot(normal, sun_direction), 0.0);

}

    #ifdef RADIANCE_API_ENABLED
    RadianceSpectrum GetSolarRadiance() {
      return ATMOSPHERE.solar_irradiance /
          (BRUNETON_PI * ATMOSPHERE.sun_angular_radius * ATMOSPHERE.sun_angular_radius);
    }
    RadianceSpectrum GetSkyRadiance(
        Position camera, Direction view_ray, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
      return GetSkyRadiance(ATMOSPHERE, TransmittanceLUT,
          g__ScatteringTexture, g__SingleMie_ScatteringTexture,
          camera, view_ray, shadow_length, sun_direction, transmittance);
    }
    RadianceSpectrum GetSkyRadianceToPoint(
        Position camera, Position p, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
      return GetSkyRadianceToPoint(ATMOSPHERE, TransmittanceLUT,
          g__ScatteringTexture, g__SingleMie_ScatteringTexture,
          camera, p, shadow_length, sun_direction, transmittance);
    }
    IrradianceSpectrum GetSunAndSkyIrradiance(
       Position p, Direction normal, Direction sun_direction,
       out IrradianceSpectrum sky_irradiance) {
      return GetSunAndSkyIrradiance(ATMOSPHERE, TransmittanceLUT,
          g__IrradianceTexture, p, normal, sun_direction, sky_irradiance);
    }
    #endif
    Luminance3 GetSolarLuminance() {
      return ATMOSPHERE.solar_irradiance /
          (BRUNETON_PI * ATMOSPHERE.sun_angular_radius * ATMOSPHERE.sun_angular_radius) *
          SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
    }
    Luminance3 GetSkyLuminance(
        Position camera, Direction view_ray, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
      return GetSkyRadiance(ATMOSPHERE, TransmittanceLUT,
          g__ScatteringTexture, g__SingleMie_ScatteringTexture,
          camera, view_ray, shadow_length, sun_direction, transmittance) *
          SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
    }
    Luminance3 GetSkyLuminanceToPoint(
        Position camera, Position p, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
      return GetSkyRadianceToPoint(ATMOSPHERE, TransmittanceLUT,
          g__ScatteringTexture, g__SingleMie_ScatteringTexture,
          camera, p, shadow_length, sun_direction, transmittance) *
          SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
    }
    Illuminance3 GetSunAndSkyIlluminance(
       Position p, Direction normal, Direction sun_direction,
       out IrradianceSpectrum sky_irradiance) {
      IrradianceSpectrum sun_irradiance = GetSunAndSkyIrradiance(
          ATMOSPHERE, TransmittanceLUT, g__IrradianceTexture, p, normal,
          sun_direction, sky_irradiance);
      sky_irradiance *= SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
      return sun_irradiance * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
    }
#endif
