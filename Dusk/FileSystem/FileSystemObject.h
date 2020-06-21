/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <atomic>
#include "FileSystem.h"

// Abstract object (file, directory, etc.)
class FileSystemObject
{
public:
    // Return the filename hashcode of this file.
    // Note that this is not the hashcode of this file content!
    DUSK_INLINE dkStringHash_t getHashcode() const { 
        return fileHashcode; 
    }

    // Return the filename.
    DUSK_INLINE dkString_t getFilename() const { 
        return nativeObjectPath; 
    }

    // Write binary padding to guarantee memory alignment of a file content being written.
    DUSK_INLINE void writeMemoryAlignmentPadding( const u64 alignmentInBytes = 16ull, const u8 paddingByte = 0xff ) {
        u64 streamPos = tell();
        while ( streamPos % alignmentInBytes != 0 ) {
            write( ( uint8_t* )&paddingByte, sizeof( u8 ) );
            streamPos++;
        }
    }

public:
    // Wrapped write call to allow templated write calls.
    template<typename T>    
    void write( const T& variable ) { 
        write( (uint8_t*)&variable, sizeof( T ) ); 
    }

    // Wrapped read call to allow templated write calls.
    template<typename T>    
    void read( const T& variable ) { 
        read( (uint8_t*)&variable, sizeof( T ) ); 
    }

public:
    FileSystemObject() 
        : fileHashcode( 0 )
        , nativeObjectPath( DUSK_STRING( "" ) )
        , fileOwnership( false ) 
    {
    }

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

public:
    // Acquire the ownership of this file. This operation is required in multithreaded context in order to guarantee
    // that two file won't try to access this object at the same time.
    void acquireOwnership()
    {
        // Spin until the ownership of this file has been released.
        while ( !canAcquireOwnership() );
    }

    // Release the ownership of this file. Must be called as soon as the thread has finished its operations on this object.
    void releaseOwnership()
    {
        fileOwnership.store( false, std::memory_order_release );
    }

protected:
    dkStringHash_t          fileHashcode;
    dkString_t              nativeObjectPath;

private:
    std::atomic<bool>       fileOwnership;

private:
    // Return true if this file is not in use by another thread and can safely be used by the caller; false otherwise.
    DUSK_INLINE bool canAcquireOwnership()
    {
        bool expected = false;
        return fileOwnership.compare_exchange_weak( expected, true, std::memory_order_acquire );
    }
};
