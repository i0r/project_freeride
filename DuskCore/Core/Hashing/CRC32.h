/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <stdint.h>
#include <string>

namespace
{
    template<typename T>
    static u32 Core_CRC32Impl( const T* string )
    {
        u32 hashcode = ~0u;

        for ( const T* character = string; *character; ++character ) {
            hashcode ^= static_cast<u32>( *character );

            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
        }

        return ~hashcode;
    }

    template<typename T>
    static u32 Core_CRC32Impl( const T* data, const size_t length )
    {
        u32 hashcode = ~0u;

        for ( size_t i = 0; i < length; i++ ) {
            const T character = data[i];

            hashcode ^= static_cast<u32>( character );

            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
            hashcode = hashcode & 1 ? ( hashcode >> 1 ) ^ 0x82f63b78 : hashcode >> 1;
        }

        return ~hashcode;
    }
}

namespace dk
{
    namespace core
    {
        static inline u32 CRC32( const u8* data, const size_t length )
        {
            return Core_CRC32Impl( data, length );
        }

        static inline u32 CRC32( const i8* data, const size_t length )
        {
            return Core_CRC32Impl( data, length );
        }

        static inline u32 CRC32( const char* data, const size_t length )
        {
            return Core_CRC32Impl( data, length );
        }

        static inline u32 CRC32( const u8* data )
        {
            return Core_CRC32Impl( data );
        }

        static inline u32 CRC32( const i8* data )
        {
            return Core_CRC32Impl( data );
        }

        static inline u32 CRC32( const std::string& string )
        {
            return Core_CRC32Impl( string.c_str() );
        }

        static inline u32 CRC32( const std::wstring& string )
        {
            return Core_CRC32Impl( string.c_str() );
        }
    }
}
