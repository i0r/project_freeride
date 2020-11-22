/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "Image.h"
#include "PipelineState.h"
#include "ResourceAllocationHelpers.h"

#include "Core/Display/DisplaySurface.h"
#include "Core/Display/DisplaySurfaceWin32.h"
#include "Core/Allocators/LinearAllocator.h"

#include <d3d11.h>
#include <d3d11_1.h>

#if DUSK_USE_NVAPI
#include "nvapi.h"
#endif

#if DUSK_USE_AGS
#include "amd_ags.h"
#endif

RenderContext::RenderContext()
    : ImmediateContext( nullptr )
    , PhysicalDevice( nullptr )
    , SwapChain( nullptr )
    , SynchronisationInterval( 0 )
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    , Annotation( nullptr )
#endif
    , ActiveViewport{ 0 }
    , ActiveScissor{ 0 }
    , CsUavRegisters{ nullptr }
    , CsUavRegisterUpdateStart( ~0 )
    , CsUavRegisterUpdateCount( 0 )
    , PsUavRegisters{ nullptr }
    , PsUavRegisterUpdateStart( ~0 )
    , PsUavRegisterUpdateCount( 0 )
    , FramebufferDepthBuffer( nullptr )
    , SamplerRegisterUpdateStart( ~0 )
    , SamplerRegisterUpdateCount( 0 )
    , BindedPipelineState( nullptr )
#if DUSK_DEVBUILD
    , ActiveDebugMarker( nullptr )
#endif
	, IsNvApiLoaded( false )
    , IsAmdAgsLoaded( false )
#if DUSK_USE_AGS
    , AgsContext( nullptr )
#endif
{
    memset( CsUavRegistersInfo, 0x00, sizeof( RegisterData )* D3D11_1_UAV_SLOT_COUNT );
	memset( SrvRegistersInfo, 0x00, sizeof( RegisterData )* eShaderStage::SHADER_STAGE_COUNT* D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT );

    memset( SrvRegisterUpdateStart, 0xff, sizeof( u32 ) * eShaderStage::SHADER_STAGE_COUNT );
    memset( SrvRegisterUpdateCount, 0x00, sizeof( i32 ) * eShaderStage::SHADER_STAGE_COUNT );
    memset( SrvRegisters, 0x00, sizeof( ID3D11ShaderResourceView* ) * eShaderStage::SHADER_STAGE_COUNT * D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT );
    memset( FramebufferAttachment, 0x00, sizeof( Image* ) * D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT );

    memset( CBufferRegisterUpdateStart, 0xff, sizeof( u32 ) * eShaderStage::SHADER_STAGE_COUNT );
    memset( CBufferRegisterUpdateCount, 0x00, sizeof( i32 ) * eShaderStage::SHADER_STAGE_COUNT );
    memset( CBufferRegisters, 0x00, sizeof( ID3D11Buffer* ) * eShaderStage::SHADER_STAGE_COUNT * D3D11_COMMONSHADER_CONSTANT_BUFFER_REGISTER_COUNT );

	memset( SamplerRegisters, 0x00, sizeof( ID3D11SamplerState* )* eShaderStage::SHADER_STAGE_COUNT* D3D11_COMMONSHADER_SAMPLER_REGISTER_COUNT );
}

RenderContext::~RenderContext()
{
#define D3D11_RELEASE( object ) if ( object != nullptr ) { object->Release(); object = nullptr; }
    D3D11_RELEASE( SwapChain );
    D3D11_RELEASE( ImmediateContext );
#if DUSK_ENABLE_GPU_DEBUG_MARKER
    D3D11_RELEASE( Annotation );
#endif
    D3D11_RELEASE( PhysicalDevice );
#undef D3D11_RELEASE

#if DUSK_USE_NVAPI
    if ( IsNvApiLoaded ) {
        NvAPI_Unload();
    }
#endif

#if DUSK_USE_AGS
    if ( IsAmdAgsLoaded ) {
        agsDeInit( AgsContext );
        AgsContext = nullptr;
    }
#endif

    SynchronisationInterval = 0;
}

