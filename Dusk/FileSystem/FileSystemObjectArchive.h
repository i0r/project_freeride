/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "FileSystemObject.h"

class FileSystemObjectArchive final : public FileSystemObject
{
public:
                        FileSystemObjectArchive( const dkString_t& objectPath, u8* objectAllocatedDataBuffer, u64 dataBufferSize );
                        ~FileSystemObjectArchive();

    virtual void        open( const int32_t mode ) override;
    virtual void        close() override;
    virtual bool        isOpen() override;
    virtual bool        isGood() override;
    virtual u64         tell() override;
    virtual u64         getSize() override;
    virtual void        read( u8* buffer, const u64 size ) override;
    virtual void        write( u8* buffer, const u64 size ) override {}
    virtual void        writeString( const std::string& string ) override {}
    virtual void        writeString( const char* string, const size_t length ) override {}
    virtual void        skip( const u64 byteCountToSkip ) override;
    virtual void        seek( const u64 byteCount, const eFileReadDirection direction ) override;

private:
    u8*            dataBuffer;
    u64            bufferSize;
    bool           isFileOpen;
    i32            openedMode;
    u64            readOffset;
};