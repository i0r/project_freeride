/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Material;
class VirtualFileSystem;

#include "EditableMaterial.h"

class MaterialGenerator 
{
public:
                        MaterialGenerator( VirtualFileSystem* virtualFileSystem );
                        ~MaterialGenerator();

    // Create an editable material instance from a previously generated material instance.
    EditableMaterial    createEditableMaterial( const Material* material );

    // Create a runtime material instance from an editable material instance. Return null if the creation failed.
    Material*           createMaterial( const EditableMaterial& editableMaterial );

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