RenderDevice::~RenderDevice()
{
    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        graphicsCmdListAllocator[i]->clear();
        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = static_cast< CommandList* >( graphicsCmdListAllocator[i]->allocate( sizeof( CommandList ) ) );
            cmdList->~CommandList();
        }

        dk::core::free( memoryAllocator, graphicsCmdListAllocator[i] );
    }

    for ( i32 i = 0; i < PENDING_FRAME_COUNT; i++ ) {
        computeCmdListAllocator[i]->clear();
        for ( i32 j = 0; j < CMD_LIST_POOL_CAPACITY; j++ ) {
            CommandList* cmdList = static_cast< CommandList* >( computeCmdListAllocator[i]->allocate( sizeof( CommandList ) ) );
            cmdList->~CommandList();
        }

        dk::core::free( memoryAllocator, computeCmdListAllocator[i] );
    }
    
    destroyImage( swapChainImage );

    dk::core::free( memoryAllocator, renderContext );
}

void RenderDevice::create( DisplaySurface& displaySurface, const u32 desiredRefreshRate, const bool useDebugContext )
{
	renderContext = dk::core::allocate<RenderContext>( memoryAllocator );

    IDXGIFactory1* factory = nullptr;
    CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), ( void** )&factory );

    DUSK_LOG_INFO( "Enumerating Adapters...\n" );

    UINT bestAdapterIndex = UINT32_MAX;
    UINT bestOutputCount = 0;
    SIZE_T bestVRAM = 0;
    UINT bestVendorId = 0;

    IDXGIAdapter1* adapter = nullptr;
    IDXGIOutput* output = nullptr;
    DXGI_ADAPTER_DESC1 adapterDescription = {};

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
            bestVendorId = adapterDescription.VendorId;
        }

        DUSK_LOG_RAW( "-Adapter %i '%s' VRAM: %uMB (%u output(s) found)\n",
            i, adapterDescription.Description, adapterVRAM, outputCount );
    }

    DUSK_RAISE_FATAL_ERROR( ( bestAdapterIndex != UINT32_MAX ), "No adapters found!" );

    factory->EnumAdapters1( bestAdapterIndex, &adapter );

    DUSK_LOG_INFO( "Selected Adapter >> Adapter %u\n", bestAdapterIndex );

    bool DisableVendorExtensions = *EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "DisableVendorExtensions" ) );

#if DUSK_USE_NVAPI
    if ( !DisableVendorExtensions && bestVendorId == dk::render::NVIDIA_VENDOR_ID ) {
        _NvAPI_Status initializationResult = NvAPI_Initialize();

        const bool isInitSuccessful = initializationResult == NVAPI_OK;
        DUSK_ASSERT( isInitSuccessful, "Failed to initialize NvAPI (error code %i)\n", initializationResult );
       
        DUSK_LOG_INFO( "NvAPI succesfully initialized!\n" );
        renderContext->IsNvApiLoaded = isInitSuccessful;
    } 
#endif

#if DUSK_USE_AGS
    if ( !DisableVendorExtensions && bestVendorId == dk::render::AMD_VENDOR_ID ) {
		AGSGPUInfo gpuInfo;
		AGSConfiguration config = {};
        AGSReturnCode initializationResult = agsInit( &renderContext->AgsContext, &config, &gpuInfo );

		const bool isInitSuccessful = initializationResult == AGS_SUCCESS;
		DUSK_ASSERT( isInitSuccessful, "Failed to initialize AMD AGS (error code %i)\n", initializationResult );

        DUSK_LOG_INFO( "AGS Library initialized: v%d.%d.%d\n", gpuInfo.agsVersionMajor, gpuInfo.agsVersionMinor, gpuInfo.agsVersionPatch );
        DUSK_LOG_INFO( "Radeon Software Version:   %hs\n", gpuInfo.radeonSoftwareVersion );
        DUSK_LOG_INFO( "Driver Version:            %hs\n", gpuInfo.driverVersion );

		renderContext->IsAmdAgsLoaded = true;
    }
