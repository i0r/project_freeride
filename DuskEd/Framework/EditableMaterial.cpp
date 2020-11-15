/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "EditableMaterial.h"

#include "Io/TextStreamHelpers.h"
#include "Core/Parser.h"
#include "MaterialEditor.h" // Required for MaterialEditor::MAX_CODE_PIECE_LENGTH
#include "Graphics/GraphicsAssetCache.h"

static constexpr const char* LAYER_BLEND_MODE_LUT[LayerBlendMode::BlendModeCount] = {
    "Additive",
    "Multiplicative"
};

static constexpr const char* SHADING_MODEL_LUT[ShadingModel::ShadingModel_Count] = {
    "Default",
    "ClearCoat"
};

// TODO Move this to a shared header (already exist in MaterialGenerator.cpp).
void AppendFlag( std::string& output, const char* flagName, const bool flagValue )
{
    output.append( flagName );
    output.append( " = " );
    output.append( ( flagValue ) ? "true" : "false" );
    output.append( ";\n" );
}

void AppendMaterialAttribute( std::string& output, const char* attributeName, const MaterialAttribute& attribute )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Constant_1D:
        output.append( attributeName );
        output.append( " = " );
        output.append( std::to_string( attribute.AsFloat ) );
        output.append( ";\n" );
        break;
    case MaterialAttribute::Mutable_3D: // Encode the default vector value { scalar, scalar, scalar }.
        output.append( attributeName );
        output.append( " = {" );
        output.append( std::to_string( attribute.AsFloat3[0] ) );
        output.append( ", " );
        output.append( std::to_string( attribute.AsFloat3[1] ) );
        output.append( ", " );
        output.append( std::to_string( attribute.AsFloat3[2] ) );
        output.append( " };\n" );
        break;
    case MaterialAttribute::Constant_3D: // Encode the constant value as a RGB color #RRGGBB (TODO We might want something more accurate in the future...)
        output.append( attributeName );
        output.append( " = #" );
        output.append( dk::core::ToHexString<u8>( static_cast< u8 >( attribute.AsFloat3[0] * 255.0f ) ) );
        output.append( dk::core::ToHexString<u8>( static_cast< u8 >( attribute.AsFloat3[1] * 255.0f ) ) );
        output.append( dk::core::ToHexString<u8>( static_cast< u8 >( attribute.AsFloat3[2] * 255.0f ) ) );
        output.append( ";\n" );
        break;
    case MaterialAttribute::Texture_2D:
        output.append( attributeName );
        output.append( " = \"" );
        output.append( attribute.AsTexture.IsSRGBSpace ? "srgb|" : "linear|" );
        output.append( DUSK_NARROW_STRING( attribute.AsTexture.PathToTextureAsset ).c_str() );
        output.append( "\";\n" );
        break;
    case MaterialAttribute::Code_Piece:
        output.append( attributeName );
        output.append( " = {\"" );
        output.append( attribute.AsCodePiece.Content );
        output.append( "\"};\n" );
        break;
    case MaterialAttribute::Unused:
    default:
        break;
    }
}

