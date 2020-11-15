/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_DEVBUILD
#include "ImGuiManager.h"

#include <Core/Display/DisplaySurface.h>
#include <Core/Environment.h>
#include <Core/StringHelpers.h>

#if DUSK_WIN
#include <Core/Display/DisplaySurfaceWin32.h>
#elif DUSK_UNIX
#include <Core/Display/DisplaySurfaceXcb.h>
#endif

#include "FileSystem/VirtualFileSystem.h"

// TODO Expose this in the build script?
#define DUSKED_USE_MATERIAL_DESIGN_FONT 1

#if DUSKED_USE_MATERIAL_DESIGN_FONT
#include "ThirdParty/Google/IconsMaterialDesign.h"
#endif

#include "ThirdParty/imgui/imgui.h"

ImGuiManager::ImGuiManager()
    : memoryAllocator( nullptr )
    , iconFontTexels( nullptr )
    , settingsFilePath( "" )
{

}

ImGuiManager::~ImGuiManager()
{
    if ( iconFontTexels != nullptr ) {
        dk::core::freeArray( memoryAllocator, iconFontTexels );
    }
}

void ImGuiManager::create( const DisplaySurface& displaySurface, VirtualFileSystem* virtualFileSystem, BaseAllocator* allocator )
{
    dkString_t saveFolderPath;
    dk::core::RetrieveSavedGamesDirectory( saveFolderPath );
    settingsFilePath = DUSK_NARROW_STRING( saveFolderPath );
    settingsFilePath.append( "/DuskEd/imgui.ini" );

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();

	static const ImWchar icons_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
	ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;

    // Load font and forward it to imgui internal.
    FileSystemObject* iconFontTTF = virtualFileSystem->openFile( DUSK_STRING( "GameData/fonts/MaterialIcons-Regular.ttf" ), eFileOpenMode::FILE_OPEN_MODE_READ | eFileOpenMode::FILE_OPEN_MODE_BINARY );
	if ( iconFontTTF->isGood() ) {
        memoryAllocator = allocator;

		const u64 contentLength = iconFontTTF->getSize();
        iconFontTexels = dk::core::allocateArray<u8>( memoryAllocator, contentLength );
        iconFontTTF->read( iconFontTexels, contentLength * sizeof( u8 ) );
		iconFontTTF->close();
        
        io.Fonts->AddFontFromMemoryTTF( iconFontTexels, static_cast<i32>( contentLength ), 12.0f, &icons_config, icons_ranges );
    } else {
        DUSK_LOG_WARN( "Missing font resources '%hs'...\n", FONT_ICON_FILE_NAME_MD );
    }

    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "DuskEd::ImGuiManager";
    io.IniFilename = settingsFilePath.c_str();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    resize( displaySurface.getWidth(), displaySurface.getHeight() );

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    for ( i32 i = 0; i < ImGuiKey_COUNT; i++ ) {
        io.KeyMap[i] = i;
    }

#if DUSK_WIN
    HWND nativeHandle = displaySurface.getNativeDisplaySurface()->Handle;

    main_viewport->PlatformHandle = nativeHandle;
    main_viewport->PlatformHandleRaw = nativeHandle;
#endif
    
    // Setup style
    ImGui::StyleColorsDark();

    //ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 0.41f, 0.41f, 0.41f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_BorderShadow, ImVec4( 0.41f, 0.41f, 0.41f, 1.0f ) );

    //ImGui::PushStyleColor( ImGuiCol_Separator, ImVec4( 0.14f, 0.14f, 0.14f, 1.0f ) );

    //ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.38f, 0.38f, 0.38f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_FrameBgActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_TitleBgActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_SeparatorActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );

    //ImGui::PushStyleColor( ImGuiCol_ScrollbarGrab, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.8f, 0.8f, 0.8f, 1.0f ) );
    //ImGui::PushStyleColor( ImGuiCol_CheckMark, ImVec4( 0.1f, 0.1f, 0.1f, 1.0f ) );

    //ImGui::PushStyleColor( ImGuiCol_TitleBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    //ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    //ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    //ImGui::PushStyleColor( ImGuiCol_MenuBarBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    //ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );

    //ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    //ImGui::PushStyleColor( ImGuiCol_PopupBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );

    //ImGui::PushStyleColor( ImGuiCol_ScrollbarBg, ImVec4( 0.24f, 0.24f, 0.24f, 1.0f ) );

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
}

void ImGuiManager::update( const f32 deltaTime )
{
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = deltaTime;
    
    if ( !( io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange ) ) {
        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();

#if DUSK_WIN
        if ( imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor ) {
            // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
            ::SetCursor( NULL );
        } else {
            // Show OS mouse cursor
            LPTSTR win32_cursor = IDC_ARROW;

            switch ( imgui_cursor ) {
            case ImGuiMouseCursor_Arrow:        
                win32_cursor = IDC_ARROW; 
                break;
            case ImGuiMouseCursor_TextInput:    
                win32_cursor = IDC_IBEAM; 
                break;
            case ImGuiMouseCursor_ResizeAll:    
                win32_cursor = IDC_SIZEALL; 
                break;
            case ImGuiMouseCursor_ResizeEW:     
                win32_cursor = IDC_SIZEWE; 
                break;
            case ImGuiMouseCursor_ResizeNS:     
                win32_cursor = IDC_SIZENS; 
                break;
            case ImGuiMouseCursor_ResizeNESW:   
                win32_cursor = IDC_SIZENESW; 
                break;
            case ImGuiMouseCursor_ResizeNWSE:   
                win32_cursor = IDC_SIZENWSE; 
                break;
            case ImGuiMouseCursor_Hand:         
                win32_cursor = IDC_HAND; 
                break;
            }

            ::SetCursor( ::LoadCursor( NULL, win32_cursor ) );
        }
#endif
    }
}

void ImGuiManager::setVisible( const bool isVisible )
{
    ImGuiIO& io = ImGui::GetIO();
    io.UserData = (void*)isVisible;
}

void ImGuiManager::resize( const u32 screenWidth, const u32 screenHeight )
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = static_cast< f32 >( screenWidth );
    io.DisplaySize.y = static_cast< f32 >( screenHeight );
}
#endif
