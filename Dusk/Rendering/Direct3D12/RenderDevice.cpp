/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "Image.h"
#include "Buffer.h"

#include "Core/Display/DisplaySurface.h"
#include "Core/Display/DisplaySurfaceWin32.h"

#include "Core/Allocators/PoolAllocator.h"
#include "Core/Allocators/LinearAllocator.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#if DUSK_DEVBUILD
#include <dxgidebug.h>
#endif

DUSK_DEV_VAR_PERSISTENT( UseWarpAdapter, false, bool ) // Force WARP adapter usage (in case no D3D12 compatible adapter is available)

RenderContext::RenderContext()
    : device( nullptr )
    , swapChain( nullptr )
    , directCmdQueue( nullptr )
    , computeCmdQueue( nullptr )
    , copyCmdQueue( nullptr )
    , synchronisationInterval( 0 )
    , frameCompletionEvent( nullptr )
    , samplerDescriptorHeap( nullptr )
    , rtvDescriptorHeap( nullptr )
    , rtvDescriptorHeapOffset( 0 )
    , dsvDescriptorHeap( nullptr )
    , dsvDescriptorHeapOffset( 0 )
    , srvDescriptorHeap( nullptr )
    , srvDescriptorCountPerFrame( 0 )
    , staticBufferHeap( nullptr )
    , heapOffset( 0 )
    , staticImageHeap( nullptr )
    , imageheapOffset( 0 )
    , dynamicBufferHeap( nullptr )
    , dynamicBufferHeapPerFrameCapacity( 0 )
    , dynamicBufferHeapOffset( 0 )
    , dynamicImageHeap( nullptr )
    , dynamicImageHeapPerFrameCapacity( 0 )
    , dynamicImageHeapOffset( 0 )
    , dynamicUavImageHeap( nullptr )
    , dynamicUavImageHeapPerFrameCapacity( 0 )
    , dynamicUavImageHeapOffset( 0 )
    , volatileBufferHeap( nullptr )
    , volatileBufferHeapPerFrameCapacity( 0 )
    , volatileBufferHeapOffset( 0 )
    , dxcLibrary( nullptr )
    , dxcContainerReflection( nullptr )
{
    memset( directCmdAllocator, 0, sizeof( ID3D12CommandAllocator* ) * RenderDevice::PENDING_FRAME_COUNT * RenderDevice::CMD_LIST_POOL_CAPACITY );
    memset( computeCmdAllocator, 0, sizeof( ID3D12CommandAllocator* ) * RenderDevice::PENDING_FRAME_COUNT * RenderDevice::CMD_LIST_POOL_CAPACITY );
    memset( copyCmdAllocator, 0, sizeof( ID3D12CommandAllocator* ) * RenderDevice::PENDING_FRAME_COUNT * RenderDevice::CMD_LIST_POOL_CAPACITY );
    memset( copyCmdLists, 0, sizeof( ID3D12GraphicsCommandList* ) * RenderDevice::PENDING_FRAME_COUNT * RenderDevice::CMD_LIST_POOL_CAPACITY );
    memset( copyCmdListUsageIndex, 0, sizeof( size_t ) * RenderDevice::PENDING_FRAME_COUNT );
    memset( frameCompletionFence, 0, sizeof( ID3D12Fence* ) * RenderDevice::PENDING_FRAME_COUNT );
    memset( frameFenceValues, 0, sizeof( u64 ) * RenderDevice::PENDING_FRAME_COUNT );
    memset( srvDescriptorHeapOffset, 0, sizeof( size_t ) * RenderDevice::PENDING_FRAME_COUNT );
    memset( volatileBuffers, 0, sizeof( ID3D12Resource* ) * RenderDevice::PENDING_FRAME_COUNT );
}

