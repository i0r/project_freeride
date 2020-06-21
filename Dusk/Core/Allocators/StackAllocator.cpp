/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "StackAllocator.h"

#include "AllocationHelpers.h"

StackAllocator::StackAllocator( const size_t size, void* baseAddress )
    : BaseAllocator( size, baseAddress )
    , currentPosition( baseAddress )
    , previousPosition( nullptr )
{

}

StackAllocator::~StackAllocator()
{
    currentPosition = nullptr;
    previousPosition = nullptr;
}

void* StackAllocator::allocate( const size_t allocationSize, const u8 alignment )
{
    const u8 adjustment = dk::core::AlignForwardAdjustmentWithHeader( currentPosition, alignment, sizeof( AllocationHeader ) );

    if ( memoryUsage + adjustment + allocationSize > memorySize ) {
        return nullptr;
    }

    u8* allocatedAddress = static_cast< u8* >( currentPosition ) + adjustment;

    AllocationHeader* header = ( AllocationHeader* )( allocatedAddress - sizeof( AllocationHeader ) );
    header->adjustment = adjustment;
    header->previousAllocation = previousPosition;
    previousPosition = allocatedAddress;

    currentPosition = static_cast< void* >( allocatedAddress + allocationSize );

    memoryUsage += ( allocationSize + adjustment );
    allocationCount++;

    return static_cast< void* >( allocatedAddress );
}

void StackAllocator::free( void* pointer )
{
    u8* memoryPointer = static_cast< u8* >( pointer );

    // Retrieve Header
    AllocationHeader* header = ( AllocationHeader* )( memoryPointer - sizeof( AllocationHeader ) );

    memoryUsage -= ( u8* )currentPosition - ( u8* )pointer + header->adjustment;
    allocationCount--;

    currentPosition = memoryPointer - header->adjustment;
    previousPosition = header->previousAllocation;
}
