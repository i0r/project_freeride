/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D12
struct ID3D12Device;
struct ID3D12CommandQueue;
struct IDXGISwapChain3;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
struct ID3D12DescriptorHeap;
struct ID3D12Heap;
struct ID3D12Resource;

#include <Core/Allocators/PoolAllocator.h>

#include <Rendering/RenderDevice.h>

#include <d3dcompiler.h>
#include <ThirdParty/dxc/include/dxc/dxcapi.use.h>

struct RenderMemoryHeap 
{
    // Heap handle (allocated by the active RenderContext).
    ID3D12Heap*     Heap;

    // Offset (in bytes) of the next free allocable memory section.
    size_t          Offset;

    // Memory capacity (in bytes) allocable for a single buffered frame (if the heap is used for multiple frame buffering).
    // If the heap doesn't use multiple frame buffering, this should be equal to the total capacity of the heap.
    size_t          PerFrameCapacity;

    // Total size (in bytes) of the heap (should be = PerFrameCapacity * NumberOfFrameBuffered).
    size_t          Size;

    RenderMemoryHeap( ID3D12Heap* allocatedHeap, const size_t perFrameCapacity, const bool isUsingMultipleFrameBuffering )
        : Heap( allocatedHeap )
        , Offset( 0ull )
        , PerFrameCapacity( perFrameCapacity )
        , Size( ( isUsingMultipleFrameBuffering ) ? perFrameCapacity * RenderDevice::PENDING_FRAME_COUNT : perFrameCapacity )
    {

    }
};

struct RenderContext
{
                                RenderContext();
                                ~RenderContext();

    ID3D12Device*               device;
    IDXGISwapChain3*            swapChain;

    ID3D12CommandQueue*         directCmdQueue;
    ID3D12CommandQueue*         computeCmdQueue;
    ID3D12CommandQueue*         copyCmdQueue;

    // Timestamp frequency (in Hz) for each cmd queue
    // Might need some investigation, since I'm not completely sure if the frequency is constant or if it relies on the power management settings
    UINT64                      directQueueTimestampFreq;
    UINT64                      computeQueueTimestampFreq;

    UINT                        synchronisationInterval;

    ID3D12CommandAllocator*     directCmdAllocator[RenderDevice::PENDING_FRAME_COUNT][RenderDevice::CMD_LIST_POOL_CAPACITY];
    ID3D12CommandAllocator*     computeCmdAllocator[RenderDevice::PENDING_FRAME_COUNT][RenderDevice::CMD_LIST_POOL_CAPACITY];

    // Copy Command Lists shouldnt be exposed in the RenderDevice API (doesnt make sense at higher level)
    ID3D12CommandAllocator*     copyCmdAllocator[RenderDevice::PENDING_FRAME_COUNT][RenderDevice::CMD_LIST_POOL_CAPACITY];
    ID3D12GraphicsCommandList*  copyCmdLists[RenderDevice::PENDING_FRAME_COUNT][RenderDevice::CMD_LIST_POOL_CAPACITY];
    size_t                      copyCmdListUsageIndex[RenderDevice::PENDING_FRAME_COUNT];

    ID3D12Fence*                frameCompletionFence[RenderDevice::PENDING_FRAME_COUNT];
    HANDLE                      frameCompletionEvent;
    u64                         frameFenceValues[RenderDevice::PENDING_FRAME_COUNT];

    ID3D12DescriptorHeap*       samplerDescriptorHeap;

    ID3D12DescriptorHeap*       rtvDescriptorHeap; // RTV
    size_t                      rtvDescriptorHeapOffset;

    ID3D12DescriptorHeap*       dsvDescriptorHeap; // DSV
    size_t                      dsvDescriptorHeapOffset;

    ID3D12DescriptorHeap*       srvDescriptorHeap; // CBV / UAV / SRV (shader visible Descriptor heap)
    size_t                      srvDescriptorHeapOffset[RenderDevice::PENDING_FRAME_COUNT]; // Shared by any kind of descriptor. Root Signature should allocate the descriptors in a continuous way
                                                                                            // in order to facilitate the descriptor table bindingsize_t                      srvDescriptorPerFrame
    size_t                      srvDescriptorCountPerFrame;

    PoolAllocator*              volatileAllocatorsPool;

    RenderMemoryHeap*           StaticBufferHeap;

    ID3D12Heap*                 staticBufferHeap;    // RESOURCE_USAGE_STATIC
    size_t                      bufferheapOffset;

    ID3D12Heap*                 staticImageHeap;    // RESOURCE_USAGE_STATIC
    size_t                      imageheapOffset;

    // A memory heap holds the triple buffered resources in a linear way
    // This should help data coherency on the GPU side
    //
    // FRAME 0                  FRAME 1                            [...]
    // | RES0   RES1    RES2    | RES0      RES1        RES2       |
    // | 0      32      64      | f0Max + 0 f0Max + 32  f0Max + 64 |
    // |___________________________________________________________|
    ID3D12Heap*                 dynamicBufferHeap;   // RESOURCE_USAGE_DEFAULT
    size_t                      dynamicBufferHeapPerFrameCapacity;
    size_t                      dynamicBufferHeapOffset;

    ID3D12Heap*                 dynamicImageHeap;   // RESOURCE_USAGE_DEFAULT
    size_t                      dynamicImageHeapPerFrameCapacity;
    size_t                      dynamicImageHeapOffset;

    ID3D12Heap*                 dynamicUavImageHeap;   // RESOURCE_USAGE_DEFAULT; NON-RTV/DSV only
    size_t                      dynamicUavImageHeapPerFrameCapacity;
    size_t                      dynamicUavImageHeapOffset;

    ID3D12Heap*                 volatileBufferHeap;   // RESOURCE_USAGE_DYNAMIC
    size_t                      volatileBufferHeapPerFrameCapacity;
    size_t                      volatileBufferHeapOffset;
    ID3D12Resource*             volatileBuffers[RenderDevice::PENDING_FRAME_COUNT];

    // Class helper to load DXC library (used for shader reflection)
    // TODO We might want to deprecate this since we already have reflection infos available
    // with the renderlib framework... (we could remove spirvcross dep. too)
    dxc::DxcDllSupport          dxcHelper;
    IDxcLibrary*                dxcLibrary;
    IDxcContainerReflection*    dxcContainerReflection;

    // True if NvAPI is loaded and available; false otherwise.
    bool                        IsNvApiLoaded;

    // True if AMD AGS is loaded and available; false otherwise.
    bool                        IsAmdAgsLoaded;

#if DUSK_USE_AGS
    // Pointer to the active AMD AGS context (if loaded).
    struct AGSContext*          AgsContext;
#endif
};
#endif
