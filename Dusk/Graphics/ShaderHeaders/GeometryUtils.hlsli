#ifndef __GEO_UTILS_H__
#define __GEO_UTILS_H__ 1
float2 EncodeNormals( float3 n )
{
    static const float scale = 1.7777f;
    
    float2 enc = n.xy / (n.z+1);
    enc /= scale;
    enc = enc*0.5+0.5;
    return enc;
}

float3 DecodeNormals( float2 enc )
{
    static const float scale = 1.7777f;
    
    float3 nn = enc.rgr * float3(2*scale,2*scale,0) + float3(-scale,-scale,1);
    float g = 2.0f / dot(nn.xyz,nn.xyz);
    
    float3 n;
    n.xy = g*nn.xy;
    n.z = g-1;
    return n;
}
        
float LinearizeDepth(float depth, float near, float far)
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

float3 GetScreenPos( float2 uv, float depth ) 
{
    return float3( uv.x * 2.0f - 1.0f, ( 1.0f - uv.y ) * 2.0f - 1.0f, depth );
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
    float3 UpVector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(UpVector, N));
    float3 B = cross(N, T);

    return float4((T * H.x) + (B * H.y) + (N * H.z), H.w);
}

float ViewDepth(float depth, float near, float far) 
{
    return (far * near) / (far - depth * (far - near));
}
#endif