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
        Default_Editor,
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
    // Return the pso required to render the given Scenario (or null if the scenario is invalid/unavailable/etc.).
    PipelineState* bindForScenario( const RenderScenario scenario, CommandList* cmdList, PipelineStateCache* psoCache, const u32 samplerCount = 1u );

    // Return the name of this material.
    const char* getName() const;

    // Return true if a parameter with the given hashcode exists, and if this parameter has been declared as mutable
    // (i.e. this parameter can be modified at runtime).
    bool        isParameterMutable( const dkStringHash_t parameterHashcode ) const;

    // Invalidate cached pipeline state and will force a full pso/resource rebuild the next time this material is binded.
    // This call is pretty slow, so call it only when required (e.g. if you want to update a material being edited in the
    // editor).
    void        invalidateCache();

private:
    enum class MutableParamType {
        Float3,
    };

private:
    // Name of this material.
    std::string  name;

    // Hashmap holding each mutable parameter.
    std::unordered_map<dkStringHash_t, MutableParamType> mutableParameters;

    // Pipeline binding for the default render scenario (forward+ light pass).
    RenderScenarioBinding defaultScenario;

    // Pipeline binding for the default render scenario in the material editor (forward+ light pass).
    RenderScenarioBinding defaultEditorScenario;

    // If true, this material will invalidate previously cached states (pipeline states, shaders, etc.) and will request
    // the resources from the hard drive.
    u8 invalidateCachedStates : 1;

    u8 isAlphaBlended : 1;

    u8 isDoubleFace : 1;

    u8 enableAlphaToCoverage : 1;

    u8 isAlphaTested : 1;
};
