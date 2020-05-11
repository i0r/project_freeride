/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class Material;

#include "Widget.h"

#include <vector>

class GUIPanel : public GUIWidget
{
public:
    bool                    IsDraggable;
    bool                    IsResizable;
    bool                    IsScrollable;

    Material*               PanelMaterial;

public:
                            GUIPanel();
                            GUIPanel( GUIPanel& widget ) = default;
                            GUIPanel& operator = ( GUIPanel& widget ) = default;
                            ~GUIPanel();

    virtual void            onMouseButtonDown( const double mouseX, const double mouseY );
    virtual void            onMouseButtonUp();
    virtual void            onMouseCoordinatesUpdate( const double mouseX, const double mouseY );

    void                    addChild( GUIWidget* widget );
    void                    collectDrawCmds( DrawCommandBuilder& drawCmdBuilder ) override;
    void                    setScreenPosition( const dkVec2f& screenSpacePosition ) override;

protected:
    bool                    isMouseInside;

private:
    dkVec2f                mousePressedCoordinates;
    std::vector<GUIWidget*> children;
};
