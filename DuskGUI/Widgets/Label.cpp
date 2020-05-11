/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Label.h"

#include <Graphics/DrawCommandBuilder.h>

GUILabel::GUILabel()
    : GUIWidget()
    , Value( "" )
    , ColorAndAlpha( 1.0f, 1.0f, 1.0f, 1.0f )
{

}

GUILabel::~GUILabel()
{
    Value.clear();
    ColorAndAlpha = dkVec4f( 0.0f, 0.0f, 0.0f, 0.0f );
}

void GUILabel::collectDrawCmds( DrawCommandBuilder& drawCmdBuilder )
{
    drawCmdBuilder.addHUDText( Position, Size.x, ColorAndAlpha, Value );
}
