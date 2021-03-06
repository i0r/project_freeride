/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "MaterialEditor.h"

#include "MaterialGenerator.h"

#if DUSK_USE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#endif

#include <Core/FileSystemIOHelpers.h>
#include <Core/StringHelpers.h>
#include <Core/Environment.h>

#include "FileSystem/VirtualFileSystem.h"

#include <Graphics/GraphicsAssetCache.h>
#include <Rendering/RenderDevice.h>

#include <Graphics/ShaderHeaders/MaterialRuntimeEd.h>

static constexpr dkChar_t* DefaultTextureFilter = DUSK_STRING( "All (*.dds, *.jpg, *.png, *.png16, *.tga, *.lpng)\0*.dds;*.jpg;*.png;*.png16;*.tga;*.lpng\0DirectDraw Surface (*.dds)\0*.dds\0JPG (*.jpg)\0*.jpg\0PNG (*.png)\0*.png\0PNG 16 Bits (*.png16)\0*.png16\0Low Precision PNG (*.lpng)\0*.lpng\0TGA (*.tga)\0*.tga\0" );

void FillRuntimeMaterialAttribute( const MaterialAttribute& attribute, MaterialEdAttribute& runtimeAttribute )
{
    switch ( attribute.Type ) {
    case MaterialAttribute::Unused:
        runtimeAttribute.Type = 0;
        break;
    case MaterialAttribute::Constant_1D:
        runtimeAttribute.Type = 1;
        runtimeAttribute.Input.x = attribute.AsFloat;
        break;
    case MaterialAttribute::Constant_3D:
    case MaterialAttribute::Mutable_3D:
        runtimeAttribute.Type = 3;
        runtimeAttribute.Input = attribute.AsFloat3;
        break;
    case MaterialAttribute::Texture_2D:
        runtimeAttribute.Type = 4;
        break;
    }
}

void FillRuntimeMaterialLayer( const EditableMaterialLayer& layer, MaterialEdLayer& runtimeLayer )
{
    FillRuntimeMaterialAttribute( layer.BaseColor, runtimeLayer.BaseColor );
    FillRuntimeMaterialAttribute( layer.Reflectance, runtimeLayer.Reflectance );
    FillRuntimeMaterialAttribute( layer.Roughness, runtimeLayer.Roughness );
    FillRuntimeMaterialAttribute( layer.Metalness, runtimeLayer.Metalness );
    FillRuntimeMaterialAttribute( layer.AmbientOcclusion, runtimeLayer.AmbientOcclusion );
    FillRuntimeMaterialAttribute( layer.Emissivity, runtimeLayer.Emissivity );
    FillRuntimeMaterialAttribute( layer.BlendMask, runtimeLayer.BlendMask );
    FillRuntimeMaterialAttribute( layer.ClearCoat, runtimeLayer.ClearCoat );
    FillRuntimeMaterialAttribute( layer.ClearCoatGlossiness, runtimeLayer.ClearCoatGlossiness );

    runtimeLayer.LayerScale = layer.Scale;
    runtimeLayer.LayerOffset = layer.Offset;

    runtimeLayer.Contribution.BlendMode = layer.BlendMode;
    runtimeLayer.Contribution.DiffuseContribution = layer.DiffuseContribution;
    runtimeLayer.Contribution.SpecularContribution = layer.SpecularContribution;
    runtimeLayer.Contribution.NormalContribution = layer.NormalContribution;
}

MaterialEditor::MaterialEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs )
    : isOpened( false )
    , useInteractiveMode( false )
    , editedMaterial()
    , activeMaterial( nullptr )
    , memoryAllocator( allocator )
    , graphicsAssetCache( gfxCache )
    , virtualFileSystem( vfs )
    , materialGenerator( dk::core::allocate<MaterialGenerator>( allocator, allocator, vfs ) )
    , bufferData{}
{

}

MaterialEditor::~MaterialEditor()
{

}

