/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <cstdint>
#include <utility>

namespace
{
    template<typename T>
    constexpr u8 GetAllocatedArrayHeaderSize()
    {
        u8 headerSize = sizeof( size_t ) / sizeof( T );

        if ( sizeof( size_t ) % sizeof( T ) > 0ull ) {
            headerSize++;
        }

        return headerSize;
    }
}

class BaseAllocator
{
public:
                        BaseAllocator( const size_t size, void* baseAddress );
    virtual             ~BaseAllocator();

    void*               getBaseAddress() const;
    size_t         getSize() const;
    size_t         getMemoryUsage() const;
    size_t         getAllocationCount() const;

    virtual void*       allocate( const size_t allocationSize, const u8 alignment = 4 ) = 0;
    virtual void        free( void* pointer ) = 0;

protected:
    void*               baseAddress;
    size_t         memorySize;

    size_t         memoryUsage;
    size_t         allocationCount;
};

namespace dk
{
    namespace core
    {
        template<typename T, typename... TArgs>
        DUSK_INLINE T* allocate( BaseAllocator* allocator, TArgs... args )
        {
            return new ( allocator->allocate( sizeof( T ), alignof( T ) ) ) T( std::forward<TArgs>( args )... );
        }

        template<typename T, typename... TArgs>
        T* allocateArray( BaseAllocator* allocator, const size_t arrayLength, TArgs... args )
        {
            constexpr u8 headerSize = GetAllocatedArrayHeaderSize<T>();

            T* allocationStart = static_cast<T*>( allocator->allocate( sizeof( T ) * ( arrayLength + headerSize ), alignof( T ) ) ) + headerSize;

            // Write array length at the beginning of the allocation
            *( reinterpret_cast<size_t*>( allocationStart ) - 1 ) = arrayLength;

            for ( size_t allocation = 0; allocation < arrayLength; allocation++ ) {
                new ( allocationStart + allocation ) T( std::forward<TArgs>( args )... );
            }

            return allocationStart;
        }

        template<typename T>
        void free( BaseAllocator* allocator, T* object )
        {
            object->~T();
            allocator->free( object );
        }

        template<typename T>
        void freeArray( BaseAllocator* allocator, T* arrayObject )
        {
            size_t arrayLength = *( reinterpret_cast<size_t*>( arrayObject ) - 1ull );

            for ( size_t allocation = 0ull; allocation < arrayLength; allocation++ ) {
                ( arrayObject + allocation )->~T();
            }

            constexpr u8 headerSize = GetAllocatedArrayHeaderSize<T>();

            allocator->free( arrayObject - headerSize );
        }
    }
}