RenderContext::~RenderContext()
{
    if ( dxcHelper.IsEnabled() ) {
        dxcHelper.Cleanup();
    }

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < RenderDevice::CMD_LIST_POOL_CAPACITY; j++ ) {
            directCmdAllocator[i][j]->Release();
        }
    }
    directCmdQueue->Release();

   for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < RenderDevice::CMD_LIST_POOL_CAPACITY; j++ ) {
            computeCmdAllocator[i][j]->Release();
        }
    }
    computeCmdQueue->Release();

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < RenderDevice::CMD_LIST_POOL_CAPACITY; j++ ) {
            copyCmdLists[i][j]->Release();
        }
    }
    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < RenderDevice::CMD_LIST_POOL_CAPACITY; j++ ) {
            copyCmdAllocator[i][j]->Release();
        }
    }
    copyCmdQueue->Release();

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        frameCompletionFence[i]->Release();
    }

    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        volatileBuffers[i]->Release();
    }
    
    rtvDescriptorHeap->Release();
    srvDescriptorHeap->Release();

    staticBufferHeap->Release();
    staticImageHeap->Release();

    dynamicBufferHeap->Release();
    dynamicImageHeap->Release();
    dynamicUavImageHeap->Release();
    volatileBufferHeap->Release();
    samplerDescriptorHeap->Release();

    swapChain->Release();
    device->Release();

    CloseHandle( frameCompletionEvent );

#if DUSK_DEVBUILD
    IDXGIDebug1* dxgiDebug = nullptr;
    if ( SUCCEEDED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &dxgiDebug ) ) ) ) {
        dxgiDebug->ReportLiveObjects( DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS( DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL ) );
        dxgiDebug->Release();
    }
#endif

    swapChain = nullptr;
    device = nullptr;
}

RenderDevice::~RenderDevice()
{
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        graphicsCmdListAllocator[i]->clear();

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = static_cast< CommandList* >( graphicsCmdListAllocator[i]->allocate( sizeof( CommandList ) ) );
            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->graphicsCmdList->Release();
        }

        dk::core::free( memoryAllocator, graphicsCmdListAllocator[i] );
    }

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        computeCmdListAllocator[i]->clear();

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = static_cast< CommandList* >( computeCmdListAllocator[i]->allocate( sizeof( CommandList ) ) );
            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->graphicsCmdList->Release();
        }

        dk::core::free( memoryAllocator, computeCmdListAllocator[i] );
    }

    destroyImage( swapChainImage );

    dk::core::free( memoryAllocator, renderContext->volatileAllocatorsPool );
    dk::core::free( memoryAllocator, renderContext );
}

void RenderDevice::create( DisplaySurface& displaySurface, const u32 desiredRefreshRate, const bool useDebugContext )
{
    const NativeDisplaySurface* displaySurf = displaySurface.getNativeDisplaySurface();

#if DUSK_DEVBUILD
    // Debug Layer should be enabled as early as possible (otherwise it will usually
    // mess up the context)
    if ( useDebugContext ) {
        ID3D12Debug* debugController = nullptr;
        HRESULT operationResult = D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) );
        if ( SUCCEEDED( operationResult ) ) {
            debugController->EnableDebugLayer();
            DUSK_LOG_INFO( "Debug Context has been requested: EnableDebugLayer has been called.\n" );
        } else {
            DUSK_LOG_WARN( "Debug Context has been requested, but D3D12GetDebugInterface failed (error code: 0x%x).\n", operationResult );
        }
    }