#if DUSK_USE_IMGUI
void MaterialEditor::displayEditorWindow()
{
#define DUSK_DISPLAY_MATERIAL_FLAG( flagName )\
bool flagName##flag = editedMaterial.flagName;\
if ( ImGui::Checkbox( #flagName, ( bool* )&flagName##flag ) ) {\
editedMaterial.flagName = flagName##flag;\
\
isMaterialDirty = true;\
}
    if ( !isOpened ) {
        return;
    }

    if ( ImGui::Begin( "Material Editor", &isOpened ) ) {
        bool isMaterialDirty = false;

        if ( ImGui::Button( "Compile" ) ) {
            activeMaterial = materialGenerator->createMaterial( editedMaterial );

            if ( activeMaterial != nullptr ) {
                activeMaterial->invalidateCache();
                activeMaterial->updateResourceStreaming( graphicsAssetCache );
            }
        }

        ImGui::SameLine();

        if ( ImGui::Checkbox( "Interactive Mode", &useInteractiveMode ) ) {
            // If the interactive mode has been disabled, make sure the baked version matches the interactive one.
            if ( !useInteractiveMode ) {
                activeMaterial = materialGenerator->createMaterial( editedMaterial );

                if ( activeMaterial != nullptr ) {
                    activeMaterial->invalidateCache();
                    activeMaterial->updateResourceStreaming( graphicsAssetCache );
                }
            }
        }

        if ( ImGui::Button( "Load" ) ) {
            // TODO Legacy shit. Should be removed once the asset browser is implemented.
            dkString_t fullPathToAsset;
            if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                       dk::core::SelectionType::OpenFile,
                                                       DUSK_STRING( "Dusk Editable Material (*.dem)\0*.dem\0" ) ) ) {
                dk::core::SanitizeFilepathSlashes( fullPathToAsset );

                dkString_t workingDirectory;
                dk::core::RetrieveWorkingDirectory( workingDirectory );
                dk::core::RemoveWordFromString( workingDirectory, DUSK_STRING( "build/bin/" ) );

                if ( !dk::core::ContainsString( fullPathToAsset, workingDirectory ) ) {
                    // TODO Copy resource to the working directory.
                } else {
                    dk::core::RemoveWordFromString( fullPathToAsset, workingDirectory );
                    dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "data/" ) );
                    dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "Assets/" ) );

                    dkString_t fileSystemPath = dkString_t( DUSK_STRING( "EditorAssets/" ) ) + fullPathToAsset + DUSK_STRING( ".dem" );

                    FileSystemObject* materialDescriptor = virtualFileSystem->openFile( fileSystemPath, eFileOpenMode::FILE_OPEN_MODE_READ );
                    if ( materialDescriptor->isGood() ) {
                        editedMaterial.loadFromFile( materialDescriptor, memoryAllocator, graphicsAssetCache );
                        materialDescriptor->close();
                    }
                }
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( "Save" ) ) {
            // TODO Legacy shit. Should be removed once the asset browser is implemented.
            dkString_t fullPathToAsset;
            if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                       dk::core::SelectionType::SaveFile,
                                                       DUSK_STRING( "Dusk Editable Material (*.dem)\0*.dem\0" ) ) ) {
                dk::core::SanitizeFilepathSlashes( fullPathToAsset );

                dkString_t workingDirectory;
                dk::core::RetrieveWorkingDirectory( workingDirectory );
                dk::core::RemoveWordFromString( workingDirectory, DUSK_STRING( "build/bin/" ) );

                if ( !dk::core::ContainsString( fullPathToAsset, workingDirectory ) ) {
                    // TODO Copy resource to the working directory.
                } else {
                    dk::core::RemoveWordFromString( fullPathToAsset, workingDirectory );
                    dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "data/" ) );
                    dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "Assets/" ) );

                    dkString_t fileSystemPath = dkString_t( DUSK_STRING( "EditorAssets/" ) ) + fullPathToAsset + DUSK_STRING( ".dem" );

                    FileSystemObject* materialDescriptor = virtualFileSystem->openFile( fileSystemPath, eFileOpenMode::FILE_OPEN_MODE_WRITE );
                    if ( materialDescriptor->isGood() ) {
                        editedMaterial.serialize( materialDescriptor );
                        materialDescriptor->close();
                    }
                }
            }
        }

        // Material Description.
        ImGui::InputText( "Name", editedMaterial.Name, DUSK_MAX_PATH );

        constexpr const char* ShadingModelString[ShadingModel::ShadingModel_Count] = {
            "Default",
            "Clear Coat"
        };

        isMaterialDirty = ImGui::Combo( "Shading Model", reinterpret_cast< i32* >( &editedMaterial.SModel ), ShadingModelString, ShadingModel::ShadingModel_Count );
        
        // Material Flags.
        DUSK_DISPLAY_MATERIAL_FLAG( IsDoubleFace );
        DUSK_DISPLAY_MATERIAL_FLAG( IsAlphaTested );
        DUSK_DISPLAY_MATERIAL_FLAG( IsAlphaBlended );
        DUSK_DISPLAY_MATERIAL_FLAG( UseAlphaToCoverage );
        DUSK_DISPLAY_MATERIAL_FLAG( WriteVelocity );
        DUSK_DISPLAY_MATERIAL_FLAG( ReceiveShadow );
        DUSK_DISPLAY_MATERIAL_FLAG( ScaleUVByModelScale );
		DUSK_DISPLAY_MATERIAL_FLAG( UseRefraction );
		DUSK_DISPLAY_MATERIAL_FLAG( IsWireframe );
		DUSK_DISPLAY_MATERIAL_FLAG( IsShadeless );
        
        // Material Layers.
        for ( i32 layerIdx = 0; layerIdx < editedMaterial.LayerCount; layerIdx++ ) {
            EditableMaterialLayer& layer = editedMaterial.Layers[layerIdx];

            constexpr const char* LayerNames[Material::MAX_LAYER_COUNT] = {
                "Root Layer",
                "Layer 1",
                "Layer 2",
                "Layer 3"
            };

            // We need to distinguish the current attributes if we have multiple layer expanded at once.
            ImGui::PushID( LayerNames[layerIdx] );

            const bool isTopmostLayer = ( layerIdx == ( editedMaterial.LayerCount - 1 ) );
            const bool canAddLayer = ( editedMaterial.LayerCount < Material::MAX_LAYER_COUNT && isTopmostLayer );
            const bool canRemoveLayer = ( editedMaterial.LayerCount > 1 && isTopmostLayer );
            const bool canBlendLayer = ( editedMaterial.LayerCount > 1 && layerIdx != 0 );

            if ( !canAddLayer ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( "+" ) && canAddLayer ) {
                editedMaterial.LayerCount++;
                editedMaterial.Layers[editedMaterial.LayerCount - 1] = EditableMaterialLayer();
            }

            if ( !canAddLayer ) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

            ImGui::SameLine();

            if ( !canRemoveLayer ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( "-" ) && canRemoveLayer ) {
                editedMaterial.LayerCount--;
                break;
            }

            if ( !canRemoveLayer ) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

            ImGui::SameLine();

            if ( ImGui::TreeNode( LayerNames[layerIdx] ) ) {
                displayMaterialAttribute<true>( layerIdx, "BaseColor", layer.BaseColor );
                displayMaterialAttribute<true>( layerIdx, "Reflectance", layer.Reflectance );
                displayMaterialAttribute<true>( layerIdx, "Roughness", layer.Roughness );
                displayMaterialAttribute<true>( layerIdx, "Metalness", layer.Metalness );
                displayMaterialAttribute<true>( layerIdx, "AmbientOcclusion", layer.AmbientOcclusion );
                displayMaterialAttribute<false>( layerIdx, "Normal", layer.Normal );
                displayMaterialAttribute<true>( layerIdx, "ClearCoat", layer.ClearCoat );
                displayMaterialAttribute<true>( layerIdx, "ClearCoatGlossiness", layer.ClearCoatGlossiness );

                if ( editedMaterial.IsAlphaTested ) {
                    displayMaterialAttribute<true>( layerIdx, "AlphaMask", layer.AlphaMask );
                    ImGui::DragFloat( "AlphaCutoff", &layer.AlphaCutoff );
                }

                if ( canBlendLayer ) {
                    constexpr const char* BlendModeString[ShadingModel::ShadingModel_Count] = {
                        "Additive",
                        "Multiplicative"
                    };

                    displayMaterialAttribute<true>( layerIdx, "BlendMask", layer.BlendMask );

                    ImGui::Combo( "Blend Mode", reinterpret_cast< i32* >( &layer.BlendMode ), BlendModeString, LayerBlendMode::BlendModeCount );

                    ImGui::SliderFloat( "Diffuse Contribution", &layer.DiffuseContribution, 0.0f, 1.0f );
                    ImGui::SliderFloat( "Specular Contribution", &layer.SpecularContribution, 0.0f, 1.0f );
                    ImGui::SliderFloat( "Normal Contribution", &layer.NormalContribution, 0.0f, 1.0f );
                }

                ImGui::DragFloat2( "LayerScale", &layer.Scale[0], 0.01f, 0.01f, 1024.0f );
                ImGui::DragFloat2( "LayerOffset", &layer.Offset[0], 0.01f, 0.01f, 1024.0f );

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }
    
    ImGui::End();
    
#undef DUSK_DISPLAY_MATERIAL_FLAG
}
#endif

