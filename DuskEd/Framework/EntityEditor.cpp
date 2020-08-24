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

#include "ThirdParty/Google/IconsMaterialDesign.h"
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

#include <Maths/Vector.h>

#include "ImGuiUtilities.h"

#include "Framework/Transaction/TransactionHandler.h"
#include "Framework/Transaction/TransformCommands.h"

EntityEditor::EntityEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs, RenderWorld* rWorld, RenderDevice* activeRenderDevice )
    : isOpened( false )
    , activeEntity( nullptr )
    , activeWorld( nullptr )
    , memoryAllocator( allocator )
    , graphicsAssetCache( gfxCache )
    , virtualFileSystem( vfs )
    , renderWorld( rWorld )
    , renderDevice( activeRenderDevice )
    , wasManipulatingLastFrame( false )
	, startTransactionTranslation( dkVec3f::Zero )
	, startTransactionScale( dkVec3f( 1.0f ) )
	, startTransactionRotation( dkQuatf::Identity )
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
void EntityEditor::displayEntityInHiearchy( const dkStringHash_t entityHashcode, const Entity& entity )
{
	ImGuiTreeNodeFlags flags = 0;

    const bool isLeaf = true; // node->children.empty();
	const bool isSelected = activeEntity != nullptr && ( *activeEntity == entity );

	if ( isSelected )
		flags |= ImGuiTreeNodeFlags_Selected;

	if ( isLeaf )
		flags |= ImGuiTreeNodeFlags_Leaf;

    const char* entityName = activeWorld->getEntityNameRegister()->getName( entity );
	bool isOpened = ImGui::TreeNodeEx( entityName, flags );

	if ( ImGui::IsItemClicked() ) {
        activeEntity->setIdentifier( entity.getIdentifier() );
	}

	if ( isOpened ) {
		/*if ( !isLeaf ) {
			ImGui::Indent();
			for ( auto child : node->children ) {
				displayEntityInHiearchy( child );
			}
		}*/

		ImGui::TreePop();
	}
}

