/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "GrowingStackAllocator.h"

#include "AllocationHelpers.h"

inline size_t RoundUpToMultiple( const size_t value, const size_t multiple )
{
    return ( value + multiple - 1 ) & ~( multiple - 1 );
}

GrowingStackAllocator::GrowingStackAllocator( const size_t maxSize, void* baseVirtualAddress, const size_t pageSize )
    : BaseAllocator( maxSize, baseVirtualAddress )
    , pageSize( pageSize )
    , endVirtualAddress( static_cast<void*>( (u8*)baseVirtualAddress + maxSize ) )
    , currentPosition( baseVirtualAddress )
    , currentEndPosition( baseVirtualAddress )
    , previousPosition( nullptr )
{

}

GrowingStackAllocator::~GrowingStackAllocator()
{
    currentPosition = nullptr;
    previousPosition = nullptr;
}

void* GrowingStackAllocator::allocate( const size_t allocationSize, const u8 alignment )
{
    const u8 adjustment = dk::core::AlignForwardAdjustmentWithHeader( currentPosition, alignment, sizeof( AllocationHeader ) );

    if ( memoryUsage + adjustment + allocationSize > memorySize ) {
        return nullptr;
    }

    u8* allocatedAddress = static_cast< u8* >( currentPosition ) + adjustment;

    // If we run out of pages, commit previously reserved pages
    if ( ( allocatedAddress + allocationSize ) > currentEndPosition ) {
        const size_t neededPhysicalSize = RoundUpToMultiple( adjustment + allocationSize, pageSize );

        // If we dont have any page left, return null and enjoy your crash!
        if ( ( static_cast<u8*>( currentEndPosition ) + neededPhysicalSize ) > endVirtualAddress ) {
            return nullptr;
        }

        dk::core::VirtualAlloc( neededPhysicalSize, currentEndPosition );
        *(size_t*)currentEndPosition += neededPhysicalSize;
    }

    AllocationHeader* header = ( AllocationHeader* )( allocatedAddress - sizeof( AllocationHeader ) );
    header->adjustment = adjustment;
    header->previousAllocation = previousPosition;
    previousPosition = allocatedAddress;

    currentPosition = static_cast< void* >( allocatedAddress + allocationSize );

    memoryUsage += ( allocationSize + adjustment );
    allocationCount++;

    return static_cast< void* >( allocatedAddress );
}

void GrowingStackAllocator::free( void* pointer )
{
    u8* memoryPointer = static_cast< u8* >( pointer );

    // Retrieve Header
    AllocationHeader* header = ( AllocationHeader* )( memoryPointer - sizeof( AllocationHeader ) );

    memoryUsage -= ( u8* )currentPosition - ( u8* )pointer + header->adjustment;
    allocationCount--;

    currentPosition = memoryPointer - header->adjustment;
    previousPosition = header->previousAllocation;
}

void GrowingStackAllocator::clear()
{

}
