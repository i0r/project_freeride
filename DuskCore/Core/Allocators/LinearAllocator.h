/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BaseAllocator.h"

class LinearAllocator final : public BaseAllocator
{
public:
            LinearAllocator( const size_t size, void* baseAddress );
            LinearAllocator( LinearAllocator& ) = delete;
            LinearAllocator& operator = ( LinearAllocator& ) = delete;
            ~LinearAllocator();

    void*   allocate( const size_t allocationSize, const u8 alignment = 4 ) override;
    void    free( void* pointer ) override;
    void    clear();

private:
    void*   currentPosition;
};
