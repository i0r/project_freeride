/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/StringHelpers.h>
#include <Maths/Vector.h>

namespace dk
{
    namespace io
    {
        static void LoadTextFile( FileSystemObject* stream, std::string& string )
        {
            const u64 contentLength = stream->getSize();
            string.resize( contentLength, '\0' );

            stream->read( (uint8_t*)string.data(), contentLength * sizeof( char ) );
        }

        static void ReadString( FileSystemObject* file, dkString_t& string )
        {
            string.clear();

            uint8_t character = '\0';
            file->read( (uint8_t*)&character, sizeof( uint8_t ) );

            while ( file->isGood() && character != '\n' && character != '\r' && character != '\0' && character != 0xFF ) {
                string += character;
                file->read( (uint8_t*)&character, sizeof( uint8_t ) );
            }
        }

        static dkString_t WrappedStringToString( const dkString_t& wrappedString )
        {
            size_t stringStart = wrappedString.find_first_of( '"' );
            size_t stringEnd = wrappedString.find_first_of( '"', stringStart + 1 );

            if ( stringStart == dkString_t::npos || stringEnd == dkString_t::npos ) {
                return DUSK_STRING( "" );
            }

            u64 stringLength = stringEnd - ( stringStart + 1 );
            return wrappedString.substr( ( stringStart + 1 ), stringLength );
        }

        static dkString_t TyplessToString( const dkString_t& typelessString )
        {
            dkString_t lowerCaseString = typelessString;
            dk::core::StringToLower( lowerCaseString );

            if ( lowerCaseString == DUSK_STRING( "none" ) ) {
                return DUSK_STRING( "" );
            }

            dkString_t unwrappedString = WrappedStringToString( typelessString );
            if ( !unwrappedString.empty() ) {
                return unwrappedString;
            }

            return typelessString;
        }

        static bool StringToBoolean( const dkString_t& string )
        {
            dkString_t lowerCaseString = string;
            dk::core::StringToLower( lowerCaseString );

            return ( lowerCaseString == DUSK_STRING( "1" ) || lowerCaseString == DUSK_STRING( "true" ) );
        }

        static dkVec2u StringTo2DVectorU( const dkString_t& str )
        {
            if ( str.front() != '{' || str.back() != '}' ) {
                return dkVec2u();
            }

            dkVec2u vec = {};

            std::size_t offsetX = str.find_first_of( ',' ),
                offsetY = str.find_last_of( ',' ),

                vecEnd = str.find_last_of( '}' );

            auto vecX = str.substr( 1, offsetX - 1 );
            auto vecY = str.substr( offsetX + 1, vecEnd - offsetY - 1 );

            vec.x = static_cast<u32>( std::stoi( vecX ) );
            vec.y = static_cast<u32>( std::stoi( vecY ) );

            return vec;
        }

        static dkVec2f StringTo2DVector( const dkString_t& str )
        {
            if ( str.front() != '{' || str.back() != '}' ) {
                return dkVec2f();
            }

            dkVec2f vec = {};

            std::size_t offsetX = str.find_first_of( ',' ),
                offsetY = str.find_last_of( ',' ),

                vecEnd = str.find_last_of( '}' );

            dkString_t vecX = str.substr( 1, offsetX - 1 );
            dkString_t vecY = str.substr( offsetX + 1, vecEnd - offsetY - 1 );

            vec.x = std::stof( vecX );
            vec.y = std::stof( vecY );

            return vec;
        }

        static dkVec3f StringTo3DVector( const dkString_t& str )
        {
            if ( str.size() <= 2 || str.front() != '{' || str.back() != '}' ) {
                return dkVec3f();
            }

            dkVec3f vec = {};

            std::size_t offsetX = str.find_first_of( ',' ),
                offsetY = str.find_first_of( ',', offsetX + 1 ),
                offsetZ = str.find_last_of( ',' ),

                vecEnd = str.find_last_of( '}' );

            dkString_t vecX = str.substr( 1, offsetX - 1 );
            dkString_t vecY = str.substr( offsetX + 1, offsetY - offsetX - 1 );
            dkString_t vecZ = str.substr( offsetY + 1, vecEnd - offsetZ - 1 );

            vec.x = std::stof( vecX.c_str() );
            vec.y = std::stof( vecY.c_str() );
            vec.z = std::stof( vecZ.c_str() );

            return vec;
        }
    }
}
