/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FileSystemObjectNative.h"

#include "FileSystem.h"

#include <fstream>

FileSystemObjectNative::FileSystemObjectNative( const dkString_t& objectPath )
    : openedMode( eFileOpenMode::FILE_OPEN_MODE_NONE )
{
    nativeObjectPath = objectPath;
    fileHashcode = dk::core::CRC32( nativeObjectPath );
}

FileSystemObjectNative::~FileSystemObjectNative()
{
    close();

    nativeObjectPath = DUSK_STRING( "" );
}

void FileSystemObjectNative::open( const int32_t mode )
{
    nativeStream.open( nativeObjectPath, static_cast<std::ios_base::openmode>( mode ) );

    openedMode = mode;
}

void FileSystemObjectNative::close()
{
    nativeStream.close();

    openedMode = eFileOpenMode::FILE_OPEN_MODE_NONE;
}

bool FileSystemObjectNative::isOpen()
{
    return nativeStream.is_open();
}

bool FileSystemObjectNative::isGood()
{
    return nativeStream.good();
}

uint64_t FileSystemObjectNative::tell()
{
    return ( ( openedMode & eFileOpenMode::FILE_OPEN_MODE_READ ) == eFileOpenMode::FILE_OPEN_MODE_READ ) ? nativeStream.tellg() : nativeStream.tellp();
}

uint64_t FileSystemObjectNative::getSize()
{
    auto fileOffset = nativeStream.tellg();

    nativeStream.seekg( 0, std::ios_base::end );
    uint64_t contentLength = nativeStream.tellg();
    nativeStream.seekg( fileOffset, std::ios_base::beg );

    return contentLength;
}

void FileSystemObjectNative::read( uint8_t* buffer, const uint64_t size )
{
    nativeStream.read( (char*)buffer, size );
}

void FileSystemObjectNative::write( uint8_t* buffer, const uint64_t size )
{
    nativeStream.write( (char*)buffer, size );
}

void FileSystemObjectNative::writeString( const std::string& string )
{
    writeString( string.c_str(), string.length() );
}

void FileSystemObjectNative::writeString( const char* string, const std::size_t length )
{
    nativeStream.write( string, length );
}

void FileSystemObjectNative::skip( const uint64_t byteCountToSkip )
{
    nativeStream.ignore( byteCountToSkip );
}

void FileSystemObjectNative::seek( const uint64_t byteCount, const eFileReadDirection direction )
{
    static constexpr decltype( std::ios::beg ) FRD_TO_SEEKDIR[3] = {
        std::ios::beg,
        std::ios::cur,
        std::ios::end,
    };

    if ( ( openedMode & eFileOpenMode::FILE_OPEN_MODE_READ ) == eFileOpenMode::FILE_OPEN_MODE_READ ) {
        nativeStream.seekg( byteCount, FRD_TO_SEEKDIR[direction] );
    } else {
        nativeStream.seekp( byteCount, FRD_TO_SEEKDIR[direction] );
    }
}