void EntityEditor::displayEditorWindow( CameraData& viewportCamera, const dkVec4f& viewportBounds )
{
    if ( !isOpened || activeWorld == nullptr || activeEntity == nullptr ) {
        return;
    }

	if ( ImGui::Begin( ICON_MD_ZOOM_IN " Inspector", &isOpened ) ) {
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

    if ( ImGui::Begin( ICON_MD_LIST " Streamed Hiearchy", &isOpened ) ) {
        const auto& nameHashmap = activeWorld->getEntityNameRegister()->getRegisterHashmap();

        for ( auto& name : nameHashmap ) {
            displayEntityInHiearchy( name.first, name.second );
        }
	}
	ImGui::End();
}

void EntityEditor::displayTransformSection( const dkVec4f& viewportBounds, CameraData& viewportCamera )
{
    extern TransactionHandler* g_TransactionHandler;

    bool hasTransformAttachment = activeWorld->getTransformDatabase()->hasComponent( *activeEntity );
    if ( !hasTransformAttachment ) {
        return;
    }

	ImGui::SetNextItemOpen( true );
    if ( ImGui::TreeNode( ICON_MD_3D_ROTATION " Transform" ) ) {
        // Retrieve this instance transform information.
        TransformDatabase* transformDb = activeWorld->getTransformDatabase();
        TransformDatabase::EdInstanceData& editorInstance = transformDb->getEditorInstanceData( transformDb->lookup( *activeEntity ) );

        // TODO Move manipulation parameters to a toolbar
        static ImGuizmo::OPERATION mCurrentGizmoOperation( ImGuizmo::TRANSLATE );
        static int activeManipulationMode = 0;
        static bool useSnap = false;
        static bool lockScaleChannels = false;
        static float snap[3] = { 1.f, 1.f, 1.f };

        dkMat4x4f* modelMatrix = editorInstance.Local;

		dkVec3f newTranslation = *editorInstance.Position;
		dkVec3f newScale = *editorInstance.Scale;

		dkVec3f oldTranslation = *editorInstance.Position;
		dkVec3f oldScale = *editorInstance.Scale;
        dkQuatf oldRotation = *editorInstance.Rotation;

        // Draw Manipulation Guizmo.
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

        // Convert Quaternion to Euler Angles (for user friendliness).
        dkQuatf RotationQuat = dk::maths::ExtractRotation( *modelMatrix, *editorInstance.Scale );
        dkVec3f Rotation = RotationQuat.toEulerAngles();

        // Translation
        if ( ImGui::DragFloat3( "Translation", ( f32* )&newTranslation ) ) {
            g_TransactionHandler->commit<TranslateCommand>( &editorInstance, newTranslation, oldTranslation );
        }

        // Rotation
		if ( ImGui::DragFloat3( "Rotation", ( f32* )&Rotation, 3 ) ) {
			RotationQuat = dkQuatf( Rotation );

			g_TransactionHandler->commit<RotateCommand>( &editorInstance, RotationQuat, oldRotation );
        }

        // Scale
        if ( dk::imgui::DragFloat3WithChannelLock( "Scale", &newScale, lockScaleChannels ) ) {
            g_TransactionHandler->commit<ScaleCommand>( &editorInstance, newScale, oldScale );
        }
        
        bool isGuizmoManipulatedThisFrame = ImGuizmo::IsUsing();

        // Update transform components if the guizmo has been manipulated.
		if ( wasManipulatingLastFrame && !isGuizmoManipulatedThisFrame ) {
			// Translation
            newTranslation = dk::maths::ExtractTranslation( *modelMatrix );
            if ( newTranslation != startTransactionTranslation ) {
				g_TransactionHandler->commit<TranslateCommand>( &editorInstance, newTranslation, startTransactionTranslation );
            }

            // Scale
			newScale = dk::maths::ExtractScale( *modelMatrix );
			if ( newScale != startTransactionScale ) {
				g_TransactionHandler->commit<ScaleCommand>( &editorInstance, newScale, startTransactionScale );
			}

            // Rotation
            RotationQuat = dk::maths::ExtractRotation( *modelMatrix, *editorInstance.Scale );
            if ( RotationQuat != startTransactionRotation ) {
				g_TransactionHandler->commit<RotateCommand>( &editorInstance, RotationQuat, startTransactionRotation );
            }
        } else if ( isGuizmoManipulatedThisFrame ) {
            // Backup transform components at the begining of the transaction.
			if ( !wasManipulatingLastFrame ) {
				startTransactionTranslation = *editorInstance.Position;
				startTransactionScale = *editorInstance.Scale;
                startTransactionRotation = *editorInstance.Rotation;
            }
        
            // Update transform.
			*editorInstance.Position = dk::maths::ExtractTranslation( *modelMatrix );
			*editorInstance.Scale = dk::maths::ExtractScale( *modelMatrix );
			*editorInstance.Rotation = dk::maths::ExtractRotation( *modelMatrix, *editorInstance.Scale );
        }

        wasManipulatingLastFrame = isGuizmoManipulatedThisFrame;

        // Sanitize scale to avoid NaN and Inf.
        *editorInstance.Scale = dkVec3f::max( dkVec3f( 0.0001f ), *editorInstance.Scale );

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
	if ( ImGui::TreeNode( ICON_MD_ADD_BOX " Static Geometry" ) ) {
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

            const i32 lodCount = assignedModel->getLevelOfDetailCount();

            if ( ImGui::TreeNode( ICON_MD_ADD_BOX "Level Of Details" ) ) {
                constexpr const char* LOD_LABELS[Model::MAX_LOD_COUNT] = {
                    "LOD 0",
                    "LOD 1",
                    "LOD 2",
                    "LOD 3"
                };

                for ( i32 lodIdx = 0; lodIdx < lodCount; lodIdx++ ) {
                    const Model::LevelOfDetail& lod = assignedModel->getLevelOfDetailByIndex( lodIdx );
                    const i32 lodMeshCount = lod.MeshCount;

                    if ( ImGui::TreeNode( LOD_LABELS[lodIdx] ) ) {
                        for ( i32 meshIdx = 0; meshIdx < lodMeshCount; meshIdx++ ) {
                            Mesh& mesh = lod.MeshArray[meshIdx];

                            ImGui::Text( mesh.Name.c_str() );
                            ImGui::PushID( mesh.Name.c_str() );

                            ImGui::Button( "PLACEHOLDER", ImVec2( 64, 64 ) );
                            if ( ImGui::Button( "..." ) ) {
                                static constexpr dkChar_t* DefaultMaterialFilter = DUSK_STRING( "Dusk Engine Baked Material (*.mat)\0*.mat\0" );

                                // TODO Legacy shit. Should be removed once the asset browser is implemented.
                                dkString_t fullPathToAsset;
                                if ( dk::core::DisplayFileSelectionPrompt( fullPathToAsset,
                                                                            dk::core::SelectionType::OpenFile,
                                                                            DefaultMaterialFilter ) ) {
                                    dk::core::SanitizeFilepathSlashes( fullPathToAsset ); 

                                    dkString_t workingDirectory;
                                    dk::core::RetrieveWorkingDirectory( workingDirectory );
                                    dk::core::RemoveWordFromString( workingDirectory, DUSK_STRING( "build/bin/" ) );

                                    dkString_t vfsMaterialPath;
                                    if ( !dk::core::ContainsString( fullPathToAsset, workingDirectory ) ) {
                                        // TODO Copy resource to the working directory.
                                    } else {
                                        dk::core::RemoveWordFromString( fullPathToAsset, workingDirectory );
                                        dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "data/" ) );
                                        dk::core::RemoveWordFromString( fullPathToAsset, DUSK_STRING( "Assets/" ) );

                                        vfsMaterialPath = dkString_t( DUSK_STRING( "GameData/" ) ) + fullPathToAsset;
                                    }

                                    mesh.RenderMaterial = graphicsAssetCache->getMaterial( vfsMaterialPath.c_str() );
                                }
                            }

                            if ( mesh.RenderMaterial != nullptr ) {
                                ImGui::SameLine();
                                ImGui::Text( mesh.RenderMaterial->getName() );
                            }

                            ImGui::PopID();
                        }

                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }
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
