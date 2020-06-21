/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FileSystemNative.h"

#include "FileSystemObjectNative.h"

#if DUSK_UNIX
#include "FileSystemUnix.h"
#elif DUSK_WIN
#include "FileSystemWin32.h"
#endif

#include "Core/Hashing/CRC32.h"

FileSystemNative::FileSystemNative( const dkString_t& customWorkingDirectory )
{
    /*if ( customWorkingDirectory.empty() ) {
        RetrieveWorkingDirectory( workingDirectory );
    } else*/ 
    {
        workingDirectory = customWorkingDirectory;
    }

    if ( !workingDirectory.empty() && workingDirectory.back() != '/' ) {
        workingDirectory += '/';
    }
}

FileSystemNative::~FileSystemNative()
{
    workingDirectory.clear();

    while ( !openedFiles.empty() ) {
        auto& openedFile = openedFiles.back();
        // TODO Fucks up the CRT at closing; probably due to some race condition or poor thread sync...
        //openedFile->close();
        delete openedFile;

        openedFiles.pop_back();
    }
}

FileSystemObject* FileSystemNative::openFile( const dkString_t& filename, const int32_t mode )
{
    const bool useWriteMode = ( ( mode & eFileOpenMode::FILE_OPEN_MODE_WRITE ) == eFileOpenMode::FILE_OPEN_MODE_WRITE );

    if ( ( !fileExists( filename ) && !useWriteMode ) || ( useWriteMode && isReadOnly() ) ) {
        return nullptr;
    }

    // Build flagset
    int32_t openMode = 0;
    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_READ ) == eFileOpenMode::FILE_OPEN_MODE_READ ) {
        openMode |= std::ios::in;
    }

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_WRITE ) == eFileOpenMode::FILE_OPEN_MODE_WRITE ) {
        openMode |= std::ios::out;
    }

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_BINARY ) == eFileOpenMode::FILE_OPEN_MODE_BINARY ) {
        openMode |= std::ios::binary;
    }

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_APPEND ) == eFileOpenMode::FILE_OPEN_MODE_APPEND ) {
        openMode |= std::ios::app;
    }

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_TRUNCATE ) == eFileOpenMode::FILE_OPEN_MODE_TRUNCATE ) {
        openMode |= std::ios::trunc;
    }

    if ( ( mode & eFileOpenMode::FILE_OPEN_MODE_START_FROM_END ) == eFileOpenMode::FILE_OPEN_MODE_START_FROM_END ) {
        openMode |= std::ios::ate;
    }

    // Check if the file has already been opened
    dkStringHash_t fileHashcode = dk::core::CRC32( filename );
    for ( auto& openedFile : openedFiles ) {
        if ( openedFile->getHashcode() == fileHashcode ) {
            openedFile->open( openMode );
            return openedFile;
        }
    }

    // If the file has never been opened, open it
    openedFiles.push_back( new FileSystemObjectNative( filename ) );

    FileSystemObjectNative* openedFile = openedFiles.back();
    openedFile->open( openMode );

    return openedFile;
}

void FileSystemNative::closeFile( FileSystemObject* fileSystemObject )
{
    if ( fileSystemObject == nullptr ) {
        return;
    }

  /* openedFiles.remove_if( [=]( FileSystemObject* obj ) { 
        return obj->getHashcode() == fileSystemObject->getHashcode(); 
    } );*/

    delete fileSystemObject;
}

void FileSystemNative::createFolder( const dkString_t& folderName )
{
    dk::core::CreateFolderImpl( folderName );
}

bool FileSystemNative::fileExists( const dkString_t& filename )
{
    return dk::core::FileExistsImpl( filename );
}

bool FileSystemNative::isReadOnly()
{
    return false;
}

dkString_t FileSystemNative::resolveFilename( const dkString_t& mountPoint, const dkString_t& filename )
{
    dkString_t resolvedFilename( filename );
    return resolvedFilename.replace( resolvedFilename.begin(), resolvedFilename.begin() + mountPoint.length(), workingDirectory );
}
