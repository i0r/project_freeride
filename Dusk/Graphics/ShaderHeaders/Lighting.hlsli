#ifndef __LIGHTING_H__
#define __LIGHTING_H__ 1

#include <Light.h>

float3 GetPointLightIlluminance( in PointLightGPU light, in float4 positionWS, in float depthVs, inout float3 L )
{
    float3 unormalizedL = light.WorldPosition - positionWS.xyz;
    float distance = length( unormalizedL );
    L = normalize( unormalizedL );

    float illuminance = pow( saturate( 1 - pow( ( distance / light.WorldRadius ), 4 ) ), 2 ) / ( distance * distance + 1 );
    float luminancePower = light.PowerInLux / ( 4.0f * sqrt( light.WorldRadius * PI ) );

    return light.ColorLinearSpace * luminancePower * illuminance;
}    
#endif
