#ifndef __SHARED_H__
#define __SHARED_H__ 1
static const float PI = 3.1415926535897932384626433f;
static const float INV_PI = ( 1.0 / PI );

inline float Square( in float var )
{
    return ( var * var );
}

inline float Pow5( in float var )
{
    return pow( var, 5.0f );
}
#endif
