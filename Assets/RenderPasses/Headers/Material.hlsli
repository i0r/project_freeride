#ifndef __MATERIAL_H__
#define __MATERIAL_H__ 1
#include <BRDF.hlsli>
#include <Photometry.hlsli>

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
    
    float   BlendMask;
};

float3 BRDF_Default( const float3 L, const float3 V, const float3 N, const Material material )
{
    const float3 H = normalize( V + L );
    const float LoH = saturate( dot( L, H ) );
    const float NoL = saturate( dot( N, L ) );
    const float NoV = saturate( dot( N, V ) );
    const float NoH = saturate( dot( N, H ) );
    
    float BaseColorLuminance = RGBToLuminance( material.BaseColor );    
    float3 F0 = lerp( ( 0.16f * ( material.Reflectance * material.Reflectance ) ), BaseColorLuminance, material.Metalness );   
    float F90 = saturate( 50.0f * dot( material.Reflectance, 0.33f ) );   
    float3 albedo = lerp( material.BaseColor, 0.0f, material.Metalness );
    
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

float3 BlendNormals_UDN( in float3 baseNormal, in float3 topNormal )
{
    static const float3 c = float3( 2, 1, 0 );
    
    float3 blendedNormals = topNormal * c.yyz + baseNormal.xyz;
    blendedNormals = blendedNormals * c.xxx - c.xxy;
    
    return normalize( blendedNormals );
}
#endif
