/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EntityEditor.h"

#if DUSK_USE_IMGUI
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#endif

#include <Core/FileSystemIOHelpers.h>
#include <Core/StringHelpers.h>
#include <Core/Environment.h>

#include <Framework/Cameras/Camera.h>

#include "Framework/MaterialEditor.h"
#include "Framework/StaticGeometry.h"
#include "Framework/Transform.h"
#include "Framework/EntityNameRegister.h"

#include <Graphics/GraphicsAssetCache.h>

EntityEditor::EntityEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs )
    : isOpened( false )
    , activeEntity( nullptr )
    , activeWorld( nullptr )
    , memoryAllocator( allocator )
    , graphicsAssetCache( gfxCache )
    , virtualFileSystem( vfs )
{

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
		// Name Edition.
		char* entityName = activeWorld->getEntityNameRegister()->getNameBuffer( *activeEntity );
		ImGui::InputText( "Name", entityName, Entity::MAX_NAME_LENGTH * sizeof( char ) );

        // Display component edition. We have a finite number of component type so it shouldn't
        // be too bad to manage this by hand...
        displayTransformSection( viewportBounds, viewportCamera );
    }  
    ImGui::End();
}

void EntityEditor::displayTransformSection( const dkVec4f& viewportBounds, CameraData& viewportCamera )
{
    bool hasTransformAttachment = activeWorld->getTransformDatabase()->hasComponent( *activeEntity );
    if ( !hasTransformAttachment ) {
        return;
    }

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

        ImGui::Text( "Guizmo Settings" );
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
    }
    ImGui::TreePop();
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
