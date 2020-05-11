/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
#include <d3d12.h> // Required by D3D12_RANGE

struct Buffer
{
    static constexpr i32        MAX_SIMULTANEOUS_MEMORY_MAPPING = 16;

    ID3D12Resource*             resource[RenderDevice::PENDING_FRAME_COUNT];
    u32                         size;
    u32                         Stride;
    eResourceUsage              usage; // Needed to figure out where the memory has been allocated
    D3D12_RESOURCE_STATES       currentResourceState;
    size_t                      heapOffset; // Needed for volatile resource view
    D3D12_RANGE                 memoryMappedRanges[MAX_SIMULTANEOUS_MEMORY_MAPPING];
    i32                         memoryMappedRangeCount;
    Buffer**                    allocationRequest; // Pointer to an address which should hold the buffer pointer itself.
                                                   // Used by the render device to do volatile allocation at the start of the 
                                                   // frame and avoid heap fragmentation as much as possible
};
#endif
