/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "MaterialEditor.h"

#if DUSK_USE_IMGUI
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#endif

#include <Core/FileSystemIOHelpers.h>

static constexpr dkChar_t* DefaultTextureFilter = DUSK_STRING( "All (*.dds, *.jpg, *.png, *.png16, *.tga, *.lpng)\0*.dds;*.jpg;*.png;*.png16;*.tga;*.lpng\0DirectDraw Surface (*.dds)\0*.dds\0JPG (*.jpg)\0*.jpg\0PNG (*.png)\0*.png\0PNG 16 Bits (*.png16)\0*.png16\0Low Precision PNG (*.lpng)\0*.lpng\0TGA (*.tga)\0*.tga\0" );

MaterialEditor::MaterialEditor( BaseAllocator* allocator )
    : isOpened( false )
    , editedMaterial()
    , activeMaterial( nullptr )
    , memoryAllocator( allocator )
{

}

MaterialEditor::~MaterialEditor()
{

}

#if DUSK_USE_IMGUI
void MaterialEditor::displayEditorWindow()
{
#define DUSK_DISPLAY_MATERIAL_FLAG( flagName )\
bool flagName##flag = editedMaterial.##flagName;\
if ( ImGui::Checkbox( #flagName, ( bool* )&flagName##flag ) ) {\
editedMaterial.##flagName = flagName##flag;\
\
isMaterialDirty = true;\
}\

    if ( isOpened && ImGui::Begin( "Material Editor", &isOpened ) ) {
        bool isMaterialDirty = false;

        // Material Description.
        ImGui::InputText( "Name", editedMaterial.Name, DUSK_MAX_PATH );

        constexpr const char* ShadingModelString[ShadingModel::Count] = {
            "Default",
            "Clear Coat"
        };

        isMaterialDirty = ImGui::Combo( "Shading Model", reinterpret_cast< i32* >( &editedMaterial.SModel ), ShadingModelString, ShadingModel::Count );
        
        // Material Flags.
        DUSK_DISPLAY_MATERIAL_FLAG( IsDoubleFace );
        DUSK_DISPLAY_MATERIAL_FLAG( IsAlphaTested );
        DUSK_DISPLAY_MATERIAL_FLAG( IsAlphaBlended );
        DUSK_DISPLAY_MATERIAL_FLAG( UseAlphaToCoverage );
        DUSK_DISPLAY_MATERIAL_FLAG( WriteVelocity );
        DUSK_DISPLAY_MATERIAL_FLAG( ReceiveShadow );
        DUSK_DISPLAY_MATERIAL_FLAG( ScaleUVByModelScale );
        DUSK_DISPLAY_MATERIAL_FLAG( UseRefraction );
        
        // Material Layers.
        for ( i32 layerIdx = 0; layerIdx < editedMaterial.LayerCount; layerIdx++ ) {
            EditableMaterialLayer& layer = editedMaterial.Layers[layerIdx];

            constexpr const char* LayerNames[Material::MAX_LAYER_COUNT] = {
                "Root Layer",
                "Layer 1",
                "Layer 2"
            };

            // We need to distinguish the current attributes if we have multiple layer expanded at once.
            ImGui::PushID( LayerNames[layerIdx] );

            const bool isTopmostLayer = ( layerIdx == ( editedMaterial.LayerCount - 1 ) );
            const bool canAddLayer = ( editedMaterial.LayerCount < Material::MAX_LAYER_COUNT && isTopmostLayer );
            const bool canRemoveLayer = ( editedMaterial.LayerCount > 1 && isTopmostLayer );

            if ( !canAddLayer ) {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
            }

            if ( ImGui::Button( "+" ) && canAddLayer ) {
                editedMaterial.LayerCount++;
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
                displayMaterialAttribute<true>( "BaseColor", layer.BaseColor );
                displayMaterialAttribute<true>( "Reflectance", layer.Reflectance );
                displayMaterialAttribute<true>( "Roughness", layer.Roughness );
                displayMaterialAttribute<true>( "Metalness", layer.Metalness );
                displayMaterialAttribute<true>( "AmbientOcclusion", layer.AmbientOcclusion );
                displayMaterialAttribute<false>( "Normal", layer.Normal );

                ImGui::DragFloat2( "LayerScale", &layer.Scale[0], 0.01f, 0.01f, 1024.0f );
                ImGui::DragFloat2( "LayerOffset", &layer.Offset[0], 0.01f, 0.01f, 1024.0f );

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::End();
    }
    
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

template<bool SaturateInput>
void MaterialEditor::displayMaterialAttribute( const char* displayName, MaterialAttribute& attribute )
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

            bool buttonHasBeenPressed = ( textureInstance != nullptr )
                ? ImGui::ImageButton( static_cast< ImTextureID >( textureInstance ), ImVec2( 64, 64 ) )
                : ImGui::Button( "+", ImVec2( 64, 64 ) );

            if ( buttonHasBeenPressed ) {
                dkString_t fullPathToAsset;
                if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                           dk::core::SelectionType::OpenFile, 
                                                           DefaultTextureFilter ) ) {
                    // If is not in the GameData FileSystem
                    //  Then Copy to the gamedata filesystem
                    // Use the Gfx Asset Cache to load teh texture
                }
            }
            ImGui::SameLine();
            ImGui::Text( "No Texture Bound" );
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