void AppendEditableLayer( std::string& output, const EditableMaterialLayer& layer )
{
    output.append( "\tlayer {\n" );

    AppendMaterialAttribute( output, "\t\tBaseColor", layer.BaseColor );
    AppendMaterialAttribute( output, "\t\tReflectance", layer.Reflectance );
    AppendMaterialAttribute( output, "\t\tRoughness", layer.Roughness );
    AppendMaterialAttribute( output, "\t\tMetalness", layer.Metalness );
    AppendMaterialAttribute( output, "\t\tEmissivity", layer.Emissivity );
    AppendMaterialAttribute( output, "\t\tAmbientOcclusion", layer.AmbientOcclusion );
    AppendMaterialAttribute( output, "\t\tNormal", layer.Normal );
    AppendMaterialAttribute( output, "\t\tBlendMask", layer.BlendMask );
    AppendMaterialAttribute( output, "\t\tAlphaMask", layer.AlphaMask );
    AppendMaterialAttribute( output, "\t\tClearCoat", layer.ClearCoat );
    AppendMaterialAttribute( output, "\t\tClearCoatGlossiness", layer.ClearCoatGlossiness );

    output.append( "\t\tScale = {" );
    output.append( std::to_string( layer.Scale[0] ) );
    output.append( ", " );
    output.append( std::to_string( layer.Scale[1] ) );
    output.append( " };\n" );

    output.append( "\t\tOffset = {" );
    output.append( std::to_string( layer.Offset[0] ) );
    output.append( ", " );
    output.append( std::to_string( layer.Offset[1] ) );
    output.append( " };\n" );

    output.append( "\t\tBlendMode = " );
    output.append( LAYER_BLEND_MODE_LUT[layer.BlendMode] );
    output.append( ";\n" );

    output.append( "\t\tAlphaCutoff = " );
    output.append( std::to_string( layer.AlphaCutoff ) );
    output.append( ";\n" );

    output.append( "\t\tDiffuseContribution = " );
    output.append( std::to_string( layer.DiffuseContribution ) );
    output.append( ";\n" );

    output.append( "\t\tSpecularContribution = " );
    output.append( std::to_string( layer.SpecularContribution ) );
    output.append( ";\n" );

    output.append( "\t\tNormalContribution = " );
    output.append( std::to_string( layer.NormalContribution ) );
    output.append( ";\n" );

    output.append( "\t}\n" );
}

void ReadMaterialAttribute( const std::string& value, MaterialAttribute& attribute, BaseAllocator* allocator, GraphicsAssetCache* graphicsAssetCache )
{
    if ( value.empty() || value == "none" || value == "null" ) {
        attribute.Type = MaterialAttribute::Unused;
        return;
    }

    // Check if the input is mutable (either a vector or a code piece).
    if ( value.front() == '{' && value.back() == '}' ) {
        // HLSL code piece format : { "%s" }
        // Mutable vector format: { %f, %f, %f }
        if ( value[1] == '\"' && value[value.size() - 2] == '\"' ) {
            attribute.Type = MaterialAttribute::Code_Piece;
            
            attribute.AsCodePiece.Allocator = allocator;
            attribute.AsCodePiece.Content = dk::core::allocateArray<char>( allocator, MaterialEditor::MAX_CODE_PIECE_LENGTH );

            // We subtract 4 to take in account the braces and quotes.
            memcpy( attribute.AsCodePiece.Content, &value[2], sizeof( char ) * ( value.size() - 4 ) );
        } else {
            attribute.Type = MaterialAttribute::Mutable_3D;
            attribute.AsFloat3 = dk::io::StringTo3DVector( value );
        }
    } else if ( value.front() == '#' && value.length() == 7 ) {
        // Constant 3D format : #RRGGBB (no explicit alpha since this is a separate attribute).
        f32 redChannel = std::stoul( value.substr( 1, 2 ).c_str(), nullptr, 16 ) / 255.0f;
        f32 greenChannel = std::stoul( value.substr( 3, 2 ).c_str(), nullptr, 16 ) / 255.0f;
        f32 blueChannel = std::stoul( value.substr( 5, 2 ).c_str(), nullptr, 16 ) / 255.0f;

        attribute.Type = MaterialAttribute::Constant_3D;
        attribute.AsFloat3 = dkVec3f( redChannel, greenChannel, blueChannel );
    } else if ( value.front() == '\"' && value.back() == '\"' ) {
        // Texture path format: "%s|%s"
        attribute.Type = MaterialAttribute::Texture_2D;
        
        // We store the colorspace of the texture before the path to the texture itself (kinda crappy; I guess we might
        // want something more solid in the future).
        size_t colorSpaceSeparatorOff = value.find_first_of( '|' );

        std::string colorSpace = value.substr( 0, colorSpaceSeparatorOff );
        std::string path = value.substr( colorSpaceSeparatorOff + 1 );

        attribute.AsTexture.IsSRGBSpace = ( colorSpace == "srgb" );
        attribute.AsTexture.PathToTextureAsset = StringToDuskString( path.c_str() );
        attribute.AsTexture.TextureInstance = graphicsAssetCache->getImage( attribute.AsTexture.PathToTextureAsset.c_str(), true );
    } else {
        attribute.Type = MaterialAttribute::Constant_1D;
        attribute.AsFloat = std::stof( value );
    }
}