void MaterialEditor::openEditorWindow()
{
    isOpened = true;
}

void MaterialEditor::setActiveMaterial( Material* material )
{
    activeMaterial = material;
}

Material* MaterialEditor::getActiveMaterial() const
{
    return activeMaterial;
}

MaterialEdData* MaterialEditor::getRuntimeEditionData()
{
    if ( activeMaterial == nullptr ) {
        return nullptr;
    }

    bufferData.LayerCount = editedMaterial.LayerCount;
    bufferData.ShadingModel = static_cast< i32 >( editedMaterial.SModel );
    bufferData.WriteVelocity = editedMaterial.WriteVelocity;
    bufferData.ReceiveShadow = editedMaterial.ReceiveShadow;
    bufferData.ScaleUVByModelScale = editedMaterial.ScaleUVByModelScale;

    for ( i32 layerIdx = 0; layerIdx < editedMaterial.LayerCount; layerIdx++ ) {
        const EditableMaterialLayer& layer = editedMaterial.Layers[layerIdx];
        MaterialEdLayer& runtimeLayer = bufferData.Layers[layerIdx];

        FillRuntimeMaterialLayer( layer, runtimeLayer );
    }

    return &bufferData;
}

bool MaterialEditor::isUsingInteractiveMode() const
{
    return useInteractiveMode;
}