#endif

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

    NativeDisplaySurface* surface = displaySurface.getNativeDisplaySurface();

    DUSK_LOG_INFO( "Available Refresh Rate:\n" );
    for ( u32 i = 0; i < outputDisplayModeCount; ++i ) {
        UINT refreshRate = ( displayModeList[i].RefreshRate.Numerator / displayModeList[i].RefreshRate.Denominator );

        if ( displayModeList[i].Width == surface->WindowWidth
          && displayModeList[i].Height == surface->WindowHeight ) {
            DUSK_LOG_INFO( "\t- %u Hz\n", refreshRate );

            // If we have a refresh rate matching the one desired; stop here.
            if ( desiredRefreshRate != -1 && refreshRate == desiredRefreshRate ) {
                vsyncNumerator = displayModeList[i].RefreshRate.Numerator;
                vsyncDenominator = displayModeList[i].RefreshRate.Denominator;
                vsyncRefreshRate = refreshRate;
                break;
            } else if ( refreshRate > vsyncRefreshRate ) {
                vsyncNumerator = displayModeList[i].RefreshRate.Numerator;
                vsyncDenominator = displayModeList[i].RefreshRate.Denominator;
                vsyncRefreshRate = refreshRate;
            }
        }
    }

    // Update this device refresh rate
    this->refreshRate = vsyncRefreshRate;

    DUSK_LOG_INFO( "Selected Display Mode >> %ux%u @ %uHz\n",
        surface->WindowWidth, surface->WindowHeight, vsyncRefreshRate );

    output->Release();
    factory->Release();

    const eDisplayMode displayMode = displaySurface.getDisplayMode();
    const BOOL isWindowed = ( displayMode == eDisplayMode::WINDOWED || displayMode == eDisplayMode::BORDERLESS );

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {
        {																					// DXGI_MODE_DESC BufferDesc
            surface->WindowWidth,												    //		UINT Width
            surface->WindowHeight,												    //		UINT Height
        {																				//		DXGI_RATIONAL RefreshRate
            vsyncNumerator,																//			UINT Numerator			
            vsyncDenominator,															//			UINT Denominator
        },																				//
        DXGI_FORMAT_R8G8B8A8_UNORM,										                //		DXGI_FORMAT Format
        DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,											//		DXGI_MODE_SCANLINE_ORDER ScanlineOrdering
        DXGI_MODE_SCALING_UNSPECIFIED,													//		DXGI_MODE_SCALING Scaling
        },																					//
        {																					// DXGI_SAMPLE_DESC SampleDesc
            1,																				//		UINT Count
            0,																				//		UINT Quality
        },																					//
        DXGI_USAGE_RENDER_TARGET_OUTPUT,													// DXGI_USAGE BufferUsage
        1,																					// UINT BufferCount
        surface->Handle,														// HWND OutputWindow
        isWindowed,	// BOOL Windowed
        DXGI_SWAP_EFFECT_DISCARD,															// DXGI_SWAP_EFFECT SwapEffect
        0,																					// UINT Flags
    };

    UINT creationFlags = 0;

#if DUSK_DEVBUILD
    if ( useDebugContext ) {
        DUSK_LOG_INFO( "Debug context requested: D3D11_CREATE_DEVICE_DEBUG flag has been set\n" );
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif

    ID3D11DeviceContext*	nativeDeviceContext = nullptr;
    ID3D11Device*			nativeDevice = nullptr;
    IDXGISwapChain*			swapChain = nullptr;

    static constexpr D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT nativeDeviceCreationResult = D3D11CreateDeviceAndSwapChain(
        adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        creationFlags,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain,
        &nativeDevice,
        NULL,
        &nativeDeviceContext
    );

    DUSK_LOG_INFO( "D3D11CreateDeviceAndSwapChain >> Operation result: 0x%x\n", nativeDeviceCreationResult );

#if DUSK_DEVBUILD
    if ( nativeDevice == nullptr ) {
        DUSK_LOG_WARN( "Missing Windows SDK! Recreating device without D3D11_CREATE_DEVICE_DEBUG flag...\n" );

        creationFlags = 0u;

        static constexpr D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        const HRESULT nativeDeviceCreationResult = D3D11CreateDeviceAndSwapChain(
            adapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            creationFlags,
            &featureLevel,
            1,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain,
            &nativeDevice,
            NULL,
            &nativeDeviceContext
        );

        DUSK_LOG_INFO( "D3D11CreateDeviceAndSwapChain >> Operation result: 0x%x\n", nativeDeviceCreationResult );
    }
#endif

    // It should be safe to release the adapter info after the device creation
    adapter->Release();

    renderContext->PhysicalDevice = nativeDevice;
    renderContext->ImmediateContext = nativeDeviceContext;
    renderContext->SwapChain = swapChain;

    swapChainImage = dk::core::allocate<Image>( memoryAllocator );
    swapChainImage->texture2D = nullptr;
    swapChainImage->Description.width = surface->WindowWidth;
    swapChainImage->Description.height = surface->WindowHeight;
    swapChainImage->Description.arraySize = 1;
    swapChainImage->Description.format = VIEW_FORMAT_R8G8B8A8_UNORM;
    swapChainImage->Description.dimension = ImageDesc::DIMENSION_2D;
    swapChainImage->Description.depth = 1;
    swapChainImage->Description.mipCount = 1;

    // Allocate backbuffer texels.
    resizeBackbuffer( surface->WindowWidth, surface->WindowHeight );

#if DUSK_ENABLE_GPU_DEBUG_MARKER
    // Retrieve debug marker module
    renderContext->ImmediateContext->QueryInterface( IID_PPV_ARGS( &renderContext->Annotation ) );
#endif

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
            CommandList* cmdList = dk::core::allocate<CommandList>( graphicsCmdListAllocator[i], CommandList::Type::GRAPHICS );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->ImmediateContext = nativeDeviceContext;
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
            CommandList* cmdList = dk::core::allocate<CommandList>( computeCmdListAllocator[i], CommandList::Type::COMPUTE );
            cmdList->initialize( memoryAllocator, j );

            NativeCommandList* nativeCmdList = cmdList->getNativeCommandList();
            nativeCmdList->ImmediateContext = nativeDeviceContext;
        }

        computeCmdListAllocator[i]->clear();
    }

}

