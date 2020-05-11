/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "LinearAllocator.h"

#include "AllocationHelpers.h"

LinearAllocator::LinearAllocator( const size_t size, void* baseAddress )
    : BaseAllocator( size, baseAddress )
    , currentPosition( baseAddress )
{

}

LinearAllocator::~LinearAllocator()
{
    currentPosition = nullptr;
}

void* LinearAllocator::allocate( const size_t allocationSize, const u8 alignment )
{
    const u8 adjustment = dk::core::AlignForwardAdjustment( currentPosition, alignment );

    if ( memoryUsage + adjustment + allocationSize > memorySize ) {
        return nullptr;
    }

    u8* allocatedAddress = static_cast< u8* >( currentPosition ) + adjustment;
    currentPosition = static_cast< void* >( allocatedAddress + allocationSize );

    memoryUsage += ( allocationSize + adjustment );
    allocationCount++;

    return static_cast< void* >( allocatedAddress );
}

void LinearAllocator::free( void* pointer )
{
    DUSK_DEV_ASSERT( true, "Bad API usage (not implemented)!\n" );
}

void LinearAllocator::clear()
{
    allocationCount = 0;
    memoryUsage = 0;
    currentPosition = baseAddress;
}
