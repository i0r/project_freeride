#ifndef __PHOTOMETRY_H__
#define __PHOTOMETRY_H__ 1
float computeEV100FromAvgLuminance( float avg_luminance )
{
    return log2( avg_luminance * 100.0 / 12.5 );
}

float convertEV100ToExposure(float EV100)
{
    float maxLuminance = 1.2f * pow(2.0f, EV100);
    return 1.0f / maxLuminance;
}

float Luminance2dB( float _Luminance )
{
    return 8.6858896380650365530225783783321 * log( _Luminance );
}

float dB2Luminance( float _dB )
{
    return pow( 10.0, 0.05 * _dB );
}

// RGB to luminance
inline float RGBToLuminance( const in float3 linearRGB )
{
    return dot( float3( 0.2126f, 0.7152f, 0.0722f ), linearRGB );
}
#endif
