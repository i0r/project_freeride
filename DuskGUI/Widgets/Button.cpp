/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Button.h"

#include <Graphics/DrawCommandBuilder.h>

GUIButton::GUIButton()
    : GUIPanel()
{

}

GUIButton::~GUIButton()
{

}

void GUIButton::onMouseButtonDown( const double mouseX, const double mouseY )
{
    if ( isMouseInside ) return;

    GUIPanel::onMouseButtonDown( mouseX, mouseY );

    if ( isMouseInside )
        *Value = !*Value;
}

void GUIButton::onMouseButtonUp()
{
    GUIPanel::onMouseButtonUp();
}

void GUIButton::onMouseCoordinatesUpdate( const double mouseX, const double mouseY )
{

}
