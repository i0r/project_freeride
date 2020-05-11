/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Panel.h"

#include <Graphics/DrawCommandBuilder.h>

GUIPanel::GUIPanel()
    : GUIWidget()
    , IsDraggable( false )
    , IsResizable( false )
    , IsScrollable( false )
    , isMouseInside( false )
    , PanelMaterial( nullptr )
{
    mousePressedCoordinates = dkVec2f( 0.0f, 0.0f );
}

GUIPanel::~GUIPanel()
{
    IsDraggable = false;
    IsResizable = false;
    IsScrollable = false;

    isMouseInside = false;

    mousePressedCoordinates = dkVec2f( 0.0f, 0.0f );
}

void GUIPanel::onMouseButtonDown( const double mouseX, const double mouseY )
{
    if ( isMouseInside ) {
        return;
    }

    auto panelMin = ( Position - Size );
    auto panelMax = ( Position + Size );

    const bool isMouseInsideX = ( mouseX >= panelMin.x && mouseX <= panelMax.x );
    const bool isMouseInsideY = ( mouseY >= panelMin.y && mouseY <= panelMax.y );

    isMouseInside = ( isMouseInsideX && isMouseInsideY );
    mousePressedCoordinates = Position - dkVec2f( static_cast<float>( mouseX ), static_cast<float>( mouseY ) );
}

void GUIPanel::onMouseButtonUp()
{
    isMouseInside = false;
}

void GUIPanel::onMouseCoordinatesUpdate( const double mouseX, const double mouseY )
{
    if ( IsDraggable && isMouseInside ) {
        Position = mousePressedCoordinates + dkVec2f( static_cast< float >( mouseX ), static_cast< float >( mouseY ) );

        for ( GUIWidget* child : children ) {
            child->setScreenPosition( Position - Size + ( child->VirtualPosition * Size * 2.0f ) );
        }
    }
}

void GUIPanel::addChild( GUIWidget* widget )
{
    widget->setScreenPosition( Position - Size + ( widget->VirtualPosition * Size * 2.0f ) );
    children.push_back( widget );
}

void GUIPanel::collectDrawCmds( DrawCommandBuilder& drawCmdBuilder )
{
    drawCmdBuilder.addHUDRectangle( Position, Size, 0.0f, PanelMaterial );

    for ( GUIWidget* child : children ) {
        child->collectDrawCmds( drawCmdBuilder );
    }
}

void GUIPanel::setScreenPosition( const dkVec2f& screenSpacePosition )
{
    GUIWidget::setScreenPosition( screenSpacePosition );

    // Propagate screen position update to children
    for ( GUIWidget* child : children ) {
        child->setScreenPosition( Position - Size + ( child->VirtualPosition * Size * 2.0f ) );
    }
}
