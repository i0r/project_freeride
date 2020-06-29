/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EntityEditor.h"

#if DUSK_USE_IMGUI
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>
#endif

#if DUSK_USE_FBXSDK
#include "Parsing/FbxParser.h"
#endif

#include <Core/FileSystemIOHelpers.h>
#include <Core/StringHelpers.h>
#include <Core/Environment.h>

#include <Framework/Cameras/Camera.h>

#include "Framework/World.h"
#include "Framework/MaterialEditor.h"
#include "Framework/StaticGeometry.h"
#include "Framework/Transform.h"
#include "Framework/EntityNameRegister.h"

#include <Graphics/GraphicsAssetCache.h>
#include <Graphics/Model.h>
#include <Graphics/RenderWorld.h>

EntityEditor::EntityEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs, RenderWorld* rWorld, RenderDevice* activeRenderDevice )
    : isOpened( false )
    , activeEntity( nullptr )
    , activeWorld( nullptr )
    , memoryAllocator( allocator )
    , graphicsAssetCache( gfxCache )
    , virtualFileSystem( vfs )
    , renderWorld( rWorld )
    , renderDevice( activeRenderDevice )
{
#if DUSK_USE_FBXSDK
    fbxParser = dk::core::allocate<FbxParser>( allocator );
    fbxParser->create( allocator );
#endif
}

EntityEditor::~EntityEditor()
{

}

#if DUSK_USE_IMGUI
void EntityEditor::displayEditorWindow( CameraData& viewportCamera, const dkVec4f& viewportBounds )
{
    if ( !isOpened || activeWorld == nullptr || activeEntity == nullptr ) {
        return;
    }

	if ( ImGui::Begin( "Inspector", &isOpened ) ) {
		if ( activeEntity->getIdentifier() != Entity::INVALID_ID ) {
			// Name Edition.
			char* entityName = activeWorld->getEntityNameRegister()->getNameBuffer( *activeEntity );
			ImGui::InputText( "Name", entityName, Entity::MAX_NAME_LENGTH * sizeof( char ) );

			// Display component edition. We have a finite number of component type so it shouldn't
			// be too bad to manage this by hand...
			displayTransformSection( viewportBounds, viewportCamera );
			displayStaticGeometrySection();
        }
    }

    ImGui::End();
}

void EntityEditor::displayTransformSection( const dkVec4f& viewportBounds, CameraData& viewportCamera )
{
    bool hasTransformAttachment = activeWorld->getTransformDatabase()->hasComponent( *activeEntity );
    if ( !hasTransformAttachment ) {
        return;
    }

	ImGui::SetNextItemOpen( true );
    if ( ImGui::TreeNode( "Transform" ) ) {
        // Retrieve this instance transform information.
        TransformDatabase* transformDb = activeWorld->getTransformDatabase();
        TransformDatabase::EdInstanceData& editorInstance = transformDb->getEditorInstanceData( transformDb->lookup( *activeEntity ) );

        static ImGuizmo::OPERATION mCurrentGizmoOperation( ImGuizmo::TRANSLATE );
        static int activeManipulationMode = 0;
        static bool useSnap = false;
        static float snap[3] = { 1.f, 1.f, 1.f };

        dkMat4x4f* modelMatrix = editorInstance.Local;

        ImGuizmo::SetRect( viewportBounds.x, viewportBounds.y, viewportBounds.z, viewportBounds.w );
        ImGuizmo::SetDrawlist();
        ImGuizmo::Manipulate(
            viewportCamera.viewMatrix.toArray(),
            viewportCamera.finiteProjectionMatrix.toArray(),
            static_cast< ImGuizmo::OPERATION >( activeManipulationMode ),
            ImGuizmo::MODE::LOCAL,
            modelMatrix->toArray(),
            nullptr,
            ( useSnap ) ? &snap[activeManipulationMode] : nullptr
        );

        // Convert Quaternion to Euler Angles (for user friendlyness).
        dkQuatf RotationQuat = dk::maths::ExtractRotation( *modelMatrix, *editorInstance.Scale );
        dkVec3f Rotation = RotationQuat.toEulerAngles();

        ImGui::DragFloat3( "Translation", ( float* )editorInstance.Position );
        ImGui::DragFloat3( "Rotation", ( float* )&Rotation, 3 );
        ImGui::DragFloat3( "Scale", ( float* )editorInstance.Scale );

        RotationQuat = dkQuatf( Rotation );
        *editorInstance.Rotation = RotationQuat;

        if ( ImGuizmo::IsUsing() ) {
            *editorInstance.Position = dk::maths::ExtractTranslation( *modelMatrix );
            *editorInstance.Scale = dk::maths::ExtractScale( *modelMatrix );
            *editorInstance.Rotation = dk::maths::ExtractRotation( *modelMatrix, *editorInstance.Scale );
        }

        // TODO Move those to a settings menu/panel and add shortcut for transformation mode (translate/rotate/etc.).
		/*  ImGui::Text( "Guizmo Settings" );
		  ImGui::Checkbox( "", &useSnap );
		  ImGui::SameLine();
		  switch ( mCurrentGizmoOperation ) {
		  case ImGuizmo::TRANSLATE:
			  ImGui::InputFloat3( "Snap", snap );
			  break;
		  case ImGuizmo::ROTATE:
			  ImGui::InputFloat( "Angle Snap", snap );
			  break;
		  case ImGuizmo::SCALE:
			  ImGui::InputFloat( "Scale Snap", snap );
			  break;
		  }

		  ImGui::RadioButton( "Translate", &activeManipulationMode, 0 );
		  ImGui::SameLine();
		  ImGui::RadioButton( "Rotate", &activeManipulationMode, 1 );
		  ImGui::SameLine();
		  ImGui::RadioButton( "Scale", &activeManipulationMode, 2 );
		  */
        ImGui::TreePop();
    }
}

