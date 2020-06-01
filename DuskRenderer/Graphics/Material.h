/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <unordered_map>

#include <Graphics/PipelineStateCache.h>

class Material 
{
public:
    enum class RenderScenario {
        Default,
        DepthOnly,
        Count
    };

    struct RenderScenarioBinding {
        dkString_t VertexStage;
        dkString_t PixelStage;

        PipelineStateCache::ShaderBinding PsoShaderBinding;
    };

    static constexpr i32 MAX_LAYER_COUNT = 4;

public:
                Material( BaseAllocator* allocator );
                ~Material();

    void        deserialize( FileSystemObject* object );

    // Bind this material for a certain rendering scenario (depth only, lighting, etc.).
    void        bindForScenario( const RenderScenario scenario );

    // Return the name of this material.
    const char* getName() const;

    // Return true if a parameter with the given hashcode exists, and if this parameter has been declared as mutable
    // (i.e. this parameter can be modified at runtime).
    bool        isParameterMutable( const dkStringHash_t parameterHashcode ) const;

private:
    enum class MutableParamType {
        Float3,
    };

private:
    // Name of this material.
    std::string  name;

    // Hashmap holding each mutable parameter.
    std::unordered_map<dkStringHash_t, MutableParamType> mutableParameters;

    RenderScenarioBinding defaultScenario;

    u8 isAlphaBlended : 1;

    u8 isDoubleFace : 1;

    u8 enableAlphaToCoverage : 1;

    u8 isAlphaTested : 1;
};