EditableMaterial::EditableMaterial()
{
    clear();

    // We have at least one layer (the root layer).
    LayerCount = 1;
}

void EditableMaterial::serialize( FileSystemObject* stream ) const
{
    // Serialize to a string then write the stream to the string (reduces the i/o work).
    std::string generatedAsset;

    generatedAsset.append( "editable_material \"" );
    generatedAsset.append( Name );
    generatedAsset.append( "\" {\n" );

    generatedAsset.append( "\tVersion = " );
    generatedAsset.append( std::to_string( EditableMaterial::VERSION ) );
    generatedAsset.append( ";\n" );

    generatedAsset.append( "\tShadingModel = " );
    generatedAsset.append( SHADING_MODEL_LUT[SModel] );
    generatedAsset.append( ";\n" );

    AppendFlag( generatedAsset, "\tIsDoubleFace", IsDoubleFace );
    AppendFlag( generatedAsset, "\tIsAlphaTested", IsAlphaTested );
    AppendFlag( generatedAsset, "\tIsAlphaBlended", IsAlphaBlended );
    AppendFlag( generatedAsset, "\tUseAlphaToCoverage", UseAlphaToCoverage );
    AppendFlag( generatedAsset, "\tWriteVelocity", WriteVelocity );
    AppendFlag( generatedAsset, "\tReceiveShadow", ReceiveShadow );
    AppendFlag( generatedAsset, "\tScaleUVByModelScale", ScaleUVByModelScale );
    AppendFlag( generatedAsset, "\tUseRefraction", UseRefraction );
    AppendFlag( generatedAsset, "\tIsWireframe", IsWireframe );
    AppendFlag( generatedAsset, "\tIsShadeless", IsShadeless );


    for ( i32 layerIdx = 0; layerIdx < LayerCount; layerIdx++ ) {
        AppendEditableLayer( generatedAsset, Layers[layerIdx] );
    }

    generatedAsset.append( "}\n" );

    // Write content to stream.
    stream->writeString( generatedAsset );
}

