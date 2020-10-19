#ifndef __BRDF_H__
#define __BRDF_H__ 1
#include "Shared.hlsli"

// Generalized Trowbridge-Reitz with gamma=2
float Distribution_GGX( float NoHSquared, float alphaSquared ) 
{
    float t = 1.0 + (alphaSquared - 1.0) * NoHSquared;
    return alphaSquared / (PI * t * t);
}

float3 Fresnel_Schlick( in float3 f0, in float f90, in float u )
{
    return f0 + ( f90 - f0 ) * Pow5( u );
}

float Visibility_SmithGGXCorrelated( in float NdotL, in float NdotV, float alphaG )
{
    // This is the optimized version
    float alphaG2 = Square( alphaG );
    
    // Caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake .
    float Lambda_GGXV = NdotL * sqrt( ( -NdotV * alphaG2 + NdotV ) * NdotV + alphaG2 );
    float Lambda_GGXL = NdotV * sqrt( ( -NdotL * alphaG2 + NdotL ) * NdotL + alphaG2 );

    return 0.5f / ( Lambda_GGXV + Lambda_GGXL );
}

// Stephen McAuley - A Journey Through Implementing Multiscattering BRDFs & Area Lights
float Diffuse_MultiScatteringDiffuseBRDF(float LoH, float NoL, float NoV, float NoH, float Roughness)
{
    float g = saturate(0.18455f * log(2.f / pow(Roughness, 4.f) - 1.f));    
    float f0 = LoH + Pow5(1.f - LoH);
    float f1 = (1.f - 0.75f * Pow5(1.f - NoL)) * (1.f - 0.75f * Pow5(1.f - NoV));
    float t = saturate(2.2f * g - 0.5f);
    float fd = f0 + (f1 - f0) * t;
    float fb = ((34.5f * g - 59.f) * g + 24.5f) * LoH * exp2(-max(73.2f * g - 21.2f, 8.9f) * sqrt(NoH));
    
    return max(fd + fb, 0.f);
}
#endif
