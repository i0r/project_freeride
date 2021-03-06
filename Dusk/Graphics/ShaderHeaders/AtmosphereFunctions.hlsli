#ifndef __ATMOSPHERE_FUNCS_H__
#define __ATMOSPHERE_FUNCS_H__ 1

#include "Atmosphere.h"

static const float2 IRRADIANCE_TEXTURE_SIZE = float2(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);

Number ClampCosine(Number mu) {
  return clamp(mu, Number(-1.0), Number(1.0));
}
float ClampDistance(float d) {
  return max(d, 0.0 * m);
}
float ClampRadius(IN_BRUNETON(AtmosphereParameters) atmosphere, float r) {
  return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);
}
float SafeSqrt(Area a) {
  return sqrt(max(a, 0.0 * m2));
}
float DistanceToTopAtmosphereBoundary(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu) {
  Area discriminant = r * r * (mu * mu - 1.0) +
      atmosphere.top_radius * atmosphere.top_radius;
  return ClampDistance(-r * mu + SafeSqrt(discriminant));
}
float DistanceToBottomAtmosphereBoundary(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu) {
  Area discriminant = r * r * (mu * mu - 1.0) +
      atmosphere.bottom_radius * atmosphere.bottom_radius;
  return ClampDistance(-r * mu - SafeSqrt(discriminant));
}
bool RayIntersectsGround(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu) {
  return mu < 0.0 && r * r * (mu * mu - 1.0) +
      atmosphere.bottom_radius * atmosphere.bottom_radius >= 0.0 * m2;
}
Number GetLayerDensity(IN_BRUNETON(DensityProfileLayer) layer, float altitude) {
  Number density = layer.exp_term * exp(layer.exp_scale * altitude) +
      layer.linear_term * altitude + layer.constant_term;
  return clamp(density, Number(0.0), Number(1.0));
}
Number GetProfileDensity(IN_BRUNETON(DensityProfile) profile, float altitude) {
  return altitude < profile.layers[0].width ?
      GetLayerDensity(profile.layers[0], altitude) :
      GetLayerDensity(profile.layers[1], altitude);
}
Number GetTextureCoordFromUnitRange(Number x, int texture_size) {
  return 0.5 / Number(texture_size) + x * (1.0 - 1.0 / Number(texture_size));
}
Number GetUnitRangeFromTextureCoord(Number u, int texture_size) {
  return (u - 0.5 / Number(texture_size)) / (1.0 - 1.0 / Number(texture_size));
}
float2 GetIrradianceTextureUvFromRMuS(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu_s) {
  Number x_r = (r - atmosphere.bottom_radius) /
      (atmosphere.top_radius - atmosphere.bottom_radius);
  Number x_mu_s = mu_s * 0.5 + 0.5;
  return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
              GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}
float2 GetTransmittanceTextureUvFromRMu(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu) {
  float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  float rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  float d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
  float d_min = atmosphere.top_radius - r;
  float d_max = rho + H;
  Number x_mu = (d - d_min) / (d_max - d_min);
  Number x_r = rho / H;
  return float2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
              GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}
