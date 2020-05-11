/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class Material;

#include "Widget.h"
#include <string>

class GUILabel : public GUIWidget
{
public:
    std::string Value;
    dkVec4f     ColorAndAlpha;

public:
                GUILabel();
                GUILabel( GUILabel& widget ) = default;
                GUILabel& operator = ( GUILabel& widget ) = default;
                ~GUILabel();

    void        collectDrawCmds( DrawCommandBuilder& drawCmdBuilder ) override;
};
