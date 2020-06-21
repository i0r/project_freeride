/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

enum eFileOpenMode
{
    FILE_OPEN_MODE_NONE = 0,
    FILE_OPEN_MODE_READ = 1,
    FILE_OPEN_MODE_WRITE = 2,
    FILE_OPEN_MODE_BINARY = 4,
    FILE_OPEN_MODE_APPEND = 8,
    FILE_OPEN_MODE_TRUNCATE = 16,
    FILE_OPEN_MODE_START_FROM_END = 32,
};

enum eFileReadDirection
{
    FILE_READ_DIRECTION_BEGIN = 0,
    FILE_READ_DIRECTION_CURRENT,
    FILE_READ_DIRECTION_END,
};

class FileSystemObject;

class FileSystem
{
public:
    virtual                     ~FileSystem() { };
    virtual FileSystemObject*   openFile( const dkString_t& filename, const i32 mode = FILE_OPEN_MODE_READ ) = 0;
    virtual void                closeFile( FileSystemObject* fileSystemObject ) = 0;
    virtual void                createFolder( const dkString_t& folderName ) = 0;
    virtual bool                fileExists( const dkString_t& filename ) = 0;
    virtual bool                isReadOnly() = 0;
    virtual dkString_t          resolveFilename( const dkString_t& mountPoint, const dkString_t& filename ) = 0;
};