void GetRMuSFromIrradianceTextureUv(IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(float2) uv, OUT_BRUNETON(float) r, OUT_BRUNETON(Number) mu_s) {
  Number x_mu_s = GetUnitRangeFromTextureCoord(uv.x, IRRADIANCE_TEXTURE_WIDTH);
  Number x_r = GetUnitRangeFromTextureCoord(uv.y, IRRADIANCE_TEXTURE_HEIGHT);
  r = atmosphere.bottom_radius +
      x_r * (atmosphere.top_radius - atmosphere.bottom_radius);
  mu_s = ClampCosine(2.0 * x_mu_s - 1.0);
}
DimensionlessSpectrum GetTransmittanceToTopAtmosphereBoundary(
    AtmosphereParameters atmosphere,
    Texture2D transmittance_texture,
    sampler transmittance_sampler,
    float r, Number mu) {
  float2 uv = GetTransmittanceTextureUvFromRMu(atmosphere, r, mu);
  return transmittance_texture.SampleLevel(transmittance_sampler, uv, 0.0f).rgb;
}
void GetRMuMuSNuFromScatteringTextureUvwz(IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(float4) uvwz, OUT_BRUNETON(float) r, OUT_BRUNETON(Number) mu, OUT_BRUNETON(Number) mu_s,
    OUT_BRUNETON(Number) nu, OUT_BRUNETON(bool) ray_r_mu_intersects_ground) {
  float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  float rho =
      H * GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_R_SIZE);
  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);
  if (uvwz.z < 0.5) {
    float d_min = r - atmosphere.bottom_radius;
    float d_max = rho;
    float d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        1.0 - 2.0 * uvwz.z, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 * m ? Number(-1.0) :
        ClampCosine(-(rho * rho + d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = true;
  } else {
    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    float d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        2.0 * uvwz.z - 1.0, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 * m ? Number(1.0) :
        ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = false;
  }
  Number x_mu_s =
      GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
  float d_min = atmosphere.top_radius - atmosphere.bottom_radius;
  float d_max = H;
  Number A =
      -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);
  Number a = (A - x_mu_s * A) / (1.0 + x_mu_s * A);
  float d = d_min + min(a, A) * (d_max - d_min);
  mu_s = d == 0.0 * m ? Number(1.0) :
     ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));
  nu = ClampCosine(uvwz.x * 2.0 - 1.0);
}
void GetRMuMuSNuFromScatteringTextureFragCoord(
    IN_BRUNETON(AtmosphereParameters) atmosphere, IN_BRUNETON(float3) frag_coord,
    OUT_BRUNETON(float) r, OUT_BRUNETON(Number) mu, OUT_BRUNETON(Number) mu_s, OUT_BRUNETON(Number) nu,
    OUT_BRUNETON(bool) ray_r_mu_intersects_ground) {
  const float4 SCATTERING_TEXTURE_SIZE = float4(
      SCATTERING_TEXTURE_NU_SIZE - 1,
      SCATTERING_TEXTURE_MU_S_SIZE,
      SCATTERING_TEXTURE_MU_SIZE,
      SCATTERING_TEXTURE_R_SIZE);
  Number frag_coord_nu =
      floor(frag_coord.x / Number(SCATTERING_TEXTURE_MU_S_SIZE));
  Number frag_coord_mu_s =
      fmod(frag_coord.x, Number(SCATTERING_TEXTURE_MU_S_SIZE));
  float4 uvwz =
      float4(frag_coord_nu, frag_coord_mu_s, frag_coord.y, frag_coord.z) /
          SCATTERING_TEXTURE_SIZE;
  GetRMuMuSNuFromScatteringTextureUvwz(
      atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  nu = clamp(nu, mu * mu_s - sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)),
      mu * mu_s + sqrt((1.0 - mu * mu) * (1.0 - mu_s * mu_s)));
}
float DistanceToNearestAtmosphereBoundary(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu, bool ray_r_mu_intersects_ground) {
  if (ray_r_mu_intersects_ground) {
    return DistanceToBottomAtmosphereBoundary(atmosphere, r, mu);
  } else {
    return DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
  }
}
DimensionlessSpectrum GetTransmittance(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    float r, Number mu, float d, bool ray_r_mu_intersects_ground) {
  float r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
  Number mu_d = ClampCosine((r * mu + d) / r_d);
  if (ray_r_mu_intersects_ground) {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_texture, transmittance_sampler, r_d, -mu_d) /
        GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_texture, transmittance_sampler, r, -mu),
        DimensionlessSpectrum(1.0, 1.0, 1.0));
  } else {
    return min(
        GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_texture, transmittance_sampler, r, mu) /
        GetTransmittanceToTopAtmosphereBoundary(
            atmosphere, transmittance_texture, transmittance_sampler, r_d, mu_d),
        DimensionlessSpectrum(1.0, 1.0, 1.0));
  }
}
DimensionlessSpectrum GetTransmittanceToSun(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    float r, Number mu_s) {
  Number sin_theta_h = atmosphere.bottom_radius / r;
  Number cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
  return GetTransmittanceToTopAtmosphereBoundary(
          atmosphere, transmittance_texture, transmittance_sampler, r, mu_s) *
      smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,
                 sin_theta_h * atmosphere.sun_angular_radius / rad,
                 mu_s - cos_theta_h);
}
void ComputeSingleScatteringIntegrand(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    float r, Number mu, Number mu_s, Number nu, float d,
    bool ray_r_mu_intersects_ground,
    OUT_BRUNETON(DimensionlessSpectrum) rayleigh, OUT_BRUNETON(DimensionlessSpectrum) mie) {
  float r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
  Number mu_s_d = ClampCosine((r * mu_s + d * nu) / r_d);
  DimensionlessSpectrum transmittance =
      GetTransmittance(
          atmosphere, transmittance_texture, transmittance_sampler, r, mu, d,
          ray_r_mu_intersects_ground) *
      GetTransmittanceToSun(
          atmosphere, transmittance_texture, transmittance_sampler, r_d, mu_s_d);
  rayleigh = transmittance * GetProfileDensity(
      atmosphere.rayleigh_density, r_d - atmosphere.bottom_radius);
  mie = transmittance * GetProfileDensity(
      atmosphere.mie_density, r_d - atmosphere.bottom_radius);
}
void ComputeSingleScattering(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    float r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground,
    OUT_BRUNETON(IrradianceSpectrum) rayleigh, OUT_BRUNETON(IrradianceSpectrum) mie) {
  static const int SAMPLE_COUNT = 50;
  float dx =
      DistanceToNearestAtmosphereBoundary(atmosphere, r, mu,
          ray_r_mu_intersects_ground) / Number(SAMPLE_COUNT);
  DimensionlessSpectrum rayleigh_sum = DimensionlessSpectrum(0.0, 0.0, 0.0);
  DimensionlessSpectrum mie_sum = DimensionlessSpectrum(0.0, 0.0, 0.0);
  for (int i = 0; i <= SAMPLE_COUNT; ++i) {
    float d_i = Number(i) * dx;
    DimensionlessSpectrum rayleigh_i;
    DimensionlessSpectrum mie_i;
    ComputeSingleScatteringIntegrand(atmosphere, transmittance_texture, transmittance_sampler,
        r, mu, mu_s, nu, d_i, ray_r_mu_intersects_ground, rayleigh_i, mie_i);
    Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
    rayleigh_sum += rayleigh_i * weight_i;
    mie_sum += mie_i * weight_i;
  }
  rayleigh = rayleigh_sum * dx * atmosphere.solar_irradiance *
      atmosphere.rayleigh_scattering;
  mie = mie_sum * dx * atmosphere.solar_irradiance * atmosphere.mie_scattering;
}
IrradianceSpectrum GetIrradiance(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(IrradianceTexture) irradiance_texture,
    sampler irradiance_sampler,
    float r, Number mu_s) {
  float2 uv = GetIrradianceTextureUvFromRMuS(atmosphere, r, mu_s);
  return irradiance_texture.SampleLevel(irradiance_sampler, uv, 0.0f).rgb;
}
InverseSolidAngle RayleighPhaseFunction(Number nu) {
  InverseSolidAngle k = 3.0 / (16.0 * PI * sr);
  return k * (1.0 + nu * nu);
}
InverseSolidAngle MiePhaseFunction(Number g, Number nu) {
  InverseSolidAngle k = 3.0 / (8.0 * PI * sr) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}
