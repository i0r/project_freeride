#ifndef __GEO_UTILS_H__
#define __GEO_UTILS_H__ 1

#include <Shared.hlsli>

// Spherical Coordinates encoding (ALU heavy but suitable for world space normals).
// Half precision should be good enough for screenspace techniques.
float2 EncodeNormals( float3 n )
{
    return float2( float2( atan2( n.y, n.x ) / PI, n.z ) + 1.0f ) * 0.5f;
}

// Decode encoded normals from a Gbuffer sample (see EncodeNormals).
float3 DecodeNormals( float2 enc )
{
    float2 ang = enc*2-1;
    float2 scth;
    sincos(ang.x * PI, scth.x, scth.y);
    float2 scphi = float2(sqrt(1.0 - ang.y*ang.y), ang.y);
    return float3(scth.y*scphi.x, scth.x*scphi.x, scphi.y);
}

// Return linearized depth. Note that if the app is using reversed z, you should use ConvertFromReversedDepth
// instead (the depth will already be linearized).
float LinearizeDepth(float depth, float near, float far)
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

// Return Linear depth from a reversed depth sample. Required so that we don't have
// to modify raymarching algorithm for reversed z.
float ConvertFromReversedDepth(float d,float zNear)
{
    return zNear / d;
}

// Return screen position from uv coordinates (0..1) and linearized depth.
float4 GetScreenPos(in float2 uv, in float z)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0 - uv.y) * 2.0f - 1.0f;
    return float4(x, y, z, 1.0f);
}

// Return a given texel world position from its uv coordinates and linearized depth.
float3 ReconstructPosition(in float2 uv, in float z, in float4x4 InvVP)
{
    float4 position_s = GetScreenPos( uv, z );
    float4 position_v = mul(position_s, InvVP);
    return position_v.xyz / position_v.w;
}

float3 GetViewDir( float3 worldPos ) 
{
    return normalize( worldPos - g_WorldPosition );
}

float4 TangentToWorld(float3 N, float4 H) 
{
    float3 UpVector = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	float3 T = normalize(cross(UpVector, N));
	float3 B = cross(N, T);

	return float4((T * H.x) + (B * H.y) + (N * H.z), H.w);
}
#endif