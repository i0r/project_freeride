/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/Types.h>

struct Hash128
{
    union {
        struct {
            u64 low;
            u64 high;
        };

        u64 data[2];
    };

    constexpr bool operator == ( const Hash128 hash ) const
    {
        return ( low ^ hash.low ) == 0 
            && ( high ^ hash.high ) == 0;
    }

    constexpr bool operator != ( const Hash128 hash ) const
    {
        return !( *this == hash );
    }
};

namespace dk
{
    namespace core
    {
        static dkString_t GetHashcodeDigest64( const uint64_t hashcode64 )
        {
            static constexpr size_t HASHCODE_SIZE = sizeof( uint64_t ) << 1;
            static constexpr const dkChar_t* LUT = DUSK_STRING( "0123456789abcdef" );

            dkString_t digest( HASHCODE_SIZE, DUSK_STRING( '0' ) );
            for ( size_t i = 0, j = ( HASHCODE_SIZE - 1 ) * 4; i < HASHCODE_SIZE; ++i, j -= 4 ) {
                digest[i] = LUT[( hashcode64 >> j ) & 0x0f];
            }

            return digest;
        }

        static dkString_t GetHashcodeDigest128( const Hash128& hashcode128 )
        {
            static constexpr size_t HASHCODE_SIZE = sizeof( Hash128 ) << 1;
            static constexpr size_t HALF_HASHCODE_SIZE = ( HASHCODE_SIZE / 2 );
            static constexpr const dkChar_t * LUT = DUSK_STRING( "0123456789abcdef" );
            dkString_t digest( HASHCODE_SIZE, DUSK_STRING( '0' ) );
            for ( size_t i = 0, j = ( HASHCODE_SIZE - 1 ) * 4; i < HASHCODE_SIZE; ++i, j -= 4 ) {
                // NOTE Somehow, MSVC tries to unroll the loop but doesn't write the correct index (the div
                // is skipped)
                // Declaring explicitly the index seems to do the trick
                const size_t index = ( i / HALF_HASHCODE_SIZE );
                digest[i] = LUT[( hashcode128.data[index] >> j ) & 0x0f];
            }

            return digest;
        }
    }
}
