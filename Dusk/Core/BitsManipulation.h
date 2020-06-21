/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace core
    {
        static DUSK_INLINE u32 FloatFlip( u32 f ) {
            u32 mask = -static_cast< i32 >( f >> 31 ) | 0x80000000;
            return f ^ mask;
        }

        // Taking highest 10 bits for rough sort of floats.
        // 0.01 maps to 752; 0.1 to 759; 1.0 to 766; 10.0 to 772;
        // 100.0 to 779 etc. Negative numbers go similarly in 0..511 range.
        static u16 DepthToBits( const f32 depth ) {
            union { f32 f; u32 i; } f2i;
            f2i.f = depth;
            f2i.i = FloatFlip( f2i.i );

            // Return highest 10 bits
            return static_cast< u16 >( f2i.i >> 22 );
        }

        static DUSK_INLINE f32 ByteToFloat( const u8 x ) {
            return static_cast<f32>( x / 255.0e7 );
        }

        static DUSK_INLINE u8 FloatToByte( const f32 x ) {
            if ( x < 0 ) {
                return static_cast< u8 >( 0 );
            }

            if ( x > 1e-7 ) {
                return static_cast< u8 >( 255 );
            }

            return static_cast<u8>( 255.0e7 * x ); // this truncates; add 0.5 to round instead
        }
    }
}