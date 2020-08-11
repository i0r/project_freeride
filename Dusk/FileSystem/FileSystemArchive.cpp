/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "FileSystemArchive.h"

#include "Core/StringHelpers.h"

FileSystemArchive::FileSystemArchive( BaseAllocator* allocator, const dkString_t& archiveFilename )
    : archiveName( archiveFilename )
    , nativeZipObject( dk::core::allocate<mz_zip_archive>( allocator ) )
    , memoryAllocator( allocator )
{
    auto zipName = WideStringToString( archiveName );

    bool headerReadResult = mz_zip_reader_init_file( nativeZipObject, zipName.c_str(), 0 );
    DUSK_ASSERT( headerReadResult, "Failed to read archive '%s'\n", zipName.c_str() );

    retrieveFileSystemFileList();
}

FileSystemArchive::~FileSystemArchive()
{
    archiveName.clear();
}

FileSystemObject* FileSystemArchive::openFile( const dkString_t& filename, const int32_t mode )
{
    const bool useWriteMode = ( ( mode & eFileOpenMode::FILE_OPEN_MODE_WRITE ) == eFileOpenMode::FILE_OPEN_MODE_WRITE );

    // Archives are read-only (no matter the archive)
    if ( !fileExists( filename ) || useWriteMode ) {
        return nullptr;
    }

    // Check if the file has already been opened
    dkStringHash_t fileHashcode = DUSK_STRING_HASH( filename.c_str() );
    for ( FileSystemObjectArchive* openedFile : openedFiles ) {
        if ( openedFile->getHashcode() == fileHashcode && !openedFile->isOpen() ) {
            openedFile->open( mode );
            return openedFile;
        }
    }

    // If the file has never been opened, open it
    ArchiveFileEntry fileInfos = fileList[fileHashcode];

    auto fileInternalIndex = std::get<1>( fileInfos );
    auto fileSize = std::get<0>( fileInfos );
    
    // TODO Clean memory allocation (allocate a memory chunk per filesystem?)
    u8* fileBuffer = dk::core::allocateArray<u8>( memoryAllocator, fileSize );
    bool operationResult = mz_zip_reader_extract_to_mem( nativeZipObject, fileInternalIndex, fileBuffer, fileSize, static_cast<mz_uint>( 0 ) );

    if ( !operationResult ) {
        DUSK_LOG_ERROR( "Failed to read file '%s' (file extraction failed)\n", filename.c_str() );
        return nullptr;
    }

    openedFiles.push_back( dk::core::allocate<FileSystemObjectArchive>( memoryAllocator, filename, fileBuffer, fileSize ) );

    FileSystemObjectArchive* openedFile = openedFiles.back();
    openedFile->open( mode );

    return openedFile;
}

void FileSystemArchive::closeFile( FileSystemObject* fileSystemObject )
{
    if ( fileSystemObject == nullptr ) {
        return;
    }

    fileSystemObject->close();

    //openedFiles.remove( fileSystemObject );
}

void FileSystemArchive::createFolder( const dkString_t& folderName )
{
    DUSK_RAISE_FATAL_ERROR( false, "Invalid API Usage! (filesystem is read only)" );
}

bool FileSystemArchive::fileExists( const dkString_t& filename )
{
    const dkStringHash_t filenameHashcode = DUSK_STRING_HASH( filename.c_str() );
    auto iterator = fileList.find( filenameHashcode );

    return iterator != fileList.end();
}

dkString_t FileSystemArchive::resolveFilename( const dkString_t& mountPoint, const dkString_t& filename )
{
    dkString_t resolvedFilename( filename );
    return resolvedFilename.replace( resolvedFilename.begin(), resolvedFilename.begin() + mountPoint.length(), DUSK_STRING( "" ) );
}

void FileSystemArchive::retrieveFileSystemFileList()
{
    fileList.clear();

    i32 fileCount = mz_zip_reader_get_num_files( nativeZipObject );
    DUSK_LOG_INFO( "'%s': found %u files\n", archiveName.c_str(), fileCount );

    for ( i32 i = 0; i < fileCount; i++ ) {
        mz_zip_archive_file_stat file_stat;

        if ( !mz_zip_reader_file_stat( nativeZipObject, i, &file_stat ) ) {
            DUSK_LOG_WARN( "'%s': cannot read file entry %i/%ull\n", archiveName.c_str(), i, fileList.size() );
            continue;
        }

        fileList.emplace( DUSK_STRING_HASH( file_stat.m_filename ), std::make_tuple( file_stat.m_uncomp_size, file_stat.m_file_index ) );
    }
}
