/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "FileSystemObject.h"
#include <fstream>

class FileSystemObjectNative final : public FileSystemObject
{
public:
                        FileSystemObjectNative( const dkString_t& objectPath );
                        ~FileSystemObjectNative();

    virtual void        open( const int32_t mode ) override;
    virtual void        close() override;
    virtual bool        isOpen() override;
    virtual bool        isGood() override;
    virtual uint64_t    tell() override;
    virtual uint64_t    getSize() override;
    virtual void        read( uint8_t* buffer, const uint64_t size ) override;
    virtual void        write( uint8_t* buffer, const uint64_t size ) override;
    virtual void        writeString( const std::string& string ) override;
    virtual void        writeString( const char* string, const std::size_t length ) override;
    virtual void        skip( const uint64_t byteCountToSkip ) override;
    virtual void        seek( const uint64_t byteCount, const eFileReadDirection direction ) override;

private:
    int32_t             openedMode;
    std::fstream        nativeStream;
};