template<bool SaturateInput>
void MaterialEditor::displayMaterialAttribute( const i32 layerIndex, const char* displayName, MaterialAttribute& attribute )
{
    constexpr const char* SlotInputTypeLabels[MaterialAttribute::InputType::Count] = {
        "None",
        "1D Constant Value",
        "3D Constant Value",
        "3D Mutable Value",
        "Texture",
        "HLSL Code"
    };

    // Input label
    if ( ImGui::TreeNode( displayName ) ) {
        const MaterialAttribute::InputType prevType = attribute.Type;

        // Input Type
        if ( ImGui::Combo( "Type", ( i32* )&attribute.Type, SlotInputTypeLabels, MaterialAttribute::InputType::Count ) ) {
            if ( prevType == MaterialAttribute::Code_Piece ) {
                dk::core::freeArray( attribute.AsCodePiece.Allocator, attribute.AsCodePiece.Content );
                attribute.AsCodePiece.Allocator = nullptr;
            }

            // Check if the new value is different (we want to reset values because of memory aliasing).
            if ( prevType != attribute.Type ) {
                if ( attribute.Type == MaterialAttribute::Code_Piece ) {
                    attribute.AsCodePiece.Content = dk::core::allocateArray<char>( memoryAllocator, MAX_CODE_PIECE_LENGTH );
                    attribute.AsCodePiece.Allocator = memoryAllocator;
                }
            }
        }

        switch ( attribute.Type ) {
        case MaterialAttribute::Constant_1D:
            if ( SaturateInput ) {
                ImGui::SliderFloat( "Value", &attribute.AsFloat, 0.0001f, 1.0f );
            } else {
                ImGui::DragFloat( "Value", &attribute.AsFloat, 1.0f, 0.0f );
            }
            break;
        case MaterialAttribute::Mutable_3D:
        case MaterialAttribute::Constant_3D:
            ImGui::ColorEdit3( "Value", &attribute.AsFloat3[0] );
            break;
        case MaterialAttribute::Texture_2D:
        {
            Image* textureInstance = attribute.AsTexture.TextureInstance;

            dkStringHash_t parameterHashcode = dk::core::CRC32( MaterialGenerator::buildTextureLayerName( displayName, MaterialLayerNames[layerIndex] ) );

            bool hasTextureBound = ( textureInstance != nullptr );

            ImGui::PushID( parameterHashcode );
            bool buttonHasBeenPressed = ( hasTextureBound )
                ? ImGui::ImageButton( static_cast< ImTextureID >( textureInstance ), ImVec2( 64, 64 ) )
                : ImGui::Button( "+", ImVec2( 64, 64 ) );
            
            if ( buttonHasBeenPressed ) {
                // TODO Legacy shit. Should be removed once the asset browser is implemented.
                dkString_t fullPathToAsset;
                if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                           dk::core::SelectionType::OpenFile, 
                                                           DefaultTextureFilter ) ) {
                    dk::core::SanitizeFilepathSlashes( fullPathToAsset );
                    
                    dkString_t workingDirectory;
                    dk::core::RetrieveWorkingDirectory( workingDirectory );
                    dk::core::RemoveWordFromString( workingDirectory, DUSK_STRING( "build/bin/" ) );

                    if ( !dk::core::ContainsString( fullPathToAsset, workingDirectory ) ) {
                        // TODO Copy resource to the working directory.
                    } else {
                        dk::core::RemoveWordFromString( fullPathToAsset, workingDirectory );
                        dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "data/" ) );
                        dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "Assets/" ) );

                        attribute.AsTexture.PathToTextureAsset = dkString_t( DUSK_STRING( "GameData/" ) ) + fullPathToAsset;
                    }

                    attribute.AsTexture.TextureInstance = graphicsAssetCache->getImage( attribute.AsTexture.PathToTextureAsset.c_str(), false );

                    if ( useInteractiveMode && activeMaterial != nullptr ) {
                        activeMaterial->setParameterAsTexture2D( parameterHashcode, DUSK_NARROW_STRING( attribute.AsTexture.PathToTextureAsset ) );

                        activeMaterial->invalidateCache();
                        activeMaterial->updateResourceStreaming( graphicsAssetCache );
                    }
                }
            }
            ImGui::SameLine();

            if ( hasTextureBound ) {
                ImageDesc* imgDesc = graphicsAssetCache->getImageDescription( attribute.AsTexture.PathToTextureAsset.c_str() );

                std::string infos;
                infos.append( DUSK_NARROW_STRING( attribute.AsTexture.PathToTextureAsset ).c_str() );

                infos.append( "\nDimensions: " );
                infos.append( std::to_string( imgDesc->width ) );
                infos.append( "x" );
                infos.append( std::to_string( imgDesc->height ) );

                infos.append( "\nMip Count: " );
                infos.append( std::to_string( imgDesc->mipCount ) );

                infos.append( "\nFormat: " );
                infos.append( VIEW_FORMAT_STRING[imgDesc->format] );
                
                ImGui::Text( infos.c_str() );
            } else {
                ImGui::Text( "No Texture Bound" );
            }

            ImGui::Checkbox( "sRGB Colorspace", &attribute.AsTexture.IsSRGBSpace ); 
            ImGui::PopID();
            break;
        }
        case MaterialAttribute::Code_Piece:
            ImGui::InputTextMultiline( "Value", attribute.AsCodePiece.Content, MAX_CODE_PIECE_LENGTH );
            break;
        default:
            break;
        }

        ImGui::TreePop();
    }
}