#endif

    pipelineStateCacheAllocator = dk::core::allocate<LinearAllocator>( memoryAllocator, 4 << 20, memoryAllocator->allocate( 4 << 20 ) );
    renderContext = dk::core::allocate<RenderContext>( memoryAllocator );

    IDXGIFactory4* factory = nullptr;
    HRESULT operationResult = CreateDXGIFactory1( IID_PPV_ARGS( &factory ) );

    DUSK_LOG_INFO( "Enumerating Adapters...\n" );

    UINT bestAdapterIndex = UINT32_MAX;
    UINT bestOutputCount = 0;
    SIZE_T bestVRAM = 0;

    IDXGIAdapter1* adapter = nullptr;
    IDXGIOutput* output = nullptr;
    DXGI_ADAPTER_DESC1 adapterDescription;

    // Enumerate adapters available on the system and choose the best one
    for ( UINT i = 0; factory->EnumAdapters1( i, &adapter ) != DXGI_ERROR_NOT_FOUND; ++i ) {
        adapter->GetDesc1( &adapterDescription );

        UINT outputCount = 0;
        for ( ; adapter->EnumOutputs( outputCount, &output ) != DXGI_ERROR_NOT_FOUND; ++outputCount );

        // VRAM (MB)
        const SIZE_T adapterVRAM = ( adapterDescription.DedicatedVideoMemory >> 20 );

        // Choose the best adapter based on available VRAM and adapter count
        if ( outputCount != 0 && adapterVRAM > bestVRAM ) {
            bestVRAM = adapterVRAM;
            bestAdapterIndex = i;
            bestOutputCount = outputCount;
        }

        DUSK_LOG_RAW( "-Adapter %i '%s' VRAM: %uMB (%u output(s) found)\n",
            i, adapterDescription.Description, adapterVRAM, outputCount );
    }

    DUSK_RAISE_FATAL_ERROR( ( bestAdapterIndex != UINT32_MAX ), "No adapters found!" );

    factory->EnumAdapters1( bestAdapterIndex, &adapter );

    DUSK_LOG_INFO( "Selected Adapter >> Adapter %u\n", bestAdapterIndex );
    DUSK_LOG_INFO( "Enumerating Outputs...\n" );

    for ( UINT outputIdx = 0; outputIdx < bestOutputCount; outputIdx++ ) {
        adapter->EnumOutputs( outputIdx, &output );

        DXGI_OUTPUT_DESC outputDescription = {};
        output->GetDesc( &outputDescription );

        DUSK_LOG_INFO( "-Output %u '%s' at %i x %i\n",
            outputIdx, outputDescription.DeviceName,
            outputDescription.DesktopCoordinates.left, outputDescription.DesktopCoordinates.top );
    }

    UINT vsyncNumerator = 0, vsyncDenominator = 0;
    UINT vsyncRefreshRate = 0;
    UINT outputDisplayModeCount = 0;

    // Query display mode count
    output->GetDisplayModeList( DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &outputDisplayModeCount, nullptr );

    // Retrieve display mode list
    DXGI_MODE_DESC displayModeList[128];
    output->GetDisplayModeList( DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &outputDisplayModeCount, displayModeList );

    for ( u32 i = 0; i < outputDisplayModeCount; ++i ) {
        UINT refreshRate = ( displayModeList[i].RefreshRate.Numerator / displayModeList[i].RefreshRate.Denominator );

        if ( displayModeList[i].Width == displaySurf->WindowWidth
        && displayModeList[i].Height == displaySurf->WindowHeight
        && refreshRate > vsyncRefreshRate ) {
            vsyncNumerator = displayModeList[i].RefreshRate.Numerator;
            vsyncDenominator = displayModeList[i].RefreshRate.Denominator;
            vsyncRefreshRate = refreshRate;
        }
    }

    DUSK_LOG_INFO( "Selected Display Mode >> %ux%u @ %uHz\n",
        displaySurf->WindowWidth, displaySurf->WindowHeight, vsyncRefreshRate );

    output->Release();

    if ( UseWarpAdapter ) {
        DUSK_LOG_INFO( "FORCE_WRAP_DEVICE is enabled. RenderDevice will use WARP adapter (expect slow as fuck rendering)\n" );

        IDXGIAdapter* warpAdapter = nullptr;
        factory->EnumWarpAdapter( IID_PPV_ARGS( &warpAdapter ) );

        HRESULT deviceCreationResult = D3D12CreateDevice( warpAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &renderContext->device ) );
        DUSK_RAISE_FATAL_ERROR( SUCCEEDED( deviceCreationResult ), "D3D12CreateDevice FAILED! (error code: 0x%x)\n", deviceCreationResult );
    } else {
        HRESULT deviceCreationResult = D3D12CreateDevice( adapter, D3D_FEATURE_LEVEL_11_0, __uuidof( ID3D12Device ), reinterpret_cast<void**>( &renderContext->device ) );
        DUSK_RAISE_FATAL_ERROR( SUCCEEDED( deviceCreationResult ), "D3D12CreateDevice FAILED! (error code: 0x%x)\n", deviceCreationResult );
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    renderContext->device->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &renderContext->directCmdQueue ) );

    D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.NodeMask = 0;
    renderContext->device->CreateCommandQueue( &computeQueueDesc, IID_PPV_ARGS( &renderContext->computeCmdQueue ) );

    D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
    copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    copyQueueDesc.NodeMask = 0;
    renderContext->device->CreateCommandQueue( &copyQueueDesc, IID_PPV_ARGS( &renderContext->copyCmdQueue ) );

    renderContext->directCmdQueue->GetTimestampFrequency( &renderContext->directQueueTimestampFreq );
    renderContext->computeCmdQueue->GetTimestampFrequency( &renderContext->computeQueueTimestampFreq );

    DUSK_LOG_INFO( "Creating SwapChain... (HWND: 0x%x)\n", displaySurf->Handle );

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferCount = PENDING_FRAME_COUNT;
    swapChainDesc.BufferDesc.Width = displaySurf->WindowWidth;
    swapChainDesc.BufferDesc.Height = displaySurf->WindowHeight;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = vsyncDenominator;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = vsyncNumerator;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = displaySurf->Handle;
    swapChainDesc.Windowed = ( displaySurface.getDisplayMode() == eDisplayMode::WINDOWED );
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    factory->CreateSwapChain( 
        renderContext->directCmdQueue,
        &swapChainDesc,
        reinterpret_cast<IDXGISwapChain**>( &renderContext->swapChain )
    );

    factory->MakeWindowAssociation( displaySurf->Handle, DXGI_MWA_NO_ALT_ENTER );
    factory->Release();

    DUSK_LOG_INFO( "Allocating heaps...\n" );

    D3D12_HEAP_DESC heapDesc;
    heapDesc.SizeInBytes = ( 64 << 20 ) * RenderDevice::PENDING_FRAME_COUNT;
    heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.CreationNodeMask = 0;
    heapDesc.Properties.VisibleNodeMask = 0;

    // Static buffer heap (stuff that should be allocated and uploaded once during the lifetime of the application)
    heapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast<void**>( &renderContext->staticBufferHeap ) );
    
    // Static images heap (same as the buffer heap, should be used for permanent images e.g. LUTs, HUD resources, etc.)
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast< void** >( &renderContext->staticImageHeap ) );

    // Dynamic buffer heap (heap used for 'regular' buffer allocation) lifetime might depends on the current context/state of the application
    heapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    heapDesc.SizeInBytes = ( 256 << 20 );
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast< void** >( &renderContext->dynamicBufferHeap ) );
    
    renderContext->dynamicBufferHeapPerFrameCapacity = ( heapDesc.SizeInBytes / PENDING_FRAME_COUNT );
    
    // Align frame offset to D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT to satify resource alignment restriction
    renderContext->dynamicBufferHeapPerFrameCapacity += ( D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT - ( renderContext->dynamicBufferHeapPerFrameCapacity % D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT ) );

    renderContext->dynamicBufferHeapOffset = 0;

    // Dynamic image heap (same as above)

    constexpr u64 PER_FRAME_CAPACITY = ( 512 << 20 );

    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    heapDesc.SizeInBytes = PER_FRAME_CAPACITY * RenderDevice::PENDING_FRAME_COUNT;
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast< void** >( &renderContext->dynamicImageHeap ) );

    renderContext->dynamicImageHeapPerFrameCapacity = PER_FRAME_CAPACITY;
    renderContext->dynamicImageHeapOffset = 0;

    // Dynamic image heap (same as above)
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    heapDesc.SizeInBytes = PER_FRAME_CAPACITY * RenderDevice::PENDING_FRAME_COUNT;
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast< void** >( &renderContext->dynamicUavImageHeap ) );

    renderContext->dynamicUavImageHeapPerFrameCapacity = PER_FRAME_CAPACITY;
    renderContext->dynamicUavImageHeapOffset = 0;

    heapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES; // Tier1 devices need to know the type of resources that'll be allocated on the heap
    heapDesc.SizeInBytes = 65536 * 64 * PENDING_FRAME_COUNT;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    renderContext->device->CreateHeap( &heapDesc, __uuidof( ID3D12Heap ), reinterpret_cast< void** >( &renderContext->volatileBufferHeap ) );

    renderContext->volatileBufferHeapPerFrameCapacity = 65536 * 64;
    renderContext->volatileBufferHeapOffset = 0;

    D3D12_RESOURCE_DESC resourceDesc;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    resourceDesc.Width = renderContext->volatileBufferHeapPerFrameCapacity;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create the volatile buffer that'll be used for dynamic resources usage
    for ( i32 i = 0; i < RenderDevice::PENDING_FRAME_COUNT; i++ ) {
        operationResult = renderContext->device->CreatePlacedResource(
            renderContext->volatileBufferHeap,
            renderContext->volatileBufferHeapPerFrameCapacity * i,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof( ID3D12Resource ),
            reinterpret_cast< void** >( &renderContext->volatileBuffers[i] )
        );

        DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Volatile buffer creation FAILED! (error code: 0x%x)", operationResult );
    }

    // Create descriptor heaps.
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = PENDING_FRAME_COUNT * 32;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    renderContext->device->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS( &renderContext->rtvDescriptorHeap ) );

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = PENDING_FRAME_COUNT * 32;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    renderContext->device->CreateDescriptorHeap( &dsvHeapDesc, IID_PPV_ARGS( &renderContext->dsvDescriptorHeap ) );

    renderContext->srvDescriptorCountPerFrame = 32 * 3;

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = static_cast<UINT>( PENDING_FRAME_COUNT * renderContext->srvDescriptorCountPerFrame ); // 32 descriptor for CBV/SRV/UAV
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    renderContext->device->CreateDescriptorHeap( &srvHeapDesc, IID_PPV_ARGS( &renderContext->srvDescriptorHeap ) );

    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc;
    samplerHeapDesc.NumDescriptors = 32;
    samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    samplerHeapDesc.NodeMask = 0;
    samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    renderContext->device->CreateDescriptorHeap( &samplerHeapDesc, IID_PPV_ARGS( &renderContext->samplerDescriptorHeap ) );

    renderContext->volatileAllocatorsPool = dk::core::allocate<PoolAllocator>( memoryAllocator, 
                                                                               sizeof( Buffer* ),
                                                                               4,
                                                                               sizeof( Buffer* ) * 128,
                                                                               memoryAllocator->allocate( sizeof( Buffer* ) * 128 ) );

    // We create the data structure but don't allocate anything (since we'll retrieve the backbuffer memory from the swapchain)
    swapChainImage = dk::core::allocate<Image>( memoryAllocator );
    swapChainImage->width = displaySurf->WindowWidth;
    swapChainImage->height = displaySurf->WindowHeight;

    for ( UINT n = 0; n < PENDING_FRAME_COUNT; n++ ) {
        swapChainImage->currentResourceState[n] = D3D12_RESOURCE_STATE_PRESENT;
    }

    swapChainImage->defaultFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainImage->arraySize = 1;
    swapChainImage->dimension = ImageDesc::DIMENSION_2D;
    swapChainImage->mipCount = 1;
    swapChainImage->samplerCount = 1;

    // Create a RTV for each frame.
    for ( UINT n = 0; n < PENDING_FRAME_COUNT; n++ ) {
        renderContext->swapChain->GetBuffer( n, IID_PPV_ARGS( &swapChainImage->resource[n] ) );
    }

    // Frame synchronization objects
    renderContext->frameCompletionEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        renderContext->frameFenceValues[i] = 0;
        renderContext->device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &renderContext->frameCompletionFence[i] ) );
    }

    // Create command list allocators (per command queue)
    constexpr size_t CMD_LIST_ALLOCATION_SIZE = sizeof( CommandList ) * CMD_LIST_POOL_CAPACITY; 
    
    // Graphics Command Lists
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        graphicsCmdListAllocator[i] = dk::core::allocate<LinearAllocator>( 
            memoryAllocator,
            CMD_LIST_ALLOCATION_SIZE, 
            memoryAllocator->allocate( CMD_LIST_ALLOCATION_SIZE ) 
        );

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            renderContext->device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &renderContext->directCmdAllocator[i][j] ) );

            CommandList* cmdList = dk::core::allocate<CommandList>( graphicsCmdListAllocator[i], CommandList::Type::GRAPHICS );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->renderContext = renderContext;

            ID3D12GraphicsCommandList** dxCmdList = &nativeCmdList->graphicsCmdList;
            HRESULT operationResult = renderContext->device->CreateCommandList( 
                0, 
                D3D12_COMMAND_LIST_TYPE_DIRECT, 
                renderContext->directCmdAllocator[i][j],
                nullptr,
                IID_PPV_ARGS( dxCmdList )
            );

            DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Command List Creation has failed! (error code: 0x%x)", operationResult );
            ( *dxCmdList )->Close();

            nativeCmdList->allocator = &renderContext->directCmdAllocator[i][j];
        }

        graphicsCmdListAllocator[i]->clear();
    }

    // Compute Command Lists
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        computeCmdListAllocator[i] = dk::core::allocate<LinearAllocator>(
            memoryAllocator,
            CMD_LIST_ALLOCATION_SIZE,
            memoryAllocator->allocate( CMD_LIST_ALLOCATION_SIZE )
        );

        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            renderContext->device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS( &renderContext->computeCmdAllocator[i][j] ) );

            CommandList* cmdList = dk::core::allocate<CommandList>( computeCmdListAllocator[i], CommandList::Type::COMPUTE );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->renderContext = renderContext;

            ID3D12GraphicsCommandList** dxCmdList = &nativeCmdList->graphicsCmdList;
            HRESULT operationResult = renderContext->device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                renderContext->computeCmdAllocator[i][j],
                nullptr,
                IID_PPV_ARGS( dxCmdList )
            );

            DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Command List Creation has failed! (error code: 0x%x)", operationResult );
            ( *dxCmdList )->Close();

            nativeCmdList->allocator = &renderContext->computeCmdAllocator[i][j];
        }

        computeCmdListAllocator[i]->clear();
    }

    // Copy Command Lists
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            renderContext->device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS( &renderContext->copyCmdAllocator[i][j] ) );

            HRESULT operationResult = renderContext->device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COPY,
                renderContext->copyCmdAllocator[i][j],
                nullptr,
                IID_PPV_ARGS( &renderContext->copyCmdLists[i][j] )
            );

            DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "Command List Creation has failed! (error code: 0x%x)", operationResult );
            renderContext->copyCmdLists[i][j]->Close();
        }
    }

    renderContext->dxcHelper.Initialize();

    HRESULT instanceCreationRes = renderContext->dxcHelper.CreateInstance( CLSID_DxcLibrary, &renderContext->dxcLibrary );
    DUSK_ASSERT( instanceCreationRes == S_OK, "DXC lib loading FAILED! (error code 0x%x)", instanceCreationRes )

    renderContext->dxcHelper.CreateInstance( CLSID_DxcContainerReflection, &renderContext->dxcContainerReflection );
}

