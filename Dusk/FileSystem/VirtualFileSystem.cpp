/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "VirtualFileSystem.h"

#include "FileSystem.h"

VirtualFileSystem::VirtualFileSystem()
{

}

VirtualFileSystem::~VirtualFileSystem()
{
    fileSystemEntries.clear();
}

void VirtualFileSystem::mount( FileSystem* media, const dkString_t& mountPoint, const uint64_t mountOrder )
{
    fileSystemEntries.push_back( { media, mountOrder, mountPoint } );

    fileSystemEntries.sort( [=]( FileSystemEntry& lEntry, FileSystemEntry& rEntry ) {
        return lEntry.MountPoint.length() >= rEntry.MountPoint.length() && lEntry.MountOrder <= rEntry.MountOrder;
    } );
}

void VirtualFileSystem::unmount( FileSystem* media )
{
    fileSystemEntries.remove_if( [=]( const FileSystemEntry& entry ) { return entry.Media == media; } );
}

FileSystemObject* VirtualFileSystem::openFile( const dkString_t& filename, const int32_t mode )
{
    for ( auto& fileSystemEntry : fileSystemEntries ) {
        if ( filename.compare( 0, fileSystemEntry.MountPoint.length(), fileSystemEntry.MountPoint ) == 0 ) {
            auto absoluteFilename = fileSystemEntry.Media->resolveFilename( fileSystemEntry.MountPoint, filename );
            auto openedFile = fileSystemEntry.Media->openFile( absoluteFilename, mode );

            if ( openedFile != nullptr ) {
                return openedFile;
            }
        }
    }

    return nullptr;
}

bool VirtualFileSystem::fileExists( const dkString_t& filename )
{
    for ( auto& fileSystemEntry : fileSystemEntries ) {
        if ( filename.compare( 0, fileSystemEntry.MountPoint.length(), fileSystemEntry.MountPoint ) == 0 ) {
            auto absoluteFilename = fileSystemEntry.Media->resolveFilename( fileSystemEntry.MountPoint, filename );
            auto fileExists = fileSystemEntry.Media->fileExists( absoluteFilename );

            if ( fileExists ) {
                return true;
            }
        }
    }

    return false;
}