void RenderDevice::enableVerticalSynchronisation( const bool enabled )
{
    renderContext->SynchronisationInterval = ( enabled ) ? 1 : 0;
}

CommandList& RenderDevice::allocateGraphicsCommandList()
{
    const size_t bufferIdx = frameIndex % PENDING_FRAME_COUNT;

    // Reset the allocator (go back to the start of the allocator memory space)
    if ( graphicsCmdListAllocator[bufferIdx]->getAllocationCount() == CMD_LIST_POOL_CAPACITY ) {
        graphicsCmdListAllocator[bufferIdx]->clear();
    }
    CommandList* cmdList = static_cast< CommandList* >( graphicsCmdListAllocator[bufferIdx]->allocate( sizeof( CommandList ) ) );
    cmdList->setFrameIndex( static_cast< i32 >( frameIndex ) );

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
    cmdList->setFrameIndex( static_cast< i32 >( frameIndex ) );

    return *cmdList;
}

CommandList& RenderDevice::allocateCopyCommandList()
{
    return allocateGraphicsCommandList();
}

void UpdateBuffer_Replay( ID3D11DeviceContext* immediateContext, ID3D11Resource* resource, const void* data, const size_t dataSize )
{
    D3D11_MAPPED_SUBRESOURCE mappedSubResource;
    HRESULT operationResult = immediateContext->Map( resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubResource );
    if ( SUCCEEDED( operationResult ) ) {
        memcpy( mappedSubResource.pData, data, dataSize );
        immediateContext->Unmap( resource, 0 );
    } else {
        DUSK_LOG_ERROR( "Failed to map buffer! (error code: 0x%x)\n", operationResult );
    }
}

