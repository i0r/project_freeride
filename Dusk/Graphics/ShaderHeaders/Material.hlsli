#ifndef __MATERIAL_H__
#define __MATERIAL_H__ 1
#include "BRDF.hlsli"
#include "Photometry.hlsli"
#include "ColorSpaces.hlsli"

// Material data on the GPU.
struct Material
{
    // Material base color.
    float3  BaseColor;
    
    // Material reflectance (how much a surface reflects).
    float   Reflectance;
    
    // Material roughness.
    float   Roughness;
    
    // Material metalness.
    float   Metalness;
    
    // Material ambient occlusion.
    float   AmbientOcclusion;
    
    // Material emissivity factor.
    float   Emissivity;
    
    // Layer Blend Mask.
    float   BlendMask;
    
    // Clear Coat Intensity.
    float   ClearCoat;
    
    // Clear Coat Glossiness.
    float   ClearCoatGlossiness;
};

float3 BRDF_Default( const float3 L, const float3 V, const float3 N, const Material material, const float3 F0, const float3 albedo )
{
    const float3 H = normalize( V + L );
    const float LoH = saturate( dot( L, H ) );
    const float NoL = max( 0.00001f, dot( N, L ) ); // Avoid division per zero and NaN...
    const float NoV = saturate( dot( N, V ) );
    const float NoH = saturate( dot( N, H ) );
       
    float F90 = saturate( 50.0f * dot( material.Reflectance, 0.33f ) );   

    // Alpha is roughness squared
    float alphaSquared =  Square( Square( material.Roughness ) );
    
    float Fd = Diffuse_MultiScatteringDiffuseBRDF( LoH, NoL, NoV, NoH, material.Roughness );
    
    float distribution = Distribution_GGX( Square( NoH ), alphaSquared );
    float3 fresnel = Fresnel_Schlick( F0, F90, LoH );
    float visibility = Visibility_SmithGGXCorrelated( NoV, NoL, alphaSquared );
    
    float3 diffuse = ( Fd * albedo * INV_PI );
    
    float3 specular = ( distribution * fresnel * visibility );
    
    return ( diffuse + specular ) * NoL;
}

float3 BRDF_ClearCoat( const float3 L, const float3 V, const float3 N, const Material material, const float3 F0, const float3 albedo )
{
    const float3 H = normalize( V + L );
    const float LoH = saturate( dot( L, H ) );
    const float NoH = saturate( dot( N, H ) );
    const float VoH = saturate( dot( V, H ) ) + 1e-5f;

    float F90 = saturate( 50.0f * dot( material.Reflectance, 0.33f ) );
    
    // IOR = 1.5 -> F0 = 0.04
    static const float ClearCoatF0 = 0.04f;
    static const float ClearCoatIOR = 1.5f;
    static const float ClearCoatRefractionFactor = 1 / ClearCoatIOR;
    
    const float clearCoatRoughness = 1.0f - material.ClearCoatGlossiness;
    const float clearCoatLinearRoughness = max( 0.01f, ( clearCoatRoughness * clearCoatRoughness ) );

    // 1. Compute the specular reflection off of the clear coat using the roughness and IOR (F0 specular intensity)
    float clearCoatDistribution = Distribution_GGX( NoH, clearCoatLinearRoughness );
    float clearCoatVisibility = Visibility_Kelemen( VoH );

    // 2. Compute the amount of light transmitted (refracted) into the clear coat using fresnel equations and IOR
    float Fc = pow( 1 - VoH, 5 );
    float clearCoatFresnel = Fc + ( 1 - Fc ) * ClearCoatF0;
    float clearCoatSpecular = ( clearCoatDistribution * clearCoatFresnel * clearCoatVisibility );

    // Refract rays
    float3 L2 = refract( -L, -H, ClearCoatRefractionFactor );
    float3 V2 = refract( -V, -H, ClearCoatRefractionFactor );

    float3 H2 = normalize( V2 + L2 );
    float NoL2 = saturate( dot( N, L2 ) ) + 1e-5f;
    float NoV2 = saturate( dot( N, V2 ) ) + 1e-5f;
    float NoH2 = saturate( dot( N, H2 ) );
    float LoH2 = saturate( dot( L2, H2 ) );

    float3 AbsorptionColor = ( 1 - material.ClearCoat ) + albedo * ( material.ClearCoat * ( 1 / 0.9 ) );
    float  AbsorptionDist = rcp( NoV2 ) + rcp( NoL2 );
    float3 Absorption = pow( AbsorptionColor, 0.5 * AbsorptionDist );

    float F21 = Fresnel_Schlick( ClearCoatF0, F90, saturate( dot( V2, H ) ) ).r;

    float k = Square( material.Roughness ) * 0.5;
    float G_SchlickV2 = NoV2 / ( NoV2 * ( 1 - k ) + k );
    float G_SchlickL2 = NoL2 / ( NoL2 * ( 1 - k ) + k );
    float TotalInternalReflection = 1 - F21 * G_SchlickV2 * G_SchlickL2;

    float3 LayerAttenuation = ( ( 1.0f - clearCoatFresnel ) * TotalInternalReflection ) * Absorption;

    // Specular BRDF
    float3 fresnel = Fresnel_Schlick( 0.9, F90, LoH2 );
    float visibility = Visibility_Schlick( material.Roughness, NoV2, NoL2 );
    float distribution = Distribution_GGX( NoH2, material.Roughness );

    float3 baseLayer = ( distribution * fresnel * visibility ) + ( INV_PI * albedo );

    return ( clearCoatSpecular + baseLayer * LayerAttenuation );
}

float3 BlendNormals_UDN( in float3 baseNormal, in float3 topNormal )
{
    static const float3 c = float3( 2, 1, 0 );
    
    float3 blendedNormals = topNormal * c.yyz + baseNormal.xyz;
    blendedNormals = blendedNormals * c.xxx - c.xxy;
    
    return normalize( blendedNormals );
}
#endif
