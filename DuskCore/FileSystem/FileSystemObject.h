/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/Types.h>
#include "FileSystem.h"

// Abstract object (file, directory, etc.)
class FileSystemObject
{
public:
    inline dkStringHash_t   getHashcode() const { return fileHashcode; }
    inline dkString_t       getFilename() const { return nativeObjectPath; }
    inline void             writePadding()
    {
        static constexpr uint8_t PADDING = 0xFF;

        auto streamPos = tell();
        while ( streamPos % 16 != 0 ) {
            write( ( uint8_t* )&PADDING, sizeof( char ) );
            streamPos++;
        }
    }

public:
    template<typename T>    void write( const T& variable ) { write( (uint8_t*)&variable, sizeof( T ) ); }
    template<typename T>    void read( const T& variable ) { read( (uint8_t*)&variable, sizeof( T ) ); }

public:
    virtual                 ~FileSystemObject() { }
    virtual void            open( const int32_t mode ) = 0;
    virtual void            close() = 0;
    virtual bool            isOpen() = 0;
    virtual bool            isGood() = 0;
    virtual uint64_t        tell() = 0;
    virtual uint64_t        getSize() = 0;
    virtual void            read( uint8_t* buffer, const uint64_t size ) = 0;
    virtual void            write( uint8_t* buffer, const uint64_t size ) = 0;
    virtual void            writeString( const std::string& string ) = 0;
    virtual void            writeString( const char* string, const std::size_t length ) = 0;
    virtual void            skip( const uint64_t byteCountToSkip ) = 0;
    virtual void            seek( const uint64_t byteCount, const eFileReadDirection direction ) = 0;

protected:
    dkStringHash_t          fileHashcode;
    dkString_t              nativeObjectPath;
};
