/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Panel.h"

class GUIButton : public GUIPanel
{
public:
    bool*           Value;
    
public:
                    GUIButton();
                    GUIButton( GUIButton& widget ) = default;
                    GUIButton& operator = ( GUIButton& widget ) = default;
                    ~GUIButton();

    void            onMouseButtonDown( const double mouseX, const double mouseY ) override;
    void            onMouseButtonUp() override;
    void            onMouseCoordinatesUpdate( const double mouseX, const double mouseY ) override;
};