void RenderDevice::submitCommandList( CommandList& cmdList )
{
    // Replay recorded commands
    NativeCommandList* nativeCmdList = cmdList.getNativeCommandList();

    while ( !nativeCmdList->Commands.empty() ) {
        u32* bufferPointer = nativeCmdList->Commands.front();
        nativeCmdList->Commands.pop();

        CommandPacketIdentifier identifier = *reinterpret_cast<CommandPacketIdentifier*>( bufferPointer );
        switch ( identifier ) {
        case CPI_BIND_VERTEX_BUFFER:
        {
            CommandPacket::BindVertexBuffer cmdPacket = *( CommandPacket::BindVertexBuffer* )bufferPointer;
            renderContext->ImmediateContext->IASetVertexBuffers( cmdPacket.StartBindIndex, cmdPacket.BufferCount, cmdPacket.Buffers, cmdPacket.Strides, cmdPacket.Offsets );
            break;
        }
        case CPI_BIND_INDICE_BUFFER:
        {
            CommandPacket::BindIndiceBuffer cmdPacket = *( CommandPacket::BindIndiceBuffer* )bufferPointer;

            // Check if we need to unbind the indice buffer prior to binding.
            ClearBufferUAVRegister( renderContext, cmdPacket.Buffer );
            FlushUAVRegisterUpdate( renderContext );

            ClearBufferSRVRegister( renderContext, cmdPacket.Buffer );
            FlushSRVRegisterUpdate( renderContext );

            renderContext->ImmediateContext->IASetIndexBuffer( cmdPacket.Buffer->BufferObject, cmdPacket.ViewFormat, cmdPacket.Offset );
            break;
        }
        case CPI_UPDATE_BUFFER:
        {
            CommandPacket::UpdateBuffer cmdPacket = *( CommandPacket::UpdateBuffer* )bufferPointer;
            UpdateBuffer_Replay( renderContext->ImmediateContext, cmdPacket.BufferObject, cmdPacket.Data, cmdPacket.DataSize );
            break;
        }
        case CPI_SETUP_FRAMEBUFFER:
        {
            CommandPacket::SetupFramebuffer cmdPacket = *( CommandPacket::SetupFramebuffer* )bufferPointer;
            SetupFramebuffer_Replay( renderContext, cmdPacket.RenderTargetView, cmdPacket.DepthStencilView );
            break;
        }
        case CPI_BIND_PIPELINE_STATE:
        {
            CommandPacket::BindPipelineState cmdPacket = *( CommandPacket::BindPipelineState* )bufferPointer;
            BindPipelineState_Replay( renderContext, cmdPacket.PipelineStateObject );
            break;
        }
        case CPI_PREPARE_AND_BIND_RESOURCES:
        {
            PrepareAndBindResources_Replay( renderContext, renderContext->BindedPipelineState );
            break;
        }
        case CPI_BIND_CBUFFER:
        {
            CommandPacket::BindConstantBuffer cmdPacket = *( CommandPacket::BindConstantBuffer* )bufferPointer;
            BindCBuffer_Replay( renderContext, cmdPacket.ObjectHashcode, cmdPacket.BufferObject );
            break;
        }
        case CPI_BIND_IMAGE:
        {
            CommandPacket::BindResource cmdPacket = *( CommandPacket::BindResource* )bufferPointer;
            BindImage_Replay( renderContext, cmdPacket.ImageObject, cmdPacket.ViewKey, cmdPacket.ObjectHashcode );
            break;
        }
        case CPI_BIND_BUFFER:
        {
            CommandPacket::BindResource cmdPacket = *( CommandPacket::BindResource* )bufferPointer;

            if ( cmdPacket.BufferObject != nullptr ) {
                BindBuffer_Replay( renderContext, cmdPacket.ObjectHashcode, cmdPacket.BufferObject );
            }
            break;
		}
		case CPI_BIND_SAMPLER:
		{
			CommandPacket::BindResource cmdPacket = *( CommandPacket::BindResource* )bufferPointer;

			if ( cmdPacket.SamplerObject != nullptr ) {
				BindSampler_Replay( renderContext, cmdPacket.ObjectHashcode, cmdPacket.SamplerObject );
			}
			break;
		}
        case CPI_SET_VIEWPORT:
        {
            CommandPacket::SetViewport cmdPacket = *( CommandPacket::SetViewport* )bufferPointer;

            Viewport& viewport = cmdPacket.ViewportObject;
            if ( renderContext->ActiveViewport != viewport ) {
                D3D11_VIEWPORT d3dViewport = {
                    static_cast< f32 >( viewport.X ),
                    static_cast< f32 >( viewport.Y ),
                    static_cast< f32 >( viewport.Width ),
                    static_cast< f32 >( viewport.Height ),
                    viewport.MinDepth,
                    viewport.MaxDepth,
                };
                renderContext->ImmediateContext->RSSetViewports( 1, &d3dViewport );

                renderContext->ActiveViewport = viewport;
            }
            break;
        }
        case CPI_SET_SCISSOR:
        {
            CommandPacket::SetScissor cmdPacket = *( CommandPacket::SetScissor* )bufferPointer;

            ScissorRegion& scissor = cmdPacket.ScissorObject;
            if ( renderContext->ActiveScissor != scissor ) {
                D3D11_RECT d3dScissorRegion = {
                    static_cast< LONG >( scissor.Left ),
                    static_cast< LONG >( scissor.Top ),
                    static_cast< LONG >( scissor.Right ),
                    static_cast< LONG >( scissor.Bottom )
                };

                renderContext->ImmediateContext->RSSetScissorRects( 1, &d3dScissorRegion );
                renderContext->ActiveScissor = scissor;
            }
            break;
        }
        case CPI_DRAW:
        {
            CommandPacket::Draw cmdPacket = *( CommandPacket::Draw* )bufferPointer;
            if ( cmdPacket.InstanceCount > 1 ) {
                renderContext->ImmediateContext->DrawInstanced( cmdPacket.VertexCount, cmdPacket.InstanceCount, cmdPacket.VertexOffset, cmdPacket.InstanceOffset );
            } else {
                renderContext->ImmediateContext->Draw( cmdPacket.VertexCount, cmdPacket.VertexOffset );
            }
            break;
        }
        case CPI_DRAW_INDEXED:
        {
            CommandPacket::DrawIndexed cmdPacket = *( CommandPacket::DrawIndexed* )bufferPointer;
            if ( cmdPacket.InstanceCount > 1 ) {
                renderContext->ImmediateContext->DrawIndexedInstanced( cmdPacket.IndexCount, cmdPacket.InstanceCount, cmdPacket.IndexOffset, cmdPacket.VertexOffset, cmdPacket.InstanceOffset );
            } else {
                renderContext->ImmediateContext->DrawIndexed( cmdPacket.IndexCount, cmdPacket.IndexOffset, cmdPacket.VertexOffset );
            }
            break;
        }
        case CPI_DISPATCH:
        {
            CommandPacket::Dispatch cmdPacket = *( CommandPacket::Dispatch* )bufferPointer;
            renderContext->ImmediateContext->Dispatch( cmdPacket.ThreadCountX, cmdPacket.ThreadCountY, cmdPacket.ThreadCountZ );
            break;
        }
        case CPI_PUSH_EVENT:
        {
            CommandPacket::PushEvent cmdPacket = *( CommandPacket::PushEvent* )bufferPointer;
            renderContext->Annotation->BeginEvent( cmdPacket.EventName );

#if DUSK_DEVBUILD
            renderContext->ActiveDebugMarker = cmdPacket.EventName;
#endif
            break;
        }
        case CPI_POP_EVENT:
        {
            renderContext->Annotation->EndEvent();

#if DUSK_DEVBUILD
            renderContext->ActiveDebugMarker = nullptr;
#endif
            break;
        }
        case CPI_CLEAR_IMAGES:
        {
            CommandPacket::ClearImage cmdPacket = *( CommandPacket::ClearImage* )bufferPointer;

            for ( u32 i = 0; i < cmdPacket.ImageCount; i++ ) {
                renderContext->ImmediateContext->ClearRenderTargetView( cmdPacket.RenderTargetView[i]->DefaultRenderTargetView, cmdPacket.ClearValue );
            }
            break;
        }
        case CPI_CLEAR_DEPTH_STENCIL:
        {
            CommandPacket::ClearDepthStencil cmdPacket = *( CommandPacket::ClearDepthStencil* )bufferPointer;

            UINT ClearValue = D3D11_CLEAR_DEPTH;
            if ( cmdPacket.ClearStencil ) {
                ClearValue |= D3D11_CLEAR_STENCIL;
            }

            renderContext->ImmediateContext->ClearDepthStencilView( cmdPacket.DepthStencil->DefaultDepthStencilView, ClearValue, cmdPacket.ClearValue, cmdPacket.ClearStencilValue );
            break;
        }
        case CPI_COPY_RESOURCE:
		{
			CommandPacket::CopyResource cmdPacket = *( CommandPacket::CopyResource* )bufferPointer;

			renderContext->ImmediateContext->CopyResource( cmdPacket.DestResource, cmdPacket.SourceResource );
			break;
        }
        case CPI_MULTI_DRAW_INDEXED_INSTANCED_INDIRECT:
        {
			CommandPacket::MultiDrawIndexedInstancedIndirect cmdPacket = *( CommandPacket::MultiDrawIndexedInstancedIndirect* )bufferPointer;

#if DUSK_USE_NVAPI
            if ( renderContext->IsNvApiLoaded ) {
                NvAPI_D3D11_MultiDrawIndexedInstancedIndirect( renderContext->ImmediateContext, cmdPacket.DrawCount, cmdPacket.ArgsBuffer, cmdPacket.BufferAlignmentInBytes, cmdPacket.ArgumentsSizeInBytes );
            } else
#endif

#if DUSK_USE_AGS
			if ( renderContext->IsAmdAgsLoaded ) {
				agsDriverExtensionsDX11_MultiDrawIndexedInstancedIndirect( renderContext->AgsContext, renderContext->ImmediateContext, cmdPacket.DrawCount, cmdPacket.ArgsBuffer, cmdPacket.BufferAlignmentInBytes, cmdPacket.ArgumentsSizeInBytes );
            } else
#endif
            {
				for ( u32 i = 0; i < cmdPacket.DrawCount; i++ ) {
                    renderContext->ImmediateContext->DrawIndexedInstancedIndirect( cmdPacket.ArgsBuffer, cmdPacket.BufferAlignmentInBytes + cmdPacket.ArgumentsSizeInBytes * i );
				}
            }
			break;
        }

        case CPI_COPY_IMAGE:
        {
            CommandPacket::CopyImage cmdPacket = *( CommandPacket::CopyImage* )bufferPointer;
            renderContext->ImmediateContext->CopySubresourceRegion( cmdPacket.DestImage, cmdPacket.DestResourceIndex, 0, 0, 0, cmdPacket.SourceImage, cmdPacket.SourceResourceIndex, nullptr );
            break;
        }

        case CPI_RESOLVE_IMAGE:
        {
            CommandPacket::ResolveImage cmdPacket = *( CommandPacket::ResolveImage* )bufferPointer;
            renderContext->ImmediateContext->ResolveSubresource( cmdPacket.DestImage, 0, cmdPacket.SourceImage, 0, cmdPacket.Format );
            break;
        }
        };
    }
}

