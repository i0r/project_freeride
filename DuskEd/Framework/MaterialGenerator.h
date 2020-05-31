/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class VirtualFileSystem;
class RuntimeShaderCompiler;

#include <Graphics/Material.h>
#include "EditableMaterial.h"

class MaterialGenerator 
{
public:
    struct GeneratedMaterial {
        // Describe the binding for a mutable parameter.
        struct MutableParameter {
            // The name of the parameter (written as is in the serialized asset).
            std::string ParameterName;

            // Its default value set at creation time.
            dkVec3f     DefaultValueAsFloat3;
        };

        // Describe the binding for a Rendering scenario.
        struct ScenarioBinding {
            // Hashcode of the vertex shader name.
            std::string VertexShaderName;

            // Hashcode of the pixel shader name.
            std::string PixelShaderName;
        
            // Array of mutable parameters required for the given rendering scenario.
            std::vector<MutableParameter> MutableParameters;
        };

        // Name of this material.
        std::string     MaterialName;

        // Material version.
        u32             Version;

        // Light (Forward+) rendering scenario binding.
        ScenarioBinding DefaultBinding;

        // Depth only (shadow, depth prepass, etc.) rendering scenario binding.
        ScenarioBinding DepthOnlyBinding;

        // True if this material require alpha testing.
        u8 EnableAlphaTest : 1;

        // True if this material require alpha to coverage.
        u8 EnableAlphaToCoverage : 1;

        // True if this material is double face (will disable backface culling).
        u8 IsDoubleFace : 1;

        // True if this material use alpha blending.
        u8 EnableAlphaBlend : 1;
    };

public:
                        MaterialGenerator( BaseAllocator* allocator, VirtualFileSystem* virtualFileSystem );
                        ~MaterialGenerator();

    // Create an editable material instance from a previously generated material instance.
    EditableMaterial    createEditableMaterial( const Material* material );

    // Create a runtime material instance from an editable material instance. Return null if the creation failed.
    GeneratedMaterial   createMaterial( const EditableMaterial& editableMaterial );

private:
    // The string building the HLSL code to retrieve a material layer attributes.
    std::string         materialLayersGetter;

    // The string building the HLSL code storing custom code piece (each code piece is a free standing function).
    std::string         materialSharedCode;

    // The string building the resource list for attribute fetching.
    std::string         materialResourcesCode;

    // The number of custom function used for the active generated material.
    i32                 attributeGetterCount;

    // The number of texture resource view used for the active generated material.
    i32                 texResourceCount;

    // Pointer to the active VirtualFileSystem instance.
    VirtualFileSystem*  virtualFs;

    // Allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // A local thread-safe instance of the shader compiler (for runtime shader recompiling).
    RuntimeShaderCompiler* shaderCompiler;
    
private:
    // Clear/Reset internal states for Material generation (string builders; indexes; etc.).
    void                resetMaterialTemporaryOutput();

    // Append the HLSL code to blend two Material layer.
    // The blend result will be stored in the bottom layer (since the material layering
    // works in a stack fashion).
    void                appendLayerBlend( const EditableMaterialLayer& bottomLayer, const EditableMaterialLayer& topLayer, const char* bottomLayerName, const char* topLayerName );

    // Append the HLSL code to blend two Material attribute with an additive blend mode.
    // Note that the blend is done at runtime.
    void                appendAttributeBlendAdditive( const char* attributeName, const char* bottomLayerName, const char* topLayerName, const f32 contributionFactor = 1.0f );

    // Append the HLSL code to blend two Material attribute with an multiplicative blend mode.
    // Note that the blend is done at runtime.
    void                appendAttributeBlendMultiplicative( const char* attributeName, const char* bottomLayerName, const char* topLayerName, const f32 contributionFactor = 1.0f );

    // Append the HLSL code to retrieve a given Material layer.
    // It only does the retrieval; layers blending is not included!
    void                appendLayerRead( const char* layerName, const EditableMaterialLayer& layer );

    // Append the HLSL code to retrieve a given MaterialAttribute 1D value depending on its type (either 
    // hardcoded static value, sample a given texture or fetch cbuffer attribute if mutable).
    void                appendAttributeFetch1D( const MaterialAttribute& attribute );

    // Append the HLSL code to retrieve a given MaterialAttribute 2D value depending on its type (either 
    // hardcoded static value, sample a given texture or fetch cbuffer attribute if mutable).
    void                appendAttributeFetch2D( const MaterialAttribute& attribute );

    // Append the HLSL code to retrieve a given MaterialAttribute 3D value depending on its type (either 
    // hardcoded static value, sample a given texture or fetch cbuffer attribute if mutable).
    void                appendAttributeFetch3D( const MaterialAttribute& attribute );

    // Append the HLSL code to retrieve a given MaterialAttribute 4D value depending on its type (either 
    // hardcoded static value, sample a given texture or fetch cbuffer attribute if mutable).
    void                appendAttributeFetch4D( const MaterialAttribute& attribute );
};
