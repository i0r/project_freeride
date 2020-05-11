/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
#include "RenderDocHelper.h"

#include <Core/StringHelpers.h>
#include <Core/Environment.h>
#include <Core/LibraryRuntime.h>

#include <Maths/Helpers.h>

#include <Rendering/RenderDevice.h>
#include <Core/Display/DisplaySurface.h>

#if DUSK_WIN
#include <Core/Display/DisplaySurfaceWin32.h>

DUSK_INLINE RENDERDOC_WindowHandle GetDisplaySurfaceNativePointer( const DisplaySurface& displaySurface )
{
    return reinterpret_cast< RENDERDOC_WindowHandle >( displaySurface.getNativeDisplaySurface()->Handle );
}
#elif DUSK_UNIX
#include <Core/Display/DisplaySurfaceXcb.h>

DUSK_INLINE RENDERDOC_WindowHandle GetDisplaySurfaceNativePointer( const DisplaySurface& displaySurface )
{
    return reinterpret_cast< RENDERDOC_WindowHandle >( displaySurface.getNativeDisplaySurface()->WindowInstance );
}
#endif

#if DUSK_D3D12
#include <Rendering/Direct3D12/RenderDevice.h>

DUSK_INLINE RENDERDOC_DevicePointer GetRenderDevicePointer( const RenderDevice& renderDevice )
{
    return reinterpret_cast< const RENDERDOC_DevicePointer >( renderDevice.getRenderContext()->device );
}
#elif DUSK_D3D11
#include <Rendering/Direct3D11/RenderDevice.h>

DUSK_INLINE RENDERDOC_DevicePointer GetRenderDevicePointer( const RenderDevice& renderDevice )
{
    return reinterpret_cast< const RENDERDOC_DevicePointer >( renderDevice.getRenderContext()->PhysicalDevice );
}
#elif DUSK_VULKAN
#include <Rendering/Vulkan/RenderDevice.h>

DUSK_INLINE RENDERDOC_DevicePointer GetRenderDevicePointer( const RenderDevice& renderDevice )
{
    return reinterpret_cast< const RENDERDOC_DevicePointer >( RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE( renderDevice.getRenderContext()->instance ) );
}
#endif

RenderDocHelper::RenderDocHelper()
    : renderDocLibrary( nullptr )
    , renderDocAPI( nullptr )
    , pendingFrameCountToCapture( 0u )
{

}

RenderDocHelper::~RenderDocHelper()
{
    pendingFrameCountToCapture = 0u;

    if ( renderDocAPI != nullptr ) {
        dk::core::FreeRuntimeLibrary( renderDocLibrary );
        renderDocLibrary = nullptr;
    }
}

void RenderDocHelper::create()
{
#if DUSK_WIN
    static constexpr dkChar_t* LIBRARY_NAME = DUSK_STRING( "renderdoc.dll" );
#elif DUSK_UNIX
    static constexpr const dkChar_t* LIBRARY_NAME = DUSK_STRING( "librenderdoc.so" );
#else
#error "Platform not implemented!"
#endif

    renderDocLibrary = dk::core::LoadRuntimeLibrary( LIBRARY_NAME );

    pRENDERDOC_GetAPI GetAPI = reinterpret_cast< pRENDERDOC_GetAPI >(
        dk::core::LoadFuncFromLibrary( renderDocLibrary, "RENDERDOC_GetAPI" ) );

    DUSK_RAISE_FATAL_ERROR( renderDocLibrary, "Failed to find renderdoc library in your working directory!\n"
                                                        "(add renderdoc.dll to your working directory or change UseRenderDocCapture envar)" );

    DUSK_RAISE_FATAL_ERROR( GetAPI, "Failed to retrieve RenderDoc API entrypoint! (corrupt lib?)" );

    i32 operationResult = GetAPI( eRENDERDOC_API_Version_1_4_0, reinterpret_cast< void** >( &renderDocAPI ) );
    DUSK_ASSERT( ( operationResult == 1 ), "Failed to retrieve RenderDoc API (error code %i)", operationResult )

    i32 major; i32 minor; i32 patch;
    renderDocAPI->GetAPIVersion( &major, &minor, &patch );

    DUSK_LOG_INFO( "RenderDoc library successfully loaded! API version %i.%i.%i\n", major, minor, patch );

    renderDocAPI->UnloadCrashHandler();

    renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_AllowVSync, 1 );
    renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_AllowFullscreen, 1 );
    renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_CaptureAllCmdLists, 1 );
    renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_RefAllResources, 1 );
    renderDocAPI->SetCaptureOptionU32( eRENDERDOC_Option_DebugOutputMute, 1 );

    renderDocAPI->MaskOverlayBits( eRENDERDOC_Overlay_Default, eRENDERDOC_Overlay_Default );

    RENDERDOC_InputButton captureButtons[1] = { RENDERDOC_InputButton::eRENDERDOC_Key_F11 };
    renderDocAPI->SetCaptureKeys( captureButtons, 1 );
}

void RenderDocHelper::attachTo( const DisplaySurface & displaySurface, const RenderDevice & renderDevice )
{
    RENDERDOC_WindowHandle windowHandle = GetDisplaySurfaceNativePointer( displaySurface );
    RENDERDOC_DevicePointer devicePointer = GetRenderDevicePointer( renderDevice );
    renderDocAPI->SetActiveWindow( windowHandle, devicePointer );

    dkString_t workingDirectory;
    dk::core::RetrieveWorkingDirectory( workingDirectory );

    dkString_t basePath = workingDirectory + DUSK_STRING( "captures/DuskEngine_" );
    basePath += renderDevice.getBackendName();
    renderDocAPI->SetCaptureFilePathTemplate( WideStringToString( basePath ).c_str() );
}

void RenderDocHelper::triggerCapture( const u32 frameCountToCapture )
{
    pendingFrameCountToCapture = frameCountToCapture;

    if ( frameCountToCapture > 1u ) {
        renderDocAPI->TriggerMultiFrameCapture( frameCountToCapture );
    } else {
        renderDocAPI->TriggerCapture();
    }
}

bool RenderDocHelper::openLatestCapture()
{
    char captureFilename[DUSK_MAX_PATH];

    u32 captureIdx = Max( 0u, ( renderDocAPI->GetNumCaptures() - 1u ) );
    bool hasCapturedSomething = ( renderDocAPI->GetCapture( captureIdx, captureFilename, nullptr, nullptr ) == 1 );

    if ( hasCapturedSomething ) {
        renderDocAPI->LaunchReplayUI( 0u, captureFilename );
    }

    return hasCapturedSomething;
}
#endif
#endif
