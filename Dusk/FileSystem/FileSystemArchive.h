/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "FileSystem.h"
#include "FileSystemObjectArchive.h"
#include "ThirdParty/miniz/miniz.h"

#include <unordered_map>
#include <list>
#include <tuple>

class BaseAllocator;

class FileSystemArchive final : public FileSystem
{
public:
                                FileSystemArchive( BaseAllocator* allocator, const dkString_t& archiveFilename );
                                FileSystemArchive( FileSystemArchive& ) = delete;
                                ~FileSystemArchive();

    virtual FileSystemObject*   openFile( const dkString_t& filename, const int32_t mode = eFileOpenMode::FILE_OPEN_MODE_READ ) override;
    virtual void                closeFile( FileSystemObject* fileSystemObject ) override;
    virtual void                createFolder( const dkString_t& folderName ) override;
    virtual bool                fileExists( const dkString_t& filename ) override;
    virtual bool                isReadOnly() override { return true; }
    virtual dkString_t          resolveFilename( const dkString_t& mountPoint, const dkString_t& filename );

private:
    using ArchiveFileEntry = std::tuple<uint64_t, uint32_t>;

private:
    dkString_t                                              archiveName;
    mz_zip_archive*                                         nativeZipObject;
    std::unordered_map<dkStringHash_t, ArchiveFileEntry>    fileList;
    std::list<FileSystemObjectArchive*>                     openedFiles;
    BaseAllocator*                                          memoryAllocator;

private:
    void                        retrieveFileSystemFileList();
};
