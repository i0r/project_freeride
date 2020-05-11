/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class DrawCommandBuilder;
class GUIWidget;
class GUIPanel;

#include <Maths/Vector.h>
#include <vector>

class GUIScreen
{
public:
                            GUIScreen( BaseAllocator* allocator );
                            GUIScreen( GUIScreen& widget ) = default;
                            GUIScreen& operator = ( GUIScreen& widget ) = default;
                            ~GUIScreen();

    void                    setVirtualScreenSize( const dkVec2u& virtualScreenSizeInPixels );
    void                    onScreenResize( const dkVec2u& screenSizeInPixels );

    template<typename T = GUIPanel>
    T& allocatePanel()
    {
        dkVec2f virtualToScreenRatio = dkVec2f( screenSize ) / dkVec2f( virtualScreenSize );

        T* panel = dk::core::allocate<T>( memoryAllocator );
        panels.push_back( panel );

        panel->onScreenSizeChange( virtualToScreenRatio );

        return *panel;
    }

    template<typename T> T* allocateWidget()
    {
        T* widget = dk::core::allocate<T>( memoryAllocator );
        widgets.push_back( widget );
        return widget;
    }

    void                    collectDrawCmds( DrawCommandBuilder& drawCmdBuilder );

    void                    onMouseCoordinatesUpdate( const double mouseX, const double mouseY );
    void                    onLeftMouseButtonDown( const double mouseX, const double mouseY );
    void                    onLeftMouseButtonUp();

private:
    BaseAllocator*          memoryAllocator;
    dkVec2u                virtualScreenSize;
    dkVec2u                screenSize;

    std::vector<GUIWidget*> widgets;
    std::vector<GUIPanel*>  panels;
};
