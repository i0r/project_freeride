/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_UNIX

#include <stdlib.h>
#include <cstdint>
#include <sys/mman.h>

namespace dk
{
    namespace core
    {
        static void* malloc( const size_t size )
        {
            return ::malloc( size );
        }

        static void* realloc( void* block, const size_t size )
        {
            return ::realloc( block, size );
        }

        static void free( void* block )
        {
            ::free( block );
        }

#if DUSK_GCC
        static void* AlignedMalloc( const size_t size, const u8 alignment )
        {
            return ::aligned_alloc( size, alignment );
        }

        static void* AlignedRealloc( void* block, const size_t size, const u8 alignment )
        {
            void* reallocatedBlock = realloc( block, size );
            ::posix_memalign( &reallocatedBlock, size, alignment );

            return reallocatedBlock;
        }

        static void FreeAligned( void* block )
        {
            ::free( block );
        }
#endif

        static void* ReserveAddressSpace( const size_t reservationSize, void* startAddress = nullptr )
        {
            return ::mmap( startAddress, reservationSize, PROT_NONE, MAP_SHARED | MAP_GROWSDOWN, -1, 0 );
        }

        static void ReleaseAddressSpace( void* address )
        {
            // TODO munmap needs to know the size of the allocation...
            // ::munmap( address, 0  );
        }

        static void* VirtualAlloc( const size_t allocationSize, void* reservedAddressSpace )
        {
            return nullptr;
        }

        static void VirtualFree( void* allocatedAddress )
        {

        }

        static size_t GetPageSize()
        {
            return 0ull;
        }

        static void* PageAlloc( const size_t size )
        {
            return nullptr;
        }
    }
}
#endif
