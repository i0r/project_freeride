/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FreeListAllocator.h"

#include "AllocationHelpers.h"

FreeListAllocator::FreeListAllocator( const size_t size, void* baseAddress )
    : BaseAllocator( size, baseAddress )
    , freeBlockList( static_cast<FreeBlock*>( baseAddress ) )
{
    freeBlockList->size = size;
    freeBlockList->next = nullptr;
}

FreeListAllocator::~FreeListAllocator()
{
    freeBlockList = nullptr;
}

void* FreeListAllocator::allocate( const size_t allocationSize, const u8 alignment )
{
    FreeBlock* previousFreeBlock = nullptr;
    FreeBlock* freeBlock = freeBlockList;

    while ( freeBlock != nullptr ) {
        const u8 adjustment = dk::core::AlignForwardAdjustmentWithHeader( freeBlock, alignment, sizeof( AllocationHeader ) );
        size_t requiredSize = allocationSize + adjustment;

        // If allocation doesn't fit in this FreeBlock, try the next 
        if ( freeBlock->size < requiredSize ) {
            previousFreeBlock = freeBlock;
            freeBlock = freeBlock->next;
            continue;
        }

        // If allocations in the remaining memory will be impossible 
        if ( freeBlock->size - requiredSize <= sizeof( AllocationHeader ) ) {
            // Increase allocation size instead of creating a new FreeBlock 
            requiredSize = freeBlock->size;

            if ( previousFreeBlock != nullptr )
                previousFreeBlock->next = freeBlock->next;
            else
                freeBlock = freeBlock->next;
        } else {
            // Else create a new FreeBlock containing remaining memory 
            FreeBlock* nextFreeBlock = static_cast<FreeBlock*>( freeBlock + requiredSize );

            nextFreeBlock->size = freeBlock->size - requiredSize;
            nextFreeBlock->next = freeBlock->next;

            if ( previousFreeBlock != nullptr )
                previousFreeBlock->next = nextFreeBlock;
            else
                freeBlockList = nextFreeBlock;
        }

        u8* allocatedAddress = reinterpret_cast< u8* >( freeBlock ) + adjustment;
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>( allocatedAddress - sizeof( AllocationHeader ) );
        header->size = requiredSize;
        header->adjustment = adjustment;

        memoryUsage += requiredSize;
        allocationCount++;

        return static_cast< void* >( allocatedAddress );
    }

    return nullptr;
}

void FreeListAllocator::free( void* pointer )
{
    AllocationHeader* header = ( reinterpret_cast<AllocationHeader*>( pointer ) - sizeof( AllocationHeader ) );

    const u8* blockBaseAddress = reinterpret_cast<u8*>( pointer ) - header->adjustment;
    const size_t blockSize = header->size;
    const u8* blockEndAddress = blockBaseAddress + blockSize;

    FreeBlock* previousFreeBlock = nullptr;
    FreeBlock* freeBlock = freeBlockList;

    while ( freeBlock != nullptr ) {
        if ( reinterpret_cast<u8*>(freeBlock) >= blockEndAddress ) {
            break;
        }

        previousFreeBlock = freeBlock;
        freeBlock = freeBlock->next;
    }

    if ( previousFreeBlock == nullptr ) {
        previousFreeBlock = ( FreeBlock* )blockBaseAddress;
        previousFreeBlock->size = blockSize;
        previousFreeBlock->next = freeBlockList;
        
        freeBlockList = previousFreeBlock;
    } else if ( ( u8* )previousFreeBlock + previousFreeBlock->size == blockBaseAddress ) {
        previousFreeBlock->size += blockSize;
    } else {
        FreeBlock* temp = ( FreeBlock* )blockBaseAddress;
        temp->size = blockSize;
        temp->next = previousFreeBlock->next;
        previousFreeBlock->next = temp;
        previousFreeBlock = temp;
    }

    if ( freeBlock != nullptr && ( u8* )freeBlock == blockEndAddress ) {
        previousFreeBlock->size += freeBlock->size;
        previousFreeBlock->next = freeBlock->next;
    }

    allocationCount--;
    memoryUsage -= blockSize;
}