float4 GetScatteringTextureUvwzFromRMuMuSNu(IN_BRUNETON(AtmosphereParameters) atmosphere,
    float r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground) {
  float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  float rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);
  float r_mu = r * mu;
  Area discriminant =
      r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;
  Number u_mu;
  if (ray_r_mu_intersects_ground) {
    float d = -r_mu - SafeSqrt(discriminant);
    float d_min = r - atmosphere.bottom_radius;
    float d_max = rho;
    u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  } else {
    float d = -r_mu + SafeSqrt(discriminant + H * H);
    float d_min = atmosphere.top_radius - r;
    float d_max = rho + H;
    u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  }
  float d = DistanceToTopAtmosphereBoundary(
      atmosphere, atmosphere.bottom_radius, mu_s);
  float d_min = atmosphere.top_radius - atmosphere.bottom_radius;
  float d_max = H;
  Number a = (d - d_min) / (d_max - d_min);
  Number A =
      -2.0 * atmosphere.mu_s_min * atmosphere.bottom_radius / (d_max - d_min);
  Number u_mu_s = GetTextureCoordFromUnitRange(
      max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);
  Number u_nu = (nu + 1.0) / 2.0;
  return float4(u_nu, u_mu_s, u_mu, u_r);
}
TEMPLATE(AbstractSpectrum)
AbstractSpectrum GetScattering(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(AbstractScatteringTexture TEMPLATE_ARGUMENT(AbstractSpectrum))
        scattering_texture,
    sampler transmittance_sampler,
    float r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground) {
  float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
  Number tex_x = floor(tex_coord_x);
  Number lerp = tex_coord_x - tex_x;
  float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  return AbstractSpectrum(scattering_texture.SampleLevel(transmittance_sampler, uvw0, 0.0f).rgb * (1.0 - lerp) +
      scattering_texture.SampleLevel(transmittance_sampler, uvw1, 0.0f).rgb * lerp);
}
RadianceSpectrum GetScattering(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(ReducedScatteringTexture) single_rayleigh_scattering_texture,
    IN_BRUNETON(ReducedScatteringTexture) single_mie_scattering_texture,
    IN_BRUNETON(ScatteringTexture) multiple_scattering_texture,
    sampler transmittance_sampler,
    float r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground,
    int scattering_order) {
  if (scattering_order == 1) {
    IrradianceSpectrum rayleigh = GetScattering(
        atmosphere, single_rayleigh_scattering_texture, transmittance_sampler, r, mu, mu_s, nu,
        ray_r_mu_intersects_ground);
    IrradianceSpectrum mie = GetScattering(
        atmosphere, single_mie_scattering_texture, transmittance_sampler, r, mu, mu_s, nu,
        ray_r_mu_intersects_ground);
    return rayleigh * RayleighPhaseFunction(nu) +
        mie * MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
  } else {
    return GetScattering(
        atmosphere, multiple_scattering_texture, transmittance_sampler, r, mu, mu_s, nu,
        ray_r_mu_intersects_ground);
  }
}
float3 GetExtrapolatedSingleMieScattering(
    IN_BRUNETON(AtmosphereParameters) atmosphere, IN_BRUNETON(float4) scattering) {
  if (scattering.r == 0.0) {
    return float3(0.0, 0.0, 0.0);
  }
  return scattering.rgb * scattering.a / scattering.r *
	    (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
	    (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
}
IrradianceSpectrum GetCombinedScattering(
    IN_BRUNETON(AtmosphereParameters) atmosphere,
    IN_BRUNETON(ReducedScatteringTexture) scattering_texture,
    IN_BRUNETON(ReducedScatteringTexture) single_mie_scattering_texture,
    sampler lutSampler,
    float r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground,
    OUT_BRUNETON(IrradianceSpectrum) single_mie_scattering) {
  float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
  Number tex_x = floor(tex_coord_x);
  Number lerp = tex_coord_x - tex_x;
  float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  float4 combined_scattering = scattering_texture.SampleLevel( lutSampler, uvw0, 0 ) * (1.0 - lerp) +
      scattering_texture.SampleLevel( lutSampler, uvw1, 0 ) * lerp;
  IrradianceSpectrum scattering = IrradianceSpectrum(combined_scattering.rgb);
  single_mie_scattering =
      GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
  return scattering;
}
#endif
