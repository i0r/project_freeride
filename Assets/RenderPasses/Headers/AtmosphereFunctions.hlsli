#ifndef __ATMOSPHERE_FUNCS_H__
#define __ATMOSPHERE_FUNCS_H__ 1

#include <Atmosphere.h>

static const float2 IRRADIANCE_TEXTURE_SIZE = float2(IRRADIANCE_TEXTURE_WIDTH, IRRADIANCE_TEXTURE_HEIGHT);

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
  Area discriminant = r * r * (mu * mu - 1.0) +
      atmosphere.top_radius * atmosphere.top_radius;
  return ClampDistance(-r * mu + SafeSqrt(discriminant));
}
Length DistanceToBottomAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
  Area discriminant = r * r * (mu * mu - 1.0) +
      atmosphere.bottom_radius * atmosphere.bottom_radius;
  return ClampDistance(-r * mu - SafeSqrt(discriminant));
}
bool RayIntersectsGround(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
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
Number GetTextureCoordFromUnitRange(Number x, int texture_size) {
  return 0.5 / Number(texture_size) + x * (1.0 - 1.0 / Number(texture_size));
}
Number GetUnitRangeFromTextureCoord(Number u, int texture_size) {
  return (u - 0.5 / Number(texture_size)) / (1.0 - 1.0 / Number(texture_size));
}
float2 GetIrradianceTextureUvFromRMuS(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu_s) {
  Number x_r = (r - atmosphere.bottom_radius) /
      (atmosphere.top_radius - atmosphere.bottom_radius);
  Number x_mu_s = mu_s * 0.5 + 0.5;
  return float2(GetTextureCoordFromUnitRange(x_mu_s, IRRADIANCE_TEXTURE_WIDTH),
              GetTextureCoordFromUnitRange(x_r, IRRADIANCE_TEXTURE_HEIGHT));
}
float2 GetTransmittanceTextureUvFromRMu(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu) {
  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  Length rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  Length d = DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
  Length d_min = atmosphere.top_radius - r;
  Length d_max = rho + H;
  Number x_mu = (d - d_min) / (d_max - d_min);
  Number x_r = rho / H;
  return float2(GetTextureCoordFromUnitRange(x_mu, TRANSMITTANCE_TEXTURE_WIDTH),
              GetTextureCoordFromUnitRange(x_r, TRANSMITTANCE_TEXTURE_HEIGHT));
}
void GetRMuSFromIrradianceTextureUv(IN(AtmosphereParameters) atmosphere,
    IN(float2) uv, OUT(Length) r, OUT(Number) mu_s) {
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
    Length r, Number mu) {
  float2 uv = GetTransmittanceTextureUvFromRMu(atmosphere, r, mu);
  return transmittance_texture.SampleLevel(transmittance_sampler, uv, 0.0f).rgb;
}
void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
    IN(float4) uvwz, OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s,
    OUT(Number) nu, OUT(bool) ray_r_mu_intersects_ground) {
  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  Length rho =
      H * GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_R_SIZE);
  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);
  if (uvwz.z < 0.5) {
    Length d_min = r - atmosphere.bottom_radius;
    Length d_max = rho;
    Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        1.0 - 2.0 * uvwz.z, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 * m ? Number(-1.0) :
        ClampCosine(-(rho * rho + d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = true;
  } else {
    Length d_min = atmosphere.top_radius - r;
    Length d_max = rho + H;
    Length d = d_min + (d_max - d_min) * GetUnitRangeFromTextureCoord(
        2.0 * uvwz.z - 1.0, SCATTERING_TEXTURE_MU_SIZE / 2);
    mu = d == 0.0 * m ? Number(1.0) :
        ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));
    ray_r_mu_intersects_ground = false;
  }
  Number x_mu_s =
      GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
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
void GetRMuMuSNuFromScatteringTextureFragCoord(
    IN(AtmosphereParameters) atmosphere, IN(float3) frag_coord,
    OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s, OUT(Number) nu,
    OUT(bool) ray_r_mu_intersects_ground) {
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
Length DistanceToNearestAtmosphereBoundary(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu, bool ray_r_mu_intersects_ground) {
  if (ray_r_mu_intersects_ground) {
    return DistanceToBottomAtmosphereBoundary(atmosphere, r, mu);
  } else {
    return DistanceToTopAtmosphereBoundary(atmosphere, r, mu);
  }
}
DimensionlessSpectrum GetTransmittance(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    Length r, Number mu, Length d, bool ray_r_mu_intersects_ground) {
  Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
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
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    Length r, Number mu_s) {
  Number sin_theta_h = atmosphere.bottom_radius / r;
  Number cos_theta_h = -sqrt(max(1.0 - sin_theta_h * sin_theta_h, 0.0));
  return GetTransmittanceToTopAtmosphereBoundary(
          atmosphere, transmittance_texture, transmittance_sampler, r, mu_s) *
      smoothstep(-sin_theta_h * atmosphere.sun_angular_radius / rad,
                 sin_theta_h * atmosphere.sun_angular_radius / rad,
                 mu_s - cos_theta_h);
}
void ComputeSingleScatteringIntegrand(
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    Length r, Number mu, Number mu_s, Number nu, Length d,
    bool ray_r_mu_intersects_ground,
    OUT(DimensionlessSpectrum) rayleigh, OUT(DimensionlessSpectrum) mie) {
  Length r_d = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
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
    IN(AtmosphereParameters) atmosphere,
    IN(TransmittanceTexture) transmittance_texture,
    sampler transmittance_sampler,
    Length r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground,
    OUT(IrradianceSpectrum) rayleigh, OUT(IrradianceSpectrum) mie) {
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
    IN(AtmosphereParameters) atmosphere,
    IN(IrradianceTexture) irradiance_texture,
    sampler irradiance_sampler,
    Length r, Number mu_s) {
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
float4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
    Length r, Number mu, Number mu_s, Number nu,
    bool ray_r_mu_intersects_ground) {
  Length H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  Length rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);
  Length r_mu = r * mu;
  Area discriminant =
      r_mu * r_mu - r * r + atmosphere.bottom_radius * atmosphere.bottom_radius;
  Number u_mu;
  if (ray_r_mu_intersects_ground) {
    Length d = -r_mu - SafeSqrt(discriminant);
    Length d_min = r - atmosphere.bottom_radius;
    Length d_max = rho;
    u_mu = 0.5 - 0.5 * GetTextureCoordFromUnitRange(d_max == d_min ? 0.0 :
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  } else {
    Length d = -r_mu + SafeSqrt(discriminant + H * H);
    Length d_min = atmosphere.top_radius - r;
    Length d_max = rho + H;
    u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange(
        (d - d_min) / (d_max - d_min), SCATTERING_TEXTURE_MU_SIZE / 2);
  }
  Length d = DistanceToTopAtmosphereBoundary(
      atmosphere, atmosphere.bottom_radius, mu_s);
  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
  Length d_max = H;
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
    IN(AtmosphereParameters) atmosphere,
    IN(AbstractScatteringTexture TEMPLATE_ARGUMENT(AbstractSpectrum))
        scattering_texture,
    sampler transmittance_sampler,
    Length r, Number mu, Number mu_s, Number nu,
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
    IN(AtmosphereParameters) atmosphere,
    IN(ReducedScatteringTexture) single_rayleigh_scattering_texture,
    IN(ReducedScatteringTexture) single_mie_scattering_texture,
    IN(ScatteringTexture) multiple_scattering_texture,
    sampler transmittance_sampler,
    Length r, Number mu, Number mu_s, Number nu,
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
// IrradianceSpectrum GetIrradiance(
    // IN(AtmosphereParameters) atmosphere,
    // IN(IrradianceTexture) irradiance_texture,
    // Length r, Number mu_s);
// RadianceSpectrum ComputeMultipleScattering(
    // IN(AtmosphereParameters) atmosphere,
    // IN(TransmittanceTexture) transmittance_texture,
    // IN(ScatteringDensityTexture) scattering_density_texture,
    // Length r, Number mu, Number mu_s, Number nu,
    // bool ray_r_mu_intersects_ground) {
  // assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
  // assert(mu >= -1.0 && mu <= 1.0);
  // assert(mu_s >= -1.0 && mu_s <= 1.0);
  // assert(nu >= -1.0 && nu <= 1.0);
  // const int SAMPLE_COUNT = 50;
  // Length dx =
      // DistanceToNearestAtmosphereBoundary(
          // atmosphere, r, mu, ray_r_mu_intersects_ground) /
              // Number(SAMPLE_COUNT);
  // RadianceSpectrum rayleigh_mie_sum =
      // RadianceSpectrum(0.0 * watt_per_square_meter_per_sr_per_nm);
  // for (int i = 0; i <= SAMPLE_COUNT; ++i) {
    // Length d_i = Number(i) * dx;
    // Length r_i =
        // ClampRadius(atmosphere, sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r));
    // Number mu_i = ClampCosine((r * mu + d_i) / r_i);
    // Number mu_s_i = ClampCosine((r * mu_s + d_i * nu) / r_i);
    // RadianceSpectrum rayleigh_mie_i =
        // GetScattering(
            // atmosphere, scattering_density_texture, r_i, mu_i, mu_s_i, nu,
            // ray_r_mu_intersects_ground) *
        // GetTransmittance(
            // atmosphere, transmittance_texture, r, mu, d_i,
            // ray_r_mu_intersects_ground) *
        // dx;
    // Number weight_i = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
    // rayleigh_mie_sum += rayleigh_mie_i * weight_i;
  // }
  // return rayleigh_mie_sum;
// }

// RadianceSpectrum ComputeMultipleScatteringTexture(
    // IN(AtmosphereParameters) atmosphere,
    // IN(TransmittanceTexture) transmittance_texture,
    // IN(ScatteringDensityTexture) scattering_density_texture,
    // IN(float3) frag_coord, OUT(Number) nu) {
  // Length r;
  // Number mu;
  // Number mu_s;
  // bool ray_r_mu_intersects_ground;
  // GetRMuMuSNuFromScatteringTextureFragCoord(atmosphere, frag_coord,
      // r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  // return ComputeMultipleScattering(atmosphere, transmittance_texture,
      // scattering_density_texture, r, mu, mu_s, nu,
      // ray_r_mu_intersects_ground);
// }
// IrradianceSpectrum ComputeIndirectIrradiance(
    // IN(AtmosphereParameters) atmosphere,
    // IN(ReducedScatteringTexture) single_rayleigh_scattering_texture,
    // IN(ReducedScatteringTexture) single_mie_scattering_texture,
    // IN(ScatteringTexture) multiple_scattering_texture,
    // Length r, Number mu_s, int scattering_order) {
  // assert(r >= atmosphere.bottom_radius && r <= atmosphere.top_radius);
  // assert(mu_s >= -1.0 && mu_s <= 1.0);
  // assert(scattering_order >= 1);
  // const int SAMPLE_COUNT = 32;
  // const Angle dphi = pi / Number(SAMPLE_COUNT);
  // const Angle dtheta = pi / Number(SAMPLE_COUNT);
  // IrradianceSpectrum result =
      // IrradianceSpectrum(0.0 * watt_per_square_meter_per_nm);
  // float3 omega_s = float3(sqrt(1.0 - mu_s * mu_s), 0.0, mu_s);
  // for (int j = 0; j < SAMPLE_COUNT / 2; ++j) {
    // Angle theta = (Number(j) + 0.5) * dtheta;
    // for (int i = 0; i < 2 * SAMPLE_COUNT; ++i) {
      // Angle phi = (Number(i) + 0.5) * dphi;
      // float3 omega =
          // float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
      // SolidAngle domega = (dtheta / rad) * (dphi / rad) * sin(theta) * sr;
      // Number nu = dot(omega, omega_s);
      // result += GetScattering(atmosphere, single_rayleigh_scattering_texture,
          // single_mie_scattering_texture, multiple_scattering_texture,
          // r, omega.z, mu_s, nu, false /* ray_r_theta_intersects_ground */,
          // scattering_order) *
              // omega.z * domega;
    // }
  // }
  // return result;
// }
// IrradianceSpectrum ComputeIndirectIrradianceTexture(
    // IN(AtmosphereParameters) atmosphere,
    // IN(ReducedScatteringTexture) single_rayleigh_scattering_texture,
    // IN(ReducedScatteringTexture) single_mie_scattering_texture,
    // IN(ScatteringTexture) multiple_scattering_texture,
    // IN(float2) frag_coord, int scattering_order) {
  // Length r;
  // Number mu_s;
  // GetRMuSFromIrradianceTextureUv(
      // atmosphere, frag_coord / IRRADIANCE_TEXTURE_SIZE, r, mu_s);
  // return ComputeIndirectIrradiance(atmosphere,
      // single_rayleigh_scattering_texture, single_mie_scattering_texture,
      // multiple_scattering_texture, r, mu_s, scattering_order);
// }

// #ifdef COMBINED_SCATTERING_TEXTURES
// float3 GetExtrapolatedSingleMieScattering(
    // IN(AtmosphereParameters) atmosphere, IN(float4) scattering) {
  // if (scattering.r == 0.0) {
    // return float3(0.0);
  // }
  // return scattering.rgb * scattering.a / scattering.r *
	    // (atmosphere.rayleigh_scattering.r / atmosphere.mie_scattering.r) *
	    // (atmosphere.mie_scattering / atmosphere.rayleigh_scattering);
// }
// #endif
// IrradianceSpectrum GetCombinedScattering(
    // IN(AtmosphereParameters) atmosphere,
    // IN(ReducedScatteringTexture) scattering_texture,
    // IN(ReducedScatteringTexture) single_mie_scattering_texture,
    // Length r, Number mu, Number mu_s, Number nu,
    // bool ray_r_mu_intersects_ground,
    // OUT(IrradianceSpectrum) single_mie_scattering) {
  // float4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      // atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  // Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
  // Number tex_x = floor(tex_coord_x);
  // Number lerp = tex_coord_x - tex_x;
  // float3 uvw0 = float3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      // uvwz.z, uvwz.w);
  // float3 uvw1 = float3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      // uvwz.z, uvwz.w);
// #ifdef COMBINED_SCATTERING_TEXTURES
  // float4 combined_scattering =
      // texture(scattering_texture, uvw0) * (1.0 - lerp) +
      // texture(scattering_texture, uvw1) * lerp;
  // IrradianceSpectrum scattering = IrradianceSpectrum(combined_scattering);
  // single_mie_scattering =
      // GetExtrapolatedSingleMieScattering(atmosphere, combined_scattering);
// #else
  // IrradianceSpectrum scattering = IrradianceSpectrum(
      // texture(scattering_texture, uvw0) * (1.0 - lerp) +
      // texture(scattering_texture, uvw1) * lerp);
  // single_mie_scattering = IrradianceSpectrum(
      // texture(single_mie_scattering_texture, uvw0) * (1.0 - lerp) +
      // texture(single_mie_scattering_texture, uvw1) * lerp);
// #endif
  // return scattering;
// }
// RadianceSpectrum GetSkyRadiance(
    // IN(AtmosphereParameters) atmosphere,
    // IN(TransmittanceTexture) transmittance_texture,
    // IN(ReducedScatteringTexture) scattering_texture,
    // IN(ReducedScatteringTexture) single_mie_scattering_texture,
    // Position camera, IN(Direction) view_ray, Length shadow_length,
    // IN(Direction) sun_direction, OUT(DimensionlessSpectrum) transmittance) {
  // Length r = length(camera);
  // Length rmu = dot(camera, view_ray);
  // Length distance_to_top_atmosphere_boundary = -rmu -
      // sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
  // if (distance_to_top_atmosphere_boundary > 0.0 * m) {
    // camera = camera + view_ray * distance_to_top_atmosphere_boundary;
    // r = atmosphere.top_radius;
    // rmu += distance_to_top_atmosphere_boundary;
  // } else if (r > atmosphere.top_radius) {
    // transmittance = DimensionlessSpectrum(1.0);
    // return RadianceSpectrum(0.0 * watt_per_square_meter_per_sr_per_nm);
  // }
  // Number mu = rmu / r;
  // Number mu_s = dot(camera, sun_direction) / r;
  // Number nu = dot(view_ray, sun_direction);
  // bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
  // transmittance = ray_r_mu_intersects_ground ? DimensionlessSpectrum(0.0) :
      // GetTransmittanceToTopAtmosphereBoundary(
          // atmosphere, transmittance_texture, r, mu);
  // IrradianceSpectrum single_mie_scattering;
  // IrradianceSpectrum scattering;
  // if (shadow_length == 0.0 * m) {
    // scattering = GetCombinedScattering(
        // atmosphere, scattering_texture, single_mie_scattering_texture,
        // r, mu, mu_s, nu, ray_r_mu_intersects_ground,
        // single_mie_scattering);
  // } else {
    // Length d = shadow_length;
    // Length r_p =
        // ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
    // Number mu_p = (r * mu + d) / r_p;
    // Number mu_s_p = (r * mu_s + d * nu) / r_p;
    // scattering = GetCombinedScattering(
        // atmosphere, scattering_texture, single_mie_scattering_texture,
        // r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
        // single_mie_scattering);
    // DimensionlessSpectrum shadow_transmittance =
        // GetTransmittance(atmosphere, transmittance_texture,
            // r, mu, shadow_length, ray_r_mu_intersects_ground);
    // scattering = scattering * shadow_transmittance;
    // single_mie_scattering = single_mie_scattering * shadow_transmittance;
  // }
  // return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
      // MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
// }
// RadianceSpectrum GetSkyRadianceToPoint(
    // IN(AtmosphereParameters) atmosphere,
    // IN(TransmittanceTexture) transmittance_texture,
    // IN(ReducedScatteringTexture) scattering_texture,
    // IN(ReducedScatteringTexture) single_mie_scattering_texture,
    // Position camera, IN(Position) point, Length shadow_length,
    // IN(Direction) sun_direction, OUT(DimensionlessSpectrum) transmittance) {
  // Direction view_ray = normalize(point - camera);
  // Length r = length(camera);
  // Length rmu = dot(camera, view_ray);
  // Length distance_to_top_atmosphere_boundary = -rmu -
      // sqrt(rmu * rmu - r * r + atmosphere.top_radius * atmosphere.top_radius);
  // if (distance_to_top_atmosphere_boundary > 0.0 * m) {
    // camera = camera + view_ray * distance_to_top_atmosphere_boundary;
    // r = atmosphere.top_radius;
    // rmu += distance_to_top_atmosphere_boundary;
  // }
  // Number mu = rmu / r;
  // Number mu_s = dot(camera, sun_direction) / r;
  // Number nu = dot(view_ray, sun_direction);
  // Length d = length(point - camera);
  // bool ray_r_mu_intersects_ground = RayIntersectsGround(atmosphere, r, mu);
  // transmittance = GetTransmittance(atmosphere, transmittance_texture,
      // r, mu, d, ray_r_mu_intersects_ground);
  // IrradianceSpectrum single_mie_scattering;
  // IrradianceSpectrum scattering = GetCombinedScattering(
      // atmosphere, scattering_texture, single_mie_scattering_texture,
      // r, mu, mu_s, nu, ray_r_mu_intersects_ground,
      // single_mie_scattering);
  // d = max(d - shadow_length, 0.0 * m);
  // Length r_p = ClampRadius(atmosphere, sqrt(d * d + 2.0 * r * mu * d + r * r));
  // Number mu_p = (r * mu + d) / r_p;
  // Number mu_s_p = (r * mu_s + d * nu) / r_p;
  // IrradianceSpectrum single_mie_scattering_p;
  // IrradianceSpectrum scattering_p = GetCombinedScattering(
      // atmosphere, scattering_texture, single_mie_scattering_texture,
      // r_p, mu_p, mu_s_p, nu, ray_r_mu_intersects_ground,
      // single_mie_scattering_p);
  // DimensionlessSpectrum shadow_transmittance = transmittance;
  // if (shadow_length > 0.0 * m) {
    // shadow_transmittance = GetTransmittance(atmosphere, transmittance_texture,
        // r, mu, d, ray_r_mu_intersects_ground);
  // }
  // scattering = scattering - shadow_transmittance * scattering_p;
  // single_mie_scattering =
      // single_mie_scattering - shadow_transmittance * single_mie_scattering_p;
// #ifdef COMBINED_SCATTERING_TEXTURES
  // single_mie_scattering = GetExtrapolatedSingleMieScattering(
      // atmosphere, float4(scattering, single_mie_scattering.r));
// #endif
  // single_mie_scattering = single_mie_scattering *
      // smoothstep(Number(0.0), Number(0.01), mu_s);
  // return scattering * RayleighPhaseFunction(nu) + single_mie_scattering *
      // MiePhaseFunction(atmosphere.mie_phase_function_g, nu);
// }
// IrradianceSpectrum GetSunAndSkyIrradiance(
    // IN(AtmosphereParameters) atmosphere,
    // IN(TransmittanceTexture) transmittance_texture,
    // IN(IrradianceTexture) irradiance_texture,
    // IN(Position) point, IN(Direction) normal, IN(Direction) sun_direction,
    // OUT(IrradianceSpectrum) sky_irradiance) {
  // Length r = length(point);
  // Number mu_s = dot(point, sun_direction) / r;
  // sky_irradiance = GetIrradiance(atmosphere, irradiance_texture, r, mu_s) *
      // (1.0 + dot(normal, point) / r) * 0.5;
  // return atmosphere.solar_irradiance *
      // GetTransmittanceToSun(
          // atmosphere, transmittance_texture, r, mu_s) *
      // max(dot(normal, sun_direction), 0.0);
// }
#endif
