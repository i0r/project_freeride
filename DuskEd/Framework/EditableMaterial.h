/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Graphics/Material.h>
#include <Maths/Vector.h>

struct Image;

enum ShadingModel : i32 {
    Default = 0,
    ClearCoat,

    ShadingModel_Count,
};

enum LayerBlendMode : i32 {
    Additive = 0,
    Multiplicative,

    BlendModeCount,
};

// An editable material input.
struct MaterialAttribute 
{
    enum InputType : i32 {
        // The attribute is not used. Its definition will be skipped when possible.
        // Otherwise it will use the engine default value.
        Unused,

        // A one dimensional constant (integer or float).
        Constant_1D,

        // A three dimensional constant.
        Constant_3D,

        // A mutable three dimensional vector. The value assigned to this attribute
        // will act as the default value at runtime.
        Mutable_3D,

        // A two dimensional texture sampled at runtime.
        Texture_2D,

        // A piece of HLSL code, baked during material generation.
        Code_Piece,

        // Input type count. Do not use.
        Count,
    };

    // The type of input required for this attribute (defined at compile time).
    InputType       Type;

    // We want to keep every type input active (no memory aliasing).
    // This way the previous user input is not removed whenever the type changes.
    f32         AsFloat;
    dkVec3f     AsFloat3;

    struct {
        Image*      TextureInstance;
        dkString_t  PathToTextureAsset;
        bool        IsSRGBSpace;
    } AsTexture;

    struct {
        char*           Content;
        BaseAllocator*  Allocator;
    } AsCodePiece;

    // Mutable parameter offset (in bytes).
    i32                 CBufferOffset;

    MaterialAttribute()
        : Type( InputType::Unused )
        , AsFloat( 0.0f )
        , AsFloat3( 0.0f, 0.0f, 0.0f )
        , AsTexture{ nullptr, dkString_t(), true }
        , AsCodePiece{ nullptr, nullptr }
        , CBufferOffset( 0 )
    {

    }
};

struct EditableMaterialLayer 
{
    // BRDF input.
    MaterialAttribute   BaseColor;
    MaterialAttribute   Reflectance;
    MaterialAttribute   Roughness;
    MaterialAttribute   Metalness;
    MaterialAttribute   Emissivity;
    MaterialAttribute   AmbientOcclusion;
    MaterialAttribute   Normal;
    MaterialAttribute   BlendMask;
    MaterialAttribute   AlphaMask;
    
    // Scale of the current layer.
    dkVec2f             Scale;

    // Offset of the current layer.
    dkVec2f             Offset;

    // Layer blending mode.
    LayerBlendMode      BlendMode;

    f32                 AlphaCutoff;
    f32                 DiffuseContribution;
    f32                 SpecularContribution;
    f32                 NormalContribution;

    EditableMaterialLayer()
        : BaseColor()
        , Reflectance()
        , Roughness()
        , Metalness()
        , Emissivity()
        , AmbientOcclusion()
        , Normal()
        , BlendMask()
        , AlphaMask()
        , Scale( dkVec2f( 1.0f, 1.0f ) )
        , Offset( dkVec2f( 0.0f, 0.0f ) )
        , BlendMode( LayerBlendMode::Additive )
        , AlphaCutoff( 0.33f )
        , DiffuseContribution( 1.0f )
        , SpecularContribution( 1.0f )
        , NormalContribution( 1.0f )
    {
        BaseColor.Type = MaterialAttribute::Constant_3D;
        BaseColor.AsFloat3 = dkVec3f( 0.50f, 0.50f, 0.50f );

        Reflectance.Type = MaterialAttribute::Constant_1D;
        Reflectance.AsFloat = 0.50f;

        Roughness.Type = MaterialAttribute::Constant_1D;
        Roughness.AsFloat = 0.50f;

        Metalness.Type = MaterialAttribute::Constant_1D;
        Metalness.AsFloat = 0.0f;

        Emissivity.Type = MaterialAttribute::Constant_1D;
        Emissivity.AsFloat = 0.0f;

        AmbientOcclusion.Type = MaterialAttribute::Constant_1D;
        AmbientOcclusion.AsFloat = 1.0f;

        BlendMask.Type = MaterialAttribute::Constant_1D;
        BlendMask.AsFloat = 0.50f;

        AlphaMask.Type = MaterialAttribute::Constant_1D;
        AlphaMask.AsFloat = 1.0f;
    }
};

// A data structure representing an editable material.
// Used by the Material Editor and Compiler.
// Requires a generation step to "runtime" material in order to be usable
// (see MaterialGenerator).
struct EditableMaterial 
{
    // The name of this material.
    char                    Name[DUSK_MAX_PATH];

    // The shading model of this material.
    ShadingModel            SModel;
    
    // Layer Count for this material.
    i32                     LayerCount;

    // Layers for this material.
    EditableMaterialLayer   Layers[Material::MAX_LAYER_COUNT];

    // True if this material is double faced.
    // This flag has an implicit performance cost
    // (disable backface culling; need to rasterize more stuff; ...).
    u8                      IsDoubleFace : 1;

    // True if this material require alpha testing.
    // This flag has an implicit performance cost.
    u8                      IsAlphaTested : 1;

    // True if this material require alpha blending.
    u8                      IsAlphaBlended : 1;

    // True if this material use alpha to coverage.
    // This flag is ignored if IsAlphaTested is false.
    u8                      UseAlphaToCoverage : 1;

    // True if this material need to write its velocity.
    u8                      WriteVelocity : 1;

    // True if this material can receive shadows from other shadow casters.
    u8                      ReceiveShadow : 1;

    // True if the uv maps used by the material should be scaled by the model matrix
    // used to render the geometry.
    u8                      ScaleUVByModelScale : 1;
    
    // True if this material use refraction.
    u8                      UseRefraction : 1;

    // True if this material must be rasterized with a wireframe fill mode.
    u8                      IsWireframe : 1;

	// True if this material ignore shading and lighting. Useful to render
	// special/debug materials.
	u8                      IsShadeless : 1;

    EditableMaterial()
        : SModel( ShadingModel::Default )
        , LayerCount( 1 )
        , Layers()
        , IsDoubleFace( false )
        , IsAlphaTested( false )
        , IsAlphaBlended( false )
        , UseAlphaToCoverage( false )
        , WriteVelocity( true )
        , ReceiveShadow( true )
        , ScaleUVByModelScale( false )
        , UseRefraction( false )
        , IsWireframe( false )
        , IsShadeless( false )
    {
        memset( Name, 0, sizeof( char ) * DUSK_MAX_PATH );
    }
};
