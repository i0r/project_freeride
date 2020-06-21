/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "FileSystem.h"

#include <list>

class FileSystemObjectNative;

class FileSystemNative final : public FileSystem
{
public:
                                        FileSystemNative( const dkString_t& customWorkingDirectory = DUSK_STRING( "" ) );
                                        FileSystemNative( FileSystemNative& ) = delete;
                                        FileSystemNative& operator = ( FileSystemNative& ) = delete;
                                        ~FileSystemNative() override;

    virtual FileSystemObject*           openFile( const dkString_t& filename, const int32_t mode = eFileOpenMode::FILE_OPEN_MODE_READ ) override;
    virtual void                        closeFile( FileSystemObject* fileSystemObject ) override;
    virtual void                        createFolder( const dkString_t& folderName ) override;
    virtual bool                        fileExists( const dkString_t& filename ) override;
    virtual bool                        isReadOnly() override;
    virtual dkString_t                  resolveFilename( const dkString_t& mountPoint, const dkString_t& filename );

private:
    dkString_t                          workingDirectory;
    std::list<FileSystemObjectNative*>  openedFiles;
};
