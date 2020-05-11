/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_WIN
namespace dk
{
    namespace core
    {
        static DUSK_INLINE void* malloc( const size_t size )
        {
            return ::malloc( size );
        }

        static DUSK_INLINE void* realloc( void* block, const size_t size )
        {
            return ::realloc( block, size );
        }

        static DUSK_INLINE void free( void* block )
        {
            ::free( block );
        }

#if DUSK_MSVC
        static DUSK_INLINE void* AlignedMalloc( const size_t size, const u8 alignment )
        {
            return ::_aligned_malloc( size, alignment );
        }

        static DUSK_INLINE void* AlignedRealloc( void* block, const size_t size, const u8 alignment )
        {
            return ::_aligned_realloc( block, size, alignment );
        }

        static DUSK_INLINE void FreeAligned( void* block )
        {
            ::_aligned_free( block );
        }
#endif

        static DUSK_INLINE void* ReserveAddressSpace( const size_t reservationSize, void* startAddress = nullptr )
        {
            return ::VirtualAlloc( startAddress, reservationSize, ( MEM_RESERVE | MEM_TOP_DOWN ), PAGE_NOACCESS );
        }

        static DUSK_INLINE void ReleaseAddressSpace( void* address )
        {
            ::VirtualFree( address, 0, MEM_RELEASE );
        }

        static DUSK_INLINE void* VirtualAlloc( const size_t allocationSize, void* reservedAddressSpace )
        {
            return ::VirtualAlloc( reservedAddressSpace, allocationSize, MEM_COMMIT, PAGE_READWRITE );
        }

        static DUSK_INLINE void VirtualFree( void* allocatedAddress )
        {
            ::VirtualFree( allocatedAddress, 0, MEM_DECOMMIT );
        }

        static size_t GetPageSize()
        {
            SYSTEM_INFO systemInfos = {};
            GetSystemInfo( &systemInfos );

            return systemInfos.dwAllocationGranularity;
        }

        static DUSK_INLINE void* PageAlloc( const size_t size )
        {
            return ::VirtualAlloc( nullptr, size, MEM_COMMIT, PAGE_READWRITE );
        }
    }
}
#endif
