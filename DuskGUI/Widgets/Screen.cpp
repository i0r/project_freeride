/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Screen.h"

#include <Graphics/DrawCommandBuilder.h>

#include "Widget.h"
#include "Panel.h"

GUIScreen::GUIScreen( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , virtualScreenSize( 1280u, 720u )
    , screenSize( 1280u, 720u )
{

}

GUIScreen::~GUIScreen()
{
    memoryAllocator = nullptr;
}

void GUIScreen::setVirtualScreenSize( const dkVec2u& virtualScreenSizeInPixels )
{
    virtualScreenSize = virtualScreenSize;
    virtualScreenSize = dkVec2u::max( virtualScreenSize, dkVec2u( 1u ) );
}

void GUIScreen::onScreenResize( const dkVec2u& screenSizeInPixels )
{
    screenSize = screenSizeInPixels;

    dkVec2f virtualToScreenRatio = dkVec2f( screenSize ) / dkVec2f( virtualScreenSize );
    for ( GUIPanel* panel : panels ) {
        panel->onScreenSizeChange( virtualToScreenRatio );
    } 

    for ( GUIWidget* widget : widgets ) {
        widget->onScreenSizeChange( virtualToScreenRatio );
    }
}

void GUIScreen::collectDrawCmds( DrawCommandBuilder& drawCmdBuilder )
{
    for ( GUIPanel* panel : panels ) {
        panel->collectDrawCmds( drawCmdBuilder );
    }

    for ( GUIWidget* widget : widgets ) {
        widget->collectDrawCmds( drawCmdBuilder );
    }
}

void GUIScreen::onMouseCoordinatesUpdate( const double mouseX, const double mouseY )
{
    for ( GUIPanel* panel : panels ) {
        panel->onMouseCoordinatesUpdate( mouseX, mouseY );
    }
}

void GUIScreen::onLeftMouseButtonDown( const double mouseX, const double mouseY )
{
    for ( GUIPanel* panel : panels ) {
        panel->onMouseButtonDown( mouseX, mouseY );
    }
}

void GUIScreen::onLeftMouseButtonUp()
{
    for ( GUIPanel* panel : panels ) {
        panel->onMouseButtonUp();
    }
}
