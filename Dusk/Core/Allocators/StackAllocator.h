/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BaseAllocator.h"

class StackAllocator final : public BaseAllocator
{
public:
            StackAllocator( const size_t size, void* baseAddress );
            StackAllocator( StackAllocator& ) = delete;
            StackAllocator& operator = ( StackAllocator& ) = delete;
            ~StackAllocator();

    void*   allocate( const size_t allocationSize, const u8 alignment = 4 ) override;
    void    free( void* pointer ) override;

private:
    struct AllocationHeader {
        void* previousAllocation;
        u8 adjustment;
    };

private:
    void*   currentPosition;
    void*   previousPosition;
};
