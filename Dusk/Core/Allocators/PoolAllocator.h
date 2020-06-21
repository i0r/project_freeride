/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BaseAllocator.h"

class PoolAllocator final : public BaseAllocator
{
public:
                    PoolAllocator( const size_t objectSize, const u8 objectAlignment, const size_t size, void* baseAddress );
                    PoolAllocator( PoolAllocator& ) = delete;
                    PoolAllocator& operator = ( PoolAllocator& ) = delete;
                    ~PoolAllocator();

    void*           allocate( const size_t allocationSize, const u8 alignment = 4 ) override;
    void            free( void* pointer ) override;
    void            clear();

private:
    const size_t    objectSize;
    const u8        objectAlignment;
    void**          freeList;
};
