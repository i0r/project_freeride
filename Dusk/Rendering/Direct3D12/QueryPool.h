/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
struct ID3D12QueryHeap;
struct Buffer;

struct QueryPool
{
    ID3D12QueryHeap*    queryHeap[RenderDevice::PENDING_FRAME_COUNT];
    D3D12_QUERY_TYPE    queryType;
    u32                 heapUsage[RenderDevice::PENDING_FRAME_COUNT];
    u32                 previousHeapUsage[RenderDevice::PENDING_FRAME_COUNT];
    u32                 queryRetrievalMark[RenderDevice::PENDING_FRAME_COUNT];
    u32                 heapCapacity;
    Buffer*             readbackBuffer;
};
#endif
