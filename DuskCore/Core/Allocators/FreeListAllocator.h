/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BaseAllocator.h"

class FreeListAllocator final : public BaseAllocator
{
public:
            FreeListAllocator( const size_t size, void* baseAddress );
            FreeListAllocator( FreeListAllocator& ) = delete;
            FreeListAllocator& operator = ( FreeListAllocator& ) = delete;
            ~FreeListAllocator();

    void*   allocate( const size_t allocationSize, const u8 alignment = 4 ) override;
    void    free( void* pointer ) override;
    
private:
    struct AllocationHeader { 
        size_t     size;
        u8    adjustment; 
    };

    struct FreeBlock { 
        size_t size; 
        FreeBlock*  next; 
    };

private:
    FreeBlock* freeBlockList;
};
