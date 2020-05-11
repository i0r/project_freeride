/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "PoolAllocator.h"

#include "AllocationHelpers.h"

PoolAllocator::PoolAllocator( const size_t objectSize, const u8 objectAlignment, const size_t size, void* baseAddress )
    : BaseAllocator( size, baseAddress )
    , objectSize( objectSize )
    , objectAlignment( objectAlignment )
    , freeList( nullptr )
{
    clear();
}

PoolAllocator::~PoolAllocator()
{
    freeList = nullptr;
}

void* PoolAllocator::allocate( const size_t allocationSize, const u8 alignment )
{
    void* p = freeList;
    freeList = ( void** )( *freeList );
    memoryUsage += allocationSize;
    allocationCount++;
    return p;
}

void PoolAllocator::free( void* pointer )
{
    *( ( void** )pointer ) = freeList;
    freeList = ( void** )pointer;
    memoryUsage -= objectSize;
    allocationCount--;
}

void PoolAllocator::clear()
{
    const u8 adjustment = dk::core::AlignForwardAdjustment( baseAddress, objectAlignment );

    freeList = ( void** )( ( u8* )baseAddress + adjustment );
    size_t numObjects = ( memorySize - adjustment ) / objectSize;
    void** p = freeList;

    //Initialize free blocks list 
    for ( size_t i = 0; i < numObjects - 1; i++ ) {
        *p = ( u8* )p + objectSize;
        p = ( void** )*p;
    }

    *p = nullptr;
    allocationCount = 0;
}