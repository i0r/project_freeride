/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "FileSystemObjectArchive.h"

FileSystemObjectArchive::FileSystemObjectArchive( const dkString_t& objectPath, u8* objectAllocatedDataBuffer, u64 dataBufferSize )
    : dataBuffer( objectAllocatedDataBuffer )
    , bufferSize( dataBufferSize )
    , isFileOpen( false )
    , openedMode( eFileOpenMode::FILE_OPEN_MODE_NONE )
    , readOffset( 0 )
{
    nativeObjectPath = objectPath;
    fileHashcode = DUSK_STRING_HASH( nativeObjectPath.c_str() );
}

FileSystemObjectArchive::~FileSystemObjectArchive()
{
    // Data buffer is owned by the filesystem; not the object (it should be more obvious in a future refactor).
    dataBuffer = nullptr;
    nativeObjectPath.clear();
    close();
}

void FileSystemObjectArchive::open( const int32_t mode )
{
    isFileOpen = true;
    openedMode = mode;

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_START_FROM_END ) == eFileOpenMode::FILE_OPEN_MODE_START_FROM_END ) {
        readOffset = ( bufferSize - 1 );
    }
}

void FileSystemObjectArchive::close()
{
    isFileOpen = false;
    openedMode = eFileOpenMode::FILE_OPEN_MODE_NONE;
    readOffset = 0;
}

bool FileSystemObjectArchive::isGood()
{
    return ( readOffset < bufferSize );
}

bool FileSystemObjectArchive::isOpen()
{
    return isFileOpen;
}

u64 FileSystemObjectArchive::tell()
{
    return readOffset;
}

u64 FileSystemObjectArchive::getSize()
{
    return bufferSize;
}

void FileSystemObjectArchive::read( u8* buffer, u64 size )
{
    memcpy( buffer, dataBuffer + readOffset, size );
    readOffset += size;
}

void FileSystemObjectArchive::skip( const u64 byteCountToSkip )
{
    readOffset += byteCountToSkip;
}

void FileSystemObjectArchive::seek( const u64 byteCount, const eFileReadDirection direction )
{
    switch ( direction ) {
    case eFileReadDirection::FILE_READ_DIRECTION_BEGIN:
        readOffset = byteCount;
        break;
    case eFileReadDirection::FILE_READ_DIRECTION_CURRENT:
        readOffset += byteCount;
        break;
    case eFileReadDirection::FILE_READ_DIRECTION_END:
        readOffset -= byteCount;
        break;
    }
}
