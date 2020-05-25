/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Graphics/Material.h>

class BaseAllocator;
class GraphicsAssetCache;

enum ShadingModel : i32 {
    Default = 0,
    ClearCoat,

    Count,
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
    union {
        f32         AsFloat;
        u32         AsUnsignedInteger;
        i32         AsSignedInteger;
    };

    union {
        dkVec3f     AsFloat3;
        dkVec3i     AsUnsignedInteger3;
        dkVec3u     AsSignedInteger3;
    };

    struct {
        Image*      TextureInstance;
        dkString_t  PathToTextureAsset;
    } AsTexture;

    struct {
        char*           Content;
        BaseAllocator*  Allocator;
    } AsCodePiece;

    MaterialAttribute()
        : Type( InputType::Unused )
        , AsFloat( 0.0f )
        , AsFloat3( 0.0f, 0.0f, 0.0f )
        , AsTexture{ nullptr, dkString_t() }
        , AsCodePiece{ nullptr, nullptr }
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
        , AlphaCutoff( 0.33f )
        , DiffuseContribution( 1.0f )
        , SpecularContribution( 1.0f )
        , NormalContribution( 1.0f )
    {
        BlendMask.Type = MaterialAttribute::Constant_1D;
        BlendMask.AsFloat = 0.50f;
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
    {

    }
};

class MaterialEditor 
{
public:
            MaterialEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache );
            ~MaterialEditor();

#if DUSK_USE_IMGUI
    // Display MaterialEditor panel (as a ImGui window).
    void    displayEditorWindow();
#endif

    // Open the Material Editor window.
    void    openEditorWindow();

    // Set the material to edit.
    void    setActiveMaterial( Material* material );

private:
    // True if the editor window is opened in the editor workspace.
    bool        isOpened;

    // Editable instance of the active material.
    EditableMaterial    editedMaterial;

    // Pointer to the material instance being edited.
    Material*           activeMaterial;

    // Memory allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // Pointer to the active instance of the Graphics Assets cache.
    GraphicsAssetCache* graphicsAssetCache;

private:
    // The maximum length of a code piece attribute (in characters count).
    static constexpr u32 MAX_CODE_PIECE_LENGTH = 1024;

private:
    // Display GUI for a given Material Attribute.
    // SaturateInput can be used to clamp the input between 0 and 1 (for scalar and integer inputs).
    template<bool SaturateInput>
    void                displayMaterialAttribute( const char* displayName, MaterialAttribute& attribute );
};
