/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace
{
    // Use Castagnoli or default polynomial
    static constexpr u32 POLYNOMIAL = 0x82f63b78;

    // Generate CRC lookup table
    template <unsigned c, i32 k = 8>
    struct f : f<( ( c & 1 ) ? POLYNOMIAL : 0x0 ) ^ ( c >> 1 ), k - 1>
    {};
    template <unsigned c> struct f<c, 0>
    {
        enum
        {
            value = c
        };
    };

#define A(x) B(x) B(x + 128)
#define B(x) C(x) C(x +  64)
#define C(x) D(x) D(x +  32)
#define D(x) E(x) E(x +  16)
#define E(x) F(x) F(x +   8)
#define F(x) G(x) G(x +   4)
#define G(x) H(x) H(x +   2)
#define H(x) I(x) I(x +   1)
#define I(x) f<x>::value ,

constexpr unsigned crc_table[] = { A( 0 ) };

#undef I
#undef H
#undef G
#undef F
#undef E
#undef D
#undef C
#undef B
#undef A

    // Constexpr implementation and helpers
    // Please keep those in this anonymous namespace, so that the global scope is not polluted by these

    //=====================================
    //  crc32_impl
    //      Conmpile time CRC32 implementation
    //
    //      Parameters:
    //          dataPointer: read data pointer (starting at offset 0 if this is the first function call)
    //          dataLength: length of 'p' data left to read (in bytes)
    //          crc: crc generated from previous call (should be 0xFFFFFFFF if this is the first function call)
    //      
    //      Return:
    //          Recursively call itself if 'len' is not null (= hash is not generated yet), return 'crc' otherwise.
    //=====================================
    constexpr u32 crc32_impl( const char* dataPointer, size_t dataLength, u32 crc = ~0u )
    {
        return ( dataLength != 0 ) ? crc32_impl( dataPointer + 1, dataLength - 1, ( crc >> 8 ) ^ crc_table[( crc & 0xFF ) ^ static_cast<unsigned char>( *dataPointer )] ) : crc;
    }

    constexpr u32 crc32_impl( const wchar_t* dataPointer, size_t dataLength, u32 crc = ~0u )
    {
        return ( dataLength != 0 ) ? crc32_impl( dataPointer + 1, dataLength - 1, ( crc >> 8 ) ^ crc_table[( crc & 0xFF ) ^ static_cast<unsigned char>( *dataPointer )] ) : crc;
    }

    //=====================================
    //  crc32
    //      Conmpile time CRC32
    //
    //      Parameters:
    //          data: input string to hash pointer
    //          length: length of 'data' (in bytes)
    //      
    //      Return:
    //          Hash of the data
    //=====================================
    constexpr u32 crc32( const char* data, size_t length )
    {
        return ~crc32_impl( data, length );
    }

    constexpr u32 crc32( const wchar_t* data, size_t length )
    {
        return ~crc32_impl( data, length );
    }

    //=====================================
    //  strlen_c
    //      Conmpile time strlen implementation
    //
    //      Parameters:
    //          str: input string pointer
    //      
    //      Return:
    //          Length of the string (null terminator included)
    //=====================================
    constexpr size_t strlen_c( const char* str )
    {
        return ( *str != '\0' ) ? 1 + strlen_c( str + 1 ) : 0;
    }
    
    constexpr size_t strlen_c( const wchar_t* str )
    {
        return ( *str != '\0' ) ? 1 + strlen_c( str + 1 ) : 0;
    }
}

namespace dk
{
    namespace core
    {
        constexpr u32 CompileTimeCRC32( const char* str )
        {
            return crc32( str, strlen_c( str ) );
        }

        constexpr u32 CompileTimeCRC32( const wchar_t* str )
        {
            return crc32( str, strlen_c( str ) );
        }
    }
}
