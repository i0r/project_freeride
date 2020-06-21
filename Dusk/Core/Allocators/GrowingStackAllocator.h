/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BaseAllocator.h"

class GrowingStackAllocator final : public BaseAllocator
{
public:
            GrowingStackAllocator( const size_t maxSize, void* baseVirtualAddress, const size_t pageSize = 4096 );
            GrowingStackAllocator( GrowingStackAllocator& ) = delete;
            GrowingStackAllocator& operator = ( GrowingStackAllocator& ) = delete;
            ~GrowingStackAllocator();

    void*   allocate( const size_t allocationSize, const u8 alignment = 4 ) override;
    void    free( void* pointer ) override;
    void    clear();

private:
    struct AllocationHeader {
        void* previousAllocation;
        u8 adjustment;
    };

private:
    size_t pageSize;
    void*       endVirtualAddress;

    void*   currentPosition;
    void*   currentEndPosition;
    void*   previousPosition;
};
