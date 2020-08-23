#ifndef __GEO_UTILS_H__
#define __GEO_UTILS_H__ 1
float2 EncodeNormals( float3 normalizedNormal )
{
    float2 enc = normalize( normalizedNormal.xy ) * sqrt( -normalizedNormal.z * 0.5f + 0.5f );
    return enc * 0.5f + 0.5f;
}

float3 DecodeNormals( float2 encodedNormals )
{
    float2 fenc = encodedNormals * 4 - 2;
    float f = dot( fenc, fenc );
    float g = sqrt( 1 - f / 4 );
    float3 n = float3( fenc * g, 1 - f / 2 );
    return n;
}
        
float3 GetScreenPos( float2 uv, float depth ) 
{
    return float3( uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y, depth );
}

float3 ReconstructWorldPos( float2 uv, float depth ) 
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2;
    float4 viewPos = mul( g_InverseProjectionMatrix, float4( ndcX, ndcY, depth, 1.0f ) );
    viewPos = viewPos / viewPos.w;
    return mul( g_InverseViewMatrix, viewPos ).xyz;
}

float3 GetViewDir( float3 worldPos ) 
{
    return normalize( worldPos - g_WorldPosition );
}

float3 GetViewPos( float3 screenPos ) 
{
    float4 viewPos = mul( g_InverseProjectionMatrix, float4( screenPos, 1.0f ) );
    return viewPos.xyz / viewPos.w;
}

float4 TangentToWorld(float3 N, float4 H) 
{
    float3 UpVector = abs(N.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(UpVector, N));
    float3 B = cross(N, T);

    return float4((T * H.x) + (B * H.y) + (N * H.z), H.w);
}

float Linear01Depth(float depth, float near, float far) 
{
    return (1.0f - (depth * near) / (far - depth * (far - near)));
}

float ViewDepth(float depth, float near, float far) 
{
    return (far * near) / (far - depth * (far - near));
}
#endif