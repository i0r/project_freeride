/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <unordered_map>

class Material 
{
public:
    enum class RenderScenario {
        Default,
        DepthOnly,
        Count
    };

    static constexpr i32 MAX_LAYER_COUNT = 4;

public:
                Material( BaseAllocator* allocator, const dkChar_t* materialName = DUSK_STRING( "DefaultMaterial" ) );
                ~Material();

    void        deserialize( FileSystemObject* object );

    // Bind this material for a certain rendering scenario (depth only, lighting, etc.).
    void        bindForScenario( const RenderScenario scenario );

    // Return the name of this material.
    const dkString_t& getName() const;

    // Return true if a parameter with the given hashcode exists, and if this parameter has been declared as mutable
    // (i.e. this parameter can be modified at runtime).
    bool        isParameterMutable( const dkStringHash_t parameterHashcode ) const;

private:
    enum class MutableParamType {
        Float3,
    };

private:
    // Name of this material.
    dkString_t  name;

    // Hashmap holding each mutable parameter.
    std::unordered_map<dkStringHash_t, MutableParamType> mutableParameters;
};
