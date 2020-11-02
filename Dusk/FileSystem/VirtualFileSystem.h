/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FileSystem;
class FileSystemObject;

#include <list>

class VirtualFileSystem
{
public:
                        VirtualFileSystem();
                        VirtualFileSystem( VirtualFileSystem& ) = delete;
                        VirtualFileSystem& operator = ( VirtualFileSystem& ) = delete;
                        ~VirtualFileSystem();

    void                mount( FileSystem* media, const dkString_t& mountPoint, const uint64_t mountOrder );
    void                unmount( FileSystem* media );

    FileSystemObject*   openFile( const dkString_t& filename, const int32_t mode );
    bool                fileExists( const dkString_t& filename );

private:
    struct FileSystemEntry {
        FileSystem* Media;
        uint64_t    MountOrder; // From 0 (most important fs; checked first when opening a file) to MAX_UINT64 (least important fs)
        dkString_t MountPoint;
    };

private:
    std::list<FileSystemEntry>  fileSystemEntries;

private:
    static bool SortFS( FileSystemEntry& lEntry, FileSystemEntry& rEntry );
};
