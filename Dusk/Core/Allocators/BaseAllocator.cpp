/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "BaseAllocator.h"

BaseAllocator::BaseAllocator( const size_t size, void* baseAddress )
    : baseAddress( baseAddress )
    , memorySize( size )
    , memoryUsage( 0ull )
    , allocationCount( 0ull )
{

}

BaseAllocator::~BaseAllocator()
{
    baseAddress = nullptr;
    memorySize = 0ull;
    memoryUsage = 0ull;
    allocationCount = 0ull;
}

void* BaseAllocator::getBaseAddress() const
{
    return baseAddress;
}

size_t BaseAllocator::getSize() const
{
    return memorySize;
}

size_t BaseAllocator::getMemoryUsage() const
{
    return memoryUsage;
}

size_t BaseAllocator::getAllocationCount() const
{
    return allocationCount;
}
