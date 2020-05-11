/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class DrawCommandBuilder;

#include <Maths/Vector.h>

class GUIWidget
{
public:
    // Position in virtual coordinates system
    dkVec2f        VirtualPosition;
    // Size in virtual coordinates system
    dkVec2f        VirtualSize;

public:
                    GUIWidget();
                    GUIWidget( GUIWidget& widget ) = default;
                    GUIWidget& operator = ( GUIWidget& widget ) = default;
                    ~GUIWidget();

    virtual void    collectDrawCmds( DrawCommandBuilder& drawCmdBuilder ) = 0;
    void            onScreenSizeChange( const dkVec2f& updatedVirtualToScreenSpaceFactor );

    // Override widget screenspace position
    // It should only be use for specific case (e.g. relative positioning)
    virtual void    setScreenPosition( const dkVec2f& screenSpacePosition );

protected:
    // Position in screen coordinates system
    dkVec2f        Position;
    // Size in screen coordinates system
    dkVec2f        Size;
};
