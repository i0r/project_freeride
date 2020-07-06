#ifndef __COLORSPACE_H__
#define __COLORSPACE_H__ 1
static const float MaxRange = 65025.0f; // RGBD max encodable luminance range

float4 EncodeRGBD(float3 rgb)
{
    float maxRGB = max(rgb.r,max(rgb.g,rgb.b));
    float D      = max(MaxRange / maxRGB, 1.0f);
    D            = saturate(floor(D) / 255.0f);
    return float4(rgb.rgb * (D * (255.0f / MaxRange)), D);
}

float3 DecodeRGBD(float4 rgbd)
{
    return rgbd.rgb * ((MaxRange / 255.0f) / rgbd.a);
}

float3 sRGBToLinearSpace( in float3 sRGBCol )
{
    float3 linearRGBLo = sRGBCol / 12.92;

    // Max( 0, x ) is required to make fxc stfu about X3571
    float3 linearRGBHi = max( 0, pow( abs( ( sRGBCol + 0.055 ) / 1.055 ), 2.4 ) );

    float3 linearRGB = ( sRGBCol <= 0.04045 ) ? linearRGBLo : linearRGBHi;

    return linearRGB;
}

float3 AccurateLinearToSRGB( in float3 linearCol )
{
    float3 sRGBLo = linearCol * 12.92;
    float3 sRGBHi = ( pow( abs( linearCol ), 1.0 / 2.4 ) * 1.055 ) - 0.055;
    float3 sRGB = ( linearCol <= 0.0031308 ) ? sRGBLo : sRGBHi;
    return sRGB;
}

// Academy Color Encoding System [http://www.oscars.org/science-technology/sci-tech-projects/aces]
float3 ACESFilmic(const float3 x) 
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}
#endif
