/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <unordered_map>

#include <Maths/Vector.h>
#include <Graphics/PipelineStateCache.h>

class GraphicsAssetCache;

class Material 
{
public:
    enum class RenderScenario {
        // Default Geometry Render Scenario. This scenario assumes you wanna render world geometry (e.g. static mesh).
        Default,

        // Same as Default, except that you can dynamically update layers and attributes of this material. This scenario
        // has an obvious performance cost.
        Default_Editor,

        // Default scenario; withing writing to the UAV buffer enabled.
        Default_Picking,

        // Default_Editor scenario; withing writing to the UAV buffer enabled.
        Default_Picking_Editor,

        // Depth only render pass. It will output non-linearized depth.
        Depth_Only,

        // Do not use.
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

    void            deserialize( FileSystemObject* object );

    // Bind this material for a certain rendering scenario (depth only, lighting, etc.).
    void            bindForScenario( const RenderScenario scenario, CommandList* cmdList, PipelineStateCache* psoCache, const u32 samplerCount = 1u );
        
    // Return the name of this material.
    const char*     getName() const;

    // Return true if a parameter with the given hashcode exists, and if this parameter has been declared as mutable
    // (i.e. this parameter can be modified at runtime).
    bool            isParameterMutable( const dkStringHash_t parameterHashcode ) const;

    // Invalidate cached pipeline state and will force a full pso/resource rebuild the next time this material is binded.
    // This call is pretty slow, so call it only when required (e.g. if you want to update a material being edited in the
    // editor).
    void            invalidateCache();

    // Ensure resources required to render this material are loaded in memory.
    void            updateResourceStreaming( GraphicsAssetCache* graphicsAssetCache );

    void            setParameterAsTexture2D( const dkStringHash_t parameterHashcode, const std::string& imagePath );

    // Return true if this material skip lighting step of the world rendering (e.g. is shadeless).
    bool            skipLighting() const;

    // Return true if this material is a shadow emitter and should be included in the shadow map rendering
    // false otherwise.
    bool            castShadow() const;

private:
    struct MutableParameter {
        // Cached Parameter value converted to Float3.
        dkVec3f Float3Value;

        // Pointer to the cached instance for this texture. Careful: images are usually streamed,
        // so you have to make sure the image is valid and in memory when accessing this field!
        Image* CachedImageAsset;

        // The type of this parameter. Required to figure out how to interpret the parameter.
        enum class ParamType {
            Unused,
            Float3,
            Texture2D
        } Type;
        
        // The "raw" value for this mutable parameter (aka the parsed value).
        std::string Value;
    
        MutableParameter()
            : Float3Value( dkVec3f::Zero )
            , CachedImageAsset( nullptr )
            , Type( ParamType::Unused )
            , Value( "" )
        {

        }
    };

    struct ParameterBinding {
        dkStringHash_t  ParameterHashcode;
        Image*          ImageAsset;
    };

private:
    // Name of this material.
    std::string name;

    // Hashmap holding each mutable parameter.
    std::unordered_map<dkStringHash_t, MutableParameter> mutableParameters;

    // Pipeline binding for the default render scenario (forward+ light pass).
    RenderScenarioBinding defaultScenario;

    // Pipeline binding for the default render scenario in the material editor (forward+ light pass).
    RenderScenarioBinding defaultEditorScenario;

    // (EDITOR ONLY) defaultScenario when picking is required.
    RenderScenarioBinding defaultPickingScenario;

    // (EDITOR ONLY) defaultEditorScenario when picking is required.
    RenderScenarioBinding defaultPickingEditorScenario;

	// Pipeline binding for depth only render scenario (depth prepass or shadow capture).
	RenderScenarioBinding depthOnlyScenario;

    // If true, this material will invalidate previously cached states (pipeline states, shaders, etc.) and will request
    // the resources from the hard drive.
    u8 invalidateCachedStates : 1;

    // True if this material use alpha blending.
    u8 isAlphaBlended : 1;

    // True if this material is double faced.
    u8 isDoubleFace : 1;

    // True if this material is alpha tested and needs alpha to coverage.
    u8 enableAlphaToCoverage : 1;

    // True if this material is alpha tested.
    u8 isAlphaTested : 1;

    // True if this material should be rendered with wireframe fillmode.
    u8 isWireframe : 1;

    // True if this material is shadeless and don't need a lighting pass.
    u8 isShadeless : 1;

private:
    const PipelineStateCache::ShaderBinding& Material::getScenarioShaderBinding( const RenderScenario scenario ) const;
};
