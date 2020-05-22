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

#include <imgui/imgui.h>

ImGuiManager::ImGuiManager()
    : settingsFilePath( "" )
{

}

ImGuiManager::~ImGuiManager()
{

}

void ImGuiManager::create( const DisplaySurface& displaySurface )
{
    dkString_t saveFolderPath;
    dk::core::RetrieveSavedGamesDirectory( saveFolderPath );
    settingsFilePath = WideStringToString( saveFolderPath );
    settingsFilePath.append( "/DuskEd/imgui.ini" );

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "DuskEd::ImGuiManager";
    io.IniFilename = settingsFilePath.c_str();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiViewport* main_viewport = ImGui::GetMainViewport();

#if DUSK_WIN
    HWND nativeHandle = displaySurface.getNativeDisplaySurface()->Handle;

    main_viewport->PlatformHandle = nativeHandle;
    main_viewport->PlatformHandleRaw = nativeHandle;
#endif
    
    io.DisplaySize.x = static_cast<f32>( displaySurface.getWidth() );
    io.DisplaySize.y = static_cast<f32>( displaySurface.getHeight() );

    // Setup style
    ImGui::StyleColorsDark();

    ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 0.41f, 0.41f, 0.41f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_BorderShadow, ImVec4( 0.41f, 0.41f, 0.41f, 1.0f ) );

    ImGui::PushStyleColor( ImGuiCol_Separator, ImVec4( 0.14f, 0.14f, 0.14f, 1.0f ) );

    ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.38f, 0.38f, 0.38f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_FrameBgActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_TitleBgActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_SeparatorActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );

    ImGui::PushStyleColor( ImGuiCol_ScrollbarGrab, ImVec4( 0.96f, 0.62f, 0.1f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.27f, 0.31f, 0.35f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.8f, 0.8f, 0.8f, 1.0f ) );
    ImGui::PushStyleColor( ImGuiCol_CheckMark, ImVec4( 0.1f, 0.1f, 0.1f, 1.0f ) );

    ImGui::PushStyleColor( ImGuiCol_TitleBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    ImGui::PushStyleColor( ImGuiCol_MenuBarBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );

    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );
    ImGui::PushStyleColor( ImGuiCol_PopupBg, ImVec4( 0.28f, 0.28f, 0.28f, 0.750f ) );

    ImGui::PushStyleColor( ImGuiCol_ScrollbarBg, ImVec4( 0.24f, 0.24f, 0.24f, 1.0f ) );
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
#endif
