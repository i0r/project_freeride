/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Vector.h"

namespace
{
    static constexpr double _PI = 3.14159265358979323846;

    static constexpr f32 RadicalInverseBase2( const u32 bits ) {
        u32 bitsShifted = ( bits << 16u ) | ( bits >> 16u );
        bitsShifted = ( ( bitsShifted & 0x55555555u ) << 1u ) | ( ( bitsShifted & 0xAAAAAAAAu ) >> 1u );
        bitsShifted = ( ( bitsShifted & 0x33333333u ) << 2u ) | ( ( bitsShifted & 0xCCCCCCCCu ) >> 2u );
        bitsShifted = ( ( bitsShifted & 0x0F0F0F0Fu ) << 4u ) | ( ( bitsShifted & 0xF0F0F0F0u ) >> 4u );
        bitsShifted = ( ( bitsShifted & 0x00FF00FFu ) << 8u ) | ( ( bitsShifted & 0xFF00FF00u ) >> 8u );

        return static_cast< f32 >( bitsShifted ) * 2.3283064365386963e-10f; // / 0x100000000
    }

    static constexpr f32 GetHaltonValue( i32 index, const i32 radix )
    {
        f32 fraction = 1.0f / static_cast< f32 >( radix );
        
        f32 result = 0.0f;
        while ( index > 0 ) {
            result += static_cast< f32 >( index % radix ) * fraction;

            index /= radix;
            fraction /= static_cast< f32 >( radix );
        }

        return result;
    }
}

namespace dk
{
    namespace maths
    {
        template< typename T >
        static DUSK_INLINE constexpr T PI()
        {
            return (T)_PI;
        }

        template< typename T >
        static DUSK_INLINE constexpr T TWO_PI()
        {
            return ( T )2 * PI<T>();
        }

        template< typename T >
        static DUSK_INLINE constexpr T HALF_PI()
        {
            return PI<T>() / (T)2;
        }

        template< typename T >
        static DUSK_INLINE constexpr T radians( const T x )
        {
            return ( x * PI<T>() ) / ( T )180;
        }

        template< typename T >
        static DUSK_INLINE constexpr T degrees( const T x )
        {
            return ( x * ( T )180 ) / PI<T>();
        }

        template<typename T>
        static DUSK_INLINE constexpr T clamp( const T x, const T minVal, const T maxVal)
        {
            return Min( Max( x, minVal ), maxVal );
        }

        template< typename T >
        static DUSK_INLINE constexpr T abs( const T x )
        {
            return ( x == ( T )0 ? ( T )0 : ( ( x < ( T )0 ) ? -x : x ) );
        }

        template<typename T>
        static DUSK_INLINE constexpr T lerp( const T x, const T y, const f32 a )
        {
            return x * ( 1.0f - a ) + y * a;
        }

        template<typename T>
        static DUSK_INLINE constexpr T sign( const T x )
        {
            return ( x >= 0.0f ) ? +1.0f : -1.0f;
        }

        static DUSK_INLINE constexpr dkVec2f Hammersley2D( const u32 sampleIndex, const u32 sampleCount )
        {
            return dkVec2f( static_cast< f32 >( sampleIndex ) / static_cast< f32 >( sampleCount ), ::RadicalInverseBase2( sampleIndex ) );
        }

        static DUSK_INLINE constexpr dkVec3f SphericalToCarthesianCoordinates( const f32 theta, const f32 gamma )
        {
            return dkVec3f( cos( gamma ) * cos( theta ), cos( gamma ) * sin( theta ), sin( gamma ) );
        }

        static DUSK_INLINE constexpr dkVec2f CarthesianToSphericalCoordinates( const dkVec3f& coords )
        {
            return dkVec2f( atan2( coords.x, coords.y ), atan2( hypot( coords.x, coords.y ), coords.z ) );
        }

        // Generate and return a two-dimensional random halton offset.
        static DUSK_INLINE dkVec2f HaltonOffset2D()
        {
            constexpr i32 SAMPLE_COUNT = 64;
            static i32 SampleIndex = 0;

            dkVec2f offset = dkVec2f(
                GetHaltonValue( SampleIndex & 1023, 2 ),
                GetHaltonValue( SampleIndex & 1023, 3 )
            );

            SampleIndex = ++SampleIndex % SAMPLE_COUNT;

            return offset;
        }
    }
}
