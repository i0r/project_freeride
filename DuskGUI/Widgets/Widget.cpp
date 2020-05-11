/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Widget.h"

GUIWidget::GUIWidget()
    : VirtualPosition( 0.0f, 0.0f )
    , VirtualSize( 0.0f, 0.0f )
    , Position( 0.0f, 0.0f )
    , Size( 0.0f, 0.0f )
{

}

GUIWidget::~GUIWidget()
{
    VirtualPosition = dkVec2f( 0.0f, 0.0f );
    VirtualSize = dkVec2f( 0.0f, 0.0f );
    Position = dkVec2f( 0.0f, 0.0f );
    Size = dkVec2f( 0.0f, 0.0f );
}

void GUIWidget::onScreenSizeChange( const dkVec2f& updatedVirtualToScreenSpaceFactor )
{
    Position = VirtualPosition * updatedVirtualToScreenSpaceFactor;
    Size = VirtualSize * updatedVirtualToScreenSpaceFactor;
}

void GUIWidget::setScreenPosition( const dkVec2f& screenSpacePosition )
{
    Position = screenSpacePosition + Size;
}