void RenderDevice::enableVerticalSynchronisation( const bool enabled )
{
    renderContext->synchronisationInterval = ( enabled ) ? 1 : 0;
}

CommandList& RenderDevice::allocateGraphicsCommandList()
{
    const size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    // Reset the allocator (go back to the start of the allocator memory space)
    if ( graphicsCmdListAllocator[bufferIdx]->getAllocationCount() == CMD_LIST_POOL_CAPACITY ) {
        graphicsCmdListAllocator[bufferIdx]->clear();
    }
    CommandList* cmdList = static_cast<CommandList*>( graphicsCmdListAllocator[bufferIdx]->allocate( sizeof( CommandList ) ) );
    NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();

    // The command list itself shouldn't be reset until we bind a pipeline state (which should happen if the user requested a
    // cmd list on the graphics command queue)
    ID3D12CommandAllocator* cmdListAllocator = renderContext->directCmdAllocator[bufferIdx][cmdList->getCommandListPooledIndex()];
    cmdListAllocator->Reset();

    cmdList->setFrameIndex( static_cast<i32>( frameIndex ) );

    return *cmdList;
}

CommandList& RenderDevice::allocateComputeCommandList()
{
    const size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    // Reset the allocator (go back to the start of the allocator memory space)
    if ( computeCmdListAllocator[bufferIdx]->getAllocationCount() == CMD_LIST_POOL_CAPACITY ) {
        computeCmdListAllocator[bufferIdx]->clear();
    }
    CommandList* cmdList = static_cast< CommandList* >( computeCmdListAllocator[bufferIdx]->allocate( sizeof( CommandList ) ) );
    NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();

    // The command list itself shouldn't be reset until we bind a pipeline state (which should happen if the user requested a
    // cmd list on the graphics command queue)
    ID3D12CommandAllocator* cmdListAllocator = renderContext->computeCmdAllocator[bufferIdx][cmdList->getCommandListPooledIndex()];
    cmdListAllocator->Reset();

    cmdList->setFrameIndex( static_cast< i32 >( frameIndex ) );

    return *cmdList;
}

