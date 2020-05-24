/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Material 
{
public:
    Material( BaseAllocator* allocator, const dkChar_t* name = DUSK_STRING( "DefaultMaterial" ) );
    ~Material();
};