void RenderDevice::submitCommandLists( CommandList** cmdLists, const u32 cmdListCount )
{
    // In D3D11, we can't bulk cmdlist submit so this is the best we can do (for now)
    for ( u32 i = 0u; i < cmdListCount; i++ ) {
        CommandList& cmdList = *cmdLists[i];
        submitCommandList( cmdList );
    }
}

void RenderDevice::present()
{
    HRESULT swapBufferResult = renderContext->SwapChain->Present( renderContext->SynchronisationInterval, 0 );
    if ( FAILED( swapBufferResult ) ) {
        DUSK_LOG_ERROR( "Failed to swap buffers! (error code : 0x%x)\n", swapBufferResult );

        DUSK_RAISE_FATAL_ERROR( swapBufferResult != DXGI_ERROR_DEVICE_REMOVED, "Direct3D11 device removed! (error code: 0x%X). The application will now shutdown.", renderContext->PhysicalDevice->GetDeviceRemovedReason() );
    }

    frameIndex = ( ++frameIndex % std::numeric_limits<size_t>::max() );
}

void RenderDevice::waitForPendingFrameCompletion()
{
    renderContext->ImmediateContext->Flush();
}

const dkChar_t* RenderDevice::getBackendName()
{
    return DUSK_STRING( "Direct3D11" );
}