void EditableMaterial::loadFromFile( FileSystemObject* stream, BaseAllocator* allocator, GraphicsAssetCache* graphicsAssetCache )
{
    // Make sure the material is empty.
    clear();

    // Load and parse the content from the stream.
    std::string assetStr;
    dk::io::LoadTextFile( stream, assetStr );

    Parser parser( assetStr.c_str() );
    parser.generateAST();

     // Iterate over each node of the AST.
    const u32 typeCount = parser.getTypeCount();
    for ( u32 t = 0; t < typeCount; t++ ) {
        const TypeAST& typeAST = parser.getType( t );
        switch ( typeAST.Type ) {
        case TypeAST::eTypes::EDITABLE_MATERIAL: {
            // TODO Make sure we don't overrun?
            memcpy( Name, typeAST.Name.StreamPointer, typeAST.Name.Length );

            u32 nodeCount = static_cast< u32 >( typeAST.Types.size() );
            DUSK_LOG_DEBUG( "%u types found\n", nodeCount );

            for ( u32 nodeIdx = 0; nodeIdx < nodeCount; nodeIdx++ ) {
                const Token::StreamRef& name = typeAST.Names[nodeIdx];

                // If the node is typeless, assume it is a flag.
                if ( typeAST.Types[nodeIdx] == nullptr ) {
                    std::string value = std::string( typeAST.Values[nodeIdx].StreamPointer, typeAST.Values[nodeIdx].Length );
                    dk::core::StringToLower( value );

                    if ( dk::core::ExpectKeyword( name.StreamPointer, 7, "Version" ) ) {
                        // Will be used in the future.
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 12, "ShadingModel" ) ) {
                        for ( i32 i = 0; i < ShadingModel::ShadingModel_Count; i++ ) {
                            if ( SHADING_MODEL_LUT[i] == value ) {
                                SModel = static_cast< ShadingModel >( i );
                                break;
                            }
                        }
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 12, "IsDoubleFace" ) ) {
                        IsDoubleFace = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 13, "IsAlphaTested" ) ) {
                        IsAlphaTested = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 14, "IsAlphaBlended" ) ) {
                        IsAlphaBlended = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 18, "UseAlphaToCoverage" ) ) {
                        UseAlphaToCoverage = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 13, "WriteVelocity" ) ) {
                        WriteVelocity = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 13, "ReceiveShadow" ) ) {
                        ReceiveShadow = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 19, "ScaleUVByModelScale" ) ) {
                        ScaleUVByModelScale = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 13, "UseRefraction" ) ) {
                        UseRefraction = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "IsWireframe" ) ) {
                        IsWireframe = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "IsShadeless" ) ) {
                        IsShadeless = dk::core::StringToBool( value );
                    }
                } else {
                    const TypeAST& type = *typeAST.Types[nodeIdx];

                    switch ( type.Type ) {
                    case TypeAST::EDITABLE_MATERIAL_LAYER: {
                        EditableMaterialLayer& layer = Layers[LayerCount++];

                        const u32 parameterCount = static_cast< u32 >( type.Names.size() );

                        for ( u32 i = 0; i < parameterCount; i++ ) {
                            std::string value( type.Values[i].StreamPointer, type.Values[i].Length );

                            const Token::StreamRef& name = type.Names[i];

                            MaterialAttribute* attribute = nullptr;
                            if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "BaseColor" ) ) {
                                attribute = &layer.BaseColor;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "Reflectance" ) ) {
                                attribute = &layer.Reflectance;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "Roughness" ) ) {
                                attribute = &layer.Roughness;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "Metalness" ) ) {
                                attribute = &layer.Metalness;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 10, "Emissivity" ) ) {
                                attribute = &layer.Emissivity;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 16, "AmbientOcclusion" ) ) {
                                attribute = &layer.AmbientOcclusion;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "BlendMask" ) ) {
                                attribute = &layer.BlendMask;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "AlphaMask" ) ) {
                                attribute = &layer.AlphaMask;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "ClearCoat" ) ) {
                                attribute = &layer.ClearCoat;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 19, "ClearCoatGlossiness" ) ) {
                                attribute = &layer.ClearCoatGlossiness;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 5, "Scale" ) ) {
                                layer.Scale = dk::io::StringTo2DVector( value );
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 6, "Offset" ) ) {
                                layer.Offset = dk::io::StringTo2DVector( value );
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 9, "BlendMode" ) ) {
                                for ( i32 i = 0; i < LayerBlendMode::BlendModeCount; i++ ) {
                                    if ( LAYER_BLEND_MODE_LUT[i] == value ) {
                                        layer.BlendMode = static_cast< LayerBlendMode >( i );
                                        break;
                                    }
                                }
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "AlphaCutoff" ) ) {
                                layer.AlphaCutoff = std::stof( value );
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 19, "DiffuseContribution" ) ) {
                                layer.DiffuseContribution = std::stof( value );
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 20, "SpecularContribution" ) ) {
                                layer.SpecularContribution = std::stof( value );
                                continue;
                            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 18, "NormalContribution" ) ) {
                                layer.NormalContribution = std::stof( value );
                                continue;
                            }

                            // Check if the attribute is valid.
                            if ( attribute == nullptr ) {
                                DUSK_LOG_WARN( "Invalid attribute found (name: '%hs' value '%hs')\n", std::string(name.StreamPointer, name.Length).c_str(), value.c_str() );
                                continue;
                            }

                            ReadMaterialAttribute( value, *attribute, allocator, graphicsAssetCache );
                        }
                    } break;
                    }
                }
            }
        } break;
        }
    }
}

void EditableMaterial::clear()
{
    SModel = ShadingModel::Default;
    IsDoubleFace = false;
    IsAlphaTested = false;
    IsAlphaBlended = false;
    UseAlphaToCoverage = false;
    WriteVelocity = true;
    ReceiveShadow = true;
    ScaleUVByModelScale = false;
    UseRefraction = false;
    IsWireframe = false;
    IsShadeless = false;
    LayerCount = 0;

    memset( Name, 0, sizeof( char ) * DUSK_MAX_PATH );

    for ( i32 layerIdx = 0; layerIdx < Material::MAX_LAYER_COUNT; layerIdx++ ) {
        Layers[layerIdx] = EditableMaterialLayer();
    }
}