void EntityEditor::displayStaticGeometrySection()
{
	bool hasStaticGeometryAttachment = activeWorld->getStaticGeometryDatabase()->hasComponent( *activeEntity );
	if ( !hasStaticGeometryAttachment ) {
		return;
	}

    ImGui::SetNextItemOpen( true );
	if ( ImGui::TreeNode( "Static Geometry" ) ) {
		// Retrieve this instance static geometry information.
		StaticGeometryDatabase* staticGeoDb = activeWorld->getStaticGeometryDatabase();

		const Model* assignedModel = staticGeoDb->getModel( staticGeoDb->lookup( *activeEntity ) );

        // TODO Capture a thumbnail to preview the model.
        ImGui::Button( "PLACEHOLDER", ImVec2( 64, 64 ) );

        ImGui::SameLine();

		std::string infos;
		if ( assignedModel != nullptr ) {
            infos.append( WideStringToString( assignedModel->getName() ) );
            infos.append( "\nLOD Count: " );
            infos.append( std::to_string( assignedModel->getLevelOfDetailCount() ) );
		}

        ImGui::Text( infos.c_str() );

		if ( ImGui::Button( "..." ) ) {
#if DUSK_USE_FBXSDK
			static constexpr dkChar_t* DefaultGeometryFilter = DUSK_STRING( "Autodesk FBX (*.fbx)\0*.fbx\0" );

			// TODO Legacy shit. Should be removed once the asset browser is implemented.
			dkString_t fullPathToAsset;
            if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                       dk::core::SelectionType::OpenFile,
                                                       DefaultGeometryFilter ) ) {
                dk::core::SanitizeFilepathSlashes( fullPathToAsset );
                fbxParser->load( WideStringToString( fullPathToAsset ).c_str() );

                ParsedModel* parsedModel = fbxParser->getParsedModel();
                if ( parsedModel != nullptr ) {
                    Model* builtModel = renderWorld->addAndCommitParsedDynamicModel( renderDevice, *parsedModel, graphicsAssetCache );
                    
                    if ( builtModel != nullptr ) {
                        staticGeoDb->setModel( staticGeoDb->lookup( *activeEntity ), builtModel );
                    }
                }
            }
#endif
        }

		if ( assignedModel != nullptr ) {
            ImGui::SameLine();
			ImGui::Text( assignedModel->getResourcedPath().c_str() );
		}

		ImGui::TreePop();
	}
}
#endif

void EntityEditor::openEditorWindow()
{
    isOpened = true;
}

void EntityEditor::setActiveEntity( Entity* entity )
{
    activeEntity = entity;
}

void EntityEditor::setActiveWorld( World* world )
{
    activeWorld = world;
}

bool EntityEditor::isManipulatingTransform() const
{
#ifdef DUSK_USE_IMGUI
    return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
#else
    return false;
#endif
}
