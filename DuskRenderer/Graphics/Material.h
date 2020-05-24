/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Material 
{
public:
    enum class RenderScenario {
        Default,
        DepthOnly
    };

    static constexpr i32 MAX_LAYER_COUNT = 3;

public:
            Material( BaseAllocator* allocator, const dkChar_t* name = DUSK_STRING( "DefaultMaterial" ) );
            ~Material();

    void    deserialize( FileSystemObject* object );

    // Bind this material for a certain rendering scenario (depth only, lighting, etc.).
    void    bindForScenario( const RenderScenario scenario );
};