CommandList& RenderDevice::allocateCopyCommandList()
{
    return allocateComputeCommandList();
}

void RenderDevice::submitCommandList( CommandList& cmdList )
{
    NativeCommandList* nativeCmdList = cmdList.getNativeCommandList();
    ID3D12GraphicsCommandList* dxCmdList = nativeCmdList->graphicsCmdList;

    ID3D12CommandQueue* submitQueue = ( cmdList.getCommandListType() == CommandList::Type::GRAPHICS ) ? renderContext->directCmdQueue : renderContext->computeCmdQueue;
    submitQueue->ExecuteCommandLists( 1, reinterpret_cast< ID3D12CommandList** >( &dxCmdList ) );
}

void RenderDevice::submitCommandLists( CommandList** cmdLists, const u32 cmdListCount )
{

}
 
void RenderDevice::resizeBackbuffer( const u32 width, const u32 height )
{

}

void RenderDevice::present()
{
    HRESULT operationResult = renderContext->swapChain->Present( renderContext->synchronisationInterval, 0 );
    DUSK_RAISE_FATAL_ERROR( SUCCEEDED( operationResult ), "Device removed! (error code: 0x%x)\n", renderContext->device->GetDeviceRemovedReason() );

    // Setup a fence on the direct command queue (the one used by the swapchain)
    size_t frameBufferIdx = ( frameIndex % PENDING_FRAME_COUNT );
    renderContext->directCmdQueue->Signal( renderContext->frameCompletionFence[frameBufferIdx], ( renderContext->frameFenceValues[frameBufferIdx] + PENDING_FRAME_COUNT ) );
    
    frameIndex++;
    frameBufferIdx = ( frameIndex % PENDING_FRAME_COUNT );

    // Ensure next frame resources are available
    UINT64 fenceResult = renderContext->frameCompletionFence[frameBufferIdx]->GetCompletedValue();
    if ( fenceResult < renderContext->frameFenceValues[frameBufferIdx] ) {
        DUSK_LOG_WARN( "Stalling detected (Frame Index : %u)! "
                       "You might want to increment the number of pending frame, or profile the rendering pipeline to reduce the workload...\n",
                       frameIndex );

        renderContext->frameCompletionFence[frameBufferIdx]->SetEventOnCompletion( renderContext->frameFenceValues[frameBufferIdx], renderContext->frameCompletionEvent );
        WaitForSingleObjectEx( renderContext->frameCompletionEvent, INFINITE, FALSE );
    }

    renderContext->frameFenceValues[frameBufferIdx] += PENDING_FRAME_COUNT;

    // Reset shader resource view descriptor heap for the current frame
    UINT descriptorSize = renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
    renderContext->srvDescriptorHeapOffset[frameBufferIdx] = ( descriptorSize * renderContext->srvDescriptorCountPerFrame );

    // Do volatile allocations for the next frame (split the heap accordingly to the allocation request list)
    // Reset volatile descriptor heap offset
    renderContext->volatileBufferHeapOffset = 0;

    size_t allocationRequestCount = renderContext->volatileAllocatorsPool->getAllocationCount();
    Buffer** buffers = reinterpret_cast< Buffer** >( renderContext->volatileAllocatorsPool->getBaseAddress() );

    const size_t heapOffset = renderContext->volatileBufferHeapPerFrameCapacity * frameBufferIdx;
    for ( size_t i = 0; i < allocationRequestCount; i++ ) {
        Buffer* buffer = buffers[i];
        buffer->heapOffset = heapOffset + renderContext->volatileBufferHeapOffset;
        renderContext->volatileBufferHeapOffset += buffer->size;     
    }
}

void RenderDevice::waitForPendingFrameCompletion()
{
    // Ensure next frame resources are available
    DUSK_LOG_INFO( "Waiting pending frame completion...\n",  );

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        size_t idx = ( ++frameIndex % PENDING_FRAME_COUNT );
        renderContext->directCmdQueue->Signal( renderContext->frameCompletionFence[idx], ( renderContext->frameFenceValues[idx] + PENDING_FRAME_COUNT ) );

        DUSK_LOG_INFO( "Waiting pending frame completion (%i)...\n", idx );
        renderContext->frameCompletionFence[idx]->SetEventOnCompletion( renderContext->frameFenceValues[idx] + PENDING_FRAME_COUNT, renderContext->frameCompletionEvent );
        WaitForSingleObjectEx( renderContext->frameCompletionEvent, INFINITE, FALSE );
    }
}

const dkChar_t* RenderDevice::getBackendName()
{
    return DUSK_STRING( "Direct3D12" );
}
#endif
