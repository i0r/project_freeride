/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <string>
#include <string.h>
#include <algorithm>

namespace dk
{
    namespace core
    {
        DUSK_INLINE bool ExpectKeyword( const char* str, const size_t keywordLength, const char* keyword )
        {
            return ( strncmp( str, keyword, keywordLength ) == 0 );
        }

        static DUSK_INLINE bool StringToBool( const std::string& str )
        {
            std::string lStr = str;
            std::transform( lStr.begin(), lStr.end(), lStr.begin(), ::tolower );

            return lStr == "true" || lStr == "1";
        }

        static void TrimString( dkString_t& str )
        {
            size_t charPos = str.find_first_not_of( DUSK_STRING( "     \n" ) );

            if ( charPos != dkString_t::npos ) {
                str.erase( 0, charPos );
            }

            charPos = str.find_last_not_of( DUSK_STRING( "     \n" ) );

            if ( charPos != dkString_t::npos ) {
                str.erase( charPos + 1 );
            }

            // Remove carriage return
            str.erase( std::remove( str.begin(), str.end(), '\r' ), str.end() );
            str.erase( std::remove( str.begin(), str.end(), '\t' ), str.end() );
        }

        static void RemoveNullCharacters( std::string& str )
        {
            str.erase( std::remove( str.begin(), str.end(), '\0' ), str.end() );
        }

        static bool ContainsString( const dkString_t& string, const dkString_t& containedString )
        {
            return string.find( containedString ) != dkString_t::npos;
        }

#if DUSK_UNICODE
        static void TrimString( std::string& str )
        {
            size_t charPos = str.find_first_not_of( "     \n" );

            if ( charPos != std::string::npos ) {
                str.erase( 0, charPos );
            }

            charPos = str.find_last_not_of( "     \n" );

            if ( charPos != std::string::npos ) {
                str.erase( charPos + 1 );
            }

            // Remove carriage return
            str.erase( std::remove( str.begin(), str.end(), '\r' ), str.end() );
            str.erase( std::remove( str.begin(), str.end(), '\t' ), str.end() );
        }
#endif

        static void RemoveWordFromString( dkString_t& string, const dkString_t& word, const dkString_t& newWord = DUSK_STRING( "" ) )
        {
            std::size_t wordIndex = 0;

            while ( ( wordIndex = string.find( word ) ) != dkString_t::npos ) {
                string.replace( wordIndex, word.size(), newWord );
            }
        }

        static void ReplaceWord( std::string& string, const std::string& wordToReplace, const std::string& newWord )
        {
            std::size_t wordIndex = 0;

            while ( ( wordIndex = string.find( wordToReplace, wordIndex + 1 ) ) != dkString_t::npos ) {
                string.replace( wordIndex, wordToReplace.size(), newWord );
            }
        }

        DUSK_INLINE void StringToLower( dkString_t& string )
        {
            std::transform( string.begin(), string.end(), string.begin(), ::tolower );
        }

#if DUSK_UNICODE
        DUSK_INLINE void StringToLower( std::string& string )
        {
            std::transform( string.begin(), string.end(), string.begin(), ::tolower );
        }
#endif

        DUSK_INLINE void StringToUpper( dkString_t& string )
        {
            std::transform( string.begin(), string.end(), string.begin(), ::toupper );
        }

        static dkString_t GetFileExtensionFromPath( const dkString_t& path )
        {
            const size_t dotPosition = path.find_last_of( '.' );

            if ( dotPosition != dkString_t::npos ) {
                return path.substr( dotPosition + 1, path.length() - dotPosition );
            }

            return DUSK_STRING( "" );
        }

        DUSK_INLINE void SanitizeFilepathSlashes( dkString_t& string )
        {
#if DUSK_WIN
            size_t backslashOffset = dkString_t::npos;
            while ( ( backslashOffset = string.find_first_of( '\\' ) ) != dkString_t::npos ) {
                string.replace( backslashOffset, 1, 1, '/' );
            }
#endif
        }

        static dkString_t GetFilenameWithExtensionFromPath( const dkString_t& path )
        {
            dkString_t sanitizedPath = path;
            SanitizeFilepathSlashes( sanitizedPath );

            const size_t lastSlashPosition = sanitizedPath.find_last_of( '/' );
            if ( lastSlashPosition != dkString_t::npos ) {
                return sanitizedPath.substr( lastSlashPosition + 1 );
            }

            return DUSK_STRING( "" );
        }

        static dkString_t GetFilenameOnlyFromPath( const dkString_t& path )
        {
            dkString_t filenameWithExtension = GetFilenameWithExtensionFromPath( path );
            
            const size_t lastDotPosition = filenameWithExtension.find_last_of( '.' );
            if ( lastDotPosition != dkString_t::npos ) {
                return filenameWithExtension.substr( 0, lastDotPosition );
            }

            return DUSK_STRING( "" );
        }

        DUSK_INLINE static bool IsEndOfLine( const char c )
        {
            return ( ( c == '\n' ) || ( c == '\r' ) );
        }

        DUSK_INLINE static bool IsWhitespace( const char c )
        {
            return ( ( c == ' ' ) || ( c == '\t' ) || ( c == '\v' ) || ( c == '\f' ) || IsEndOfLine( c ) );
        }

        DUSK_INLINE static bool IsAlpha( const char c )
        {
            return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
        }

        DUSK_INLINE static bool IsNumber( const char c )
        {
            return ( c >= '0' && c <= '9' );
        }
    }
}

#include <locale>
#include <codecvt>

#if DUSK_UNICODE
DUSK_INLINE std::string WideStringToString( const dkString_t& str )
{
    std::wstring_convert<::std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes( str );
}

DUSK_INLINE std::wstring StringToWideString( const std::string& str )
{
    std::wstring wstr;
    wstr.assign( str.begin(), str.end() );
    return wstr;
}
#else
DUSK_INLINE std::string WideStringToString( const dkString_t& str )
{
    return str;
}

DUSK_INLINE std::wstring StringToWideString( const std::string& str )
{
    std::wstring wstr;
    wstr.assign( str.begin(), str.end() );
    return wstr;
}
#endif