void RenderDevice::resizeBackbuffer( const u32 width, const u32 height )
{
    DUSK_LOG_DEBUG( "Received resize event: new size %ux%u\n", width, height );

    ID3D11Device* physicalDevice = renderContext->PhysicalDevice;
    ID3D11DeviceContext* immediateContext = renderContext->ImmediateContext;
    IDXGISwapChain* swapchain = renderContext->SwapChain;

    // Unbind backbuffer
    if ( swapChainImage->DefaultRenderTargetView != nullptr ) {
        swapChainImage->DefaultRenderTargetView->Release();
        swapChainImage->DefaultRenderTargetView = nullptr;
    }
    if ( swapChainImage->texture2D != nullptr ) {
        swapChainImage->texture2D->Release();
        swapChainImage->texture2D = nullptr;
    }

    immediateContext->OMSetRenderTargets( 0, 0, 0 );
    immediateContext->Flush();

    HRESULT resizeResult = swapchain->ResizeBuffers( 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0 );
    DUSK_ASSERT( SUCCEEDED( resizeResult ), "ResizeBuffers failed! (error code: 0x%x)", resizeResult );

    // Recreate the texture resource.
    ID3D11Texture2D* backbufferTex2D = nullptr;
    swapchain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&backbufferTex2D );

    swapChainImage->texture2D = backbufferTex2D;
    swapChainImage->Description.width = width;
    swapChainImage->Description.height = height;

    // Create backbuffer RTV.
    ImageViewDesc dummyView;
    swapChainImage->DefaultRenderTargetView = CreateImageRenderTargetView( physicalDevice, *swapChainImage, dummyView );
}
#endif
