/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EditorInterface.h"

#include "Editor.h"

#include "Graphics/RenderDocHelper.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/LightGrid.h"

#include "Framework/MaterialEditor.h"
#include "Framework/World.h"
#include "Framework/Cameras/FreeCamera.h"
#include "Framework/EntityEditor.h"
#include "Framework/Transform.h"
#include "Framework/Transaction/TransactionHandler.h"

#include "Core/Display/DisplaySurface.h"

#include "Graphics/ShaderHeaders/Light.h"
#include "Graphics/RenderModules/AtmosphereRenderModule.h"

#if DUSK_USE_IMGUI
#include "Graphics/RenderModules/ImGuiRenderModule.h"
#include "Framework/EditorWidgets/LoggingConsole.h"
#include "Framework/ImGuiUtilities.h"

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"

#include "ThirdParty/ImGuizmo/ImGuizmo.h"

#include "ThirdParty/Google/IconsMaterialDesign.h"
#endif

#if DUSK_USE_RENDERDOC
#include "Framework/EditorWidgets/RenderDocHelper.h"

extern RenderDocHelper* g_RenderDocHelper;
#endif

extern MaterialEditor* g_MaterialEditor;
extern WorldRenderer* g_WorldRenderer;
extern World* g_World;
extern Entity g_PickedEntity;
extern RenderDevice* g_RenderDevice;
extern FreeCamera* g_FreeCamera;
extern EntityEditor* g_EntityEditor;
extern dkVec2u g_ViewportWindowPosition;
extern DisplaySurface* g_DisplaySurface;
extern bool g_RightClickMenuOpened;
extern bool g_IsContextMenuOpened;
extern bool g_IsMouseOverViewportWindow;

EditorInterface::EditorInterface( BaseAllocator* allocator )
	: memoryAllocator( allocator )
#if DUSK_USE_RENDERDOC
	, renderDocWidget( dk::core::allocate<RenderDocHelperWidget>( memoryAllocator, g_RenderDocHelper ) )
#endif
	, menuBarHeight( 0.0f )  
	, isResizing( false )
{

}

EditorInterface::~EditorInterface()
{
#if DUSK_USE_RENDERDOC
	dk::core::free( memoryAllocator, renderDocWidget );
#endif
}

void EditorInterface::display( FrameGraph& frameGraph, ImGuiRenderModule* renderModule )
{
	const dkVec2u& ScreenSize = *EnvironmentVariables::getVariable<dkVec2u>( DUSK_STRING_HASH( "ScreenSize" ) );
	const bool IsFirstLaunch = *EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "IsFirstLaunch" ) );

	renderModule->lockForRendering();

	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	displayMenuBar();

	ImGui::SetNextWindowSize( ImVec2( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) - menuBarHeight ) );
	ImGui::SetNextWindowPos( ImVec2( 0, menuBarHeight ) );
	ImGui::Begin( "MasterWindow", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing );
	
	static bool NeedDockSetup = IsFirstLaunch;
	ImGuiIO& io = ImGui::GetIO();

	ImGuiID dockspaceID = ImGui::GetID( "EdDockspace" );
	if ( NeedDockSetup ) {
		NeedDockSetup = false;

		ImGuiContext* ctx = ImGui::GetCurrentContext();
		ImGui::DockBuilderRemoveNode( dockspaceID );
		ImGui::DockBuilderAddNode( dockspaceID );

		ImGuiID dock_main_id = dockspaceID;
		ImGuiID dock_id_prop = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id );
		ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id );

		ImGui::DockBuilderDockWindow( "Viewport", dock_main_id );
		ImGui::DockBuilderDockWindow( "Console", dock_id_bottom );
		ImGui::DockBuilderDockWindow( "Inspector", dock_id_prop );
		ImGui::DockBuilderDockWindow( "RenderDoc", dock_id_prop );
		ImGui::DockBuilderFinish( dockspaceID );
	}
	ImGui::DockSpace( dockspaceID );

#if DUSK_USE_RENDERDOC
	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	renderDocWidget->displayEditorWindow();
#endif

	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	dk::editor::DisplayLoggingConsole();

	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	g_MaterialEditor->displayEditorWindow();

	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	if ( ImGui::Begin( ICON_MD_ACCESS_TIME " Time Of Day" ) ) {
		if ( ImGui::TreeNode( "Atmosphere" ) ) {
			AtmosphereRenderModule* atmosphereRenderModule = g_WorldRenderer->getAtmosphereRenderingModule();
			LightGrid* lightGrid = g_WorldRenderer->getLightGrid();
			DirectionalLightGPU* sunLight = lightGrid->getDirectionalLightData();

			if ( ImGui::Button( "Recompute LUTs" ) ) {
				atmosphereRenderModule->triggerLutRecompute();
			}

			static bool overrideTod = true;
			ImGui::Checkbox( "Override ToD", &overrideTod );
			if ( overrideTod ) {
				ImGui::Text( ICON_MD_WB_SUNNY " Sun Settings" );
				if ( ImGui::SliderFloat2( "Sun Pos", ( float* )&sunLight->SphericalCoordinates, -1.0f, 1.0f ) ) {
					sunLight->NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( sunLight->SphericalCoordinates[0], sunLight->SphericalCoordinates[1] );
					atmosphereRenderModule->setSunAngles( sunLight->SphericalCoordinates[0], sunLight->SphericalCoordinates[1] );
				}

				ImGui::InputFloat( "Angular Radius", &sunLight->AngularRadius );

				// Cone solid angle formula = 2PI(1-cos(0.5*AD*(PI/180)))
				const f32 solidAngle = ( 2.0f * dk::maths::PI<f32>() ) * ( 1.0f - cos( sunLight->AngularRadius ) );

				f32 illuminance = sunLight->IlluminanceInLux / solidAngle;
				if ( ImGui::InputFloat( ICON_MD_LIGHTBULB_OUTLINE " Illuminance (lux)", &illuminance ) ) {
					sunLight->IlluminanceInLux = illuminance * solidAngle;
				}

				ImGui::ColorEdit3( ICON_MD_PALETTE " Color", ( float* )&sunLight->ColorLinearSpace );
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();

	/*if ( ImGui::Begin( "Cascaded Shadow Map" ) ) {
		ImVec2 winSize = ImGui::GetWindowSize();
		ImGui::Image( static_cast< ImTextureID >( g_WorldRenderer->getCascadedShadowRenderModule()->getShadowAtlas() ), ImVec2( winSize.x - 32.0f, winSize.x / 2 - 32.0f ) );

		ImGui::Text( "Global Shadow Matrix" );
		dkMat4x4f globalShadowMat = g_WorldRenderer->getCascadedShadowRenderModule()->getGlobalShadowMatrix();
		dk::imgui::InputMatrix4x4( globalShadowMat );

		ImGui::End();
	}*/

	// TODO Crappy stuff to prototype/test quickly.
	static ImVec2 viewportWinSize;
	static ImVec2 viewportWinPos;
	extern dkVec2f viewportSize;

	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	if ( ImGui::Begin( ICON_MD_SCREEN_SHARE " Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar ) ) {
		static ImVec2 prevWinSize = ImGui::GetWindowSize();

		ImVec2 winSize = ImGui::GetWindowSize();

		viewportWinSize = ImGui::GetWindowSize();
		viewportWinPos = ImGui::GetWindowPos();

		g_ViewportWindowPosition.x = static_cast< u32 >( viewportWinPos.x );
		g_ViewportWindowPosition.y = static_cast< u32 >( viewportWinPos.y );

		if ( !isResizing && ( winSize.x != prevWinSize.x || winSize.y != prevWinSize.y ) ) {
			isResizing = true;
		} else if ( isResizing ) {
			f32 deltaX = ( winSize.x - prevWinSize.x );
			f32 deltaY = ( winSize.y - prevWinSize.y );

			// Wait until the drag is over to resize stuff internally.
			if ( deltaX == 0.0f && deltaY == 0.0f ) {
				viewportSize.x = winSize.x;
				viewportSize.y = winSize.y;

				const f32 DefaultCameraFov = *EnvironmentVariables::getVariable<f32>( DUSK_STRING_HASH( "DefaultCameraFov" ) );
				g_FreeCamera->setProjectionMatrix( DefaultCameraFov, viewportSize.x, viewportSize.y );

				extern Viewport vp;
				extern ScissorRegion vpSr;

				vp.Width = static_cast< i32 >( viewportSize.x );
				vp.Height = static_cast< i32 >( viewportSize.y );

				vpSr.Right = static_cast< i32 >( viewportSize.x );
				vpSr.Bottom = static_cast< i32 >( viewportSize.y );

				isResizing = false;
			}
		}
		prevWinSize = winSize;

		g_IsMouseOverViewportWindow = ImGui::IsWindowHovered();

		winSize.x -= 4;
		winSize.y -= 4;

		ImGui::Image( static_cast< ImTextureID >( frameGraph.getPresentRenderTarget() ), winSize );
		if ( g_RightClickMenuOpened = dk::imgui::BeginPopupContextWindowWithCondition( "Viewport Popup", g_IsContextMenuOpened ) ) {
			g_IsContextMenuOpened = false;

			if ( ImGui::BeginMenu( ICON_MD_CREATE " New Entity..." ) ) {
				if ( ImGui::MenuItem( "Static Mesh" ) ) {
					g_PickedEntity = g_World->createStaticMesh();

					CameraData& cameraData = g_FreeCamera->getData();

					const dkMat4x4f& projMat = cameraData.finiteProjectionMatrix;
					const dkMat4x4f& viewMat = cameraData.viewMatrix;

					dkMat4x4f inverseViewProj = ( viewMat * projMat ).inverse();

					extern i32 shiftedMouseX;
					extern i32 shiftedMouseY;
					dkVec3f ray = {
						( ( ( 2.0f * shiftedMouseX ) / viewportWinSize.x ) - 1 ) / inverseViewProj[0][0],
						-( ( ( 2.0f * shiftedMouseY ) / viewportWinSize.y ) - 1 ) / inverseViewProj[1][1],
						1.0f
					};

					dkVec3f rayDirection =
					{
						ray.x * inverseViewProj[0][0] + ray.y * inverseViewProj[1][0] + ray.z * inverseViewProj[2][0],
						ray.x * inverseViewProj[0][1] + ray.y * inverseViewProj[1][1] + ray.z * inverseViewProj[2][1],
						ray.x * inverseViewProj[0][2] + ray.y * inverseViewProj[1][2] + ray.z * inverseViewProj[2][2]
					};

					f32 w = ray.x * inverseViewProj[0][3] + ray.y * inverseViewProj[1][3] + ray.z * inverseViewProj[2][3];

					rayDirection *= ( 1.0f / w );

					f32 backup = rayDirection.y;
					rayDirection.y = rayDirection.z;
					rayDirection.z = backup;

					dkVec3f entityPosition = cameraData.worldPosition + ( g_FreeCamera->getEyeDirection() * 10.0f + rayDirection );
					TransformDatabase* transformDatabase = g_World->getTransformDatabase();

					TransformDatabase::EdInstanceData instanceData = transformDatabase->getEditorInstanceData( transformDatabase->lookup( g_PickedEntity ) );
					instanceData.Position->x = entityPosition.x;
					instanceData.Position->y = entityPosition.y;
					instanceData.Position->z = entityPosition.z;
				}

				ImGui::EndMenu();
			}

			if ( g_PickedEntity.getIdentifier() != Entity::INVALID_ID ) {
				if ( ImGui::MenuItem( ICON_MD_DELETE " Delete" ) ) {
					g_World->releaseEntity( g_PickedEntity );
					g_PickedEntity.setIdentifier( Entity::INVALID_ID );
				}
			}

			ImGui::EndPopup();
		}
	}
	ImGui::End();

	ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
	dkVec4f viewportWidgetBounds = dkVec4f( viewportWinPos.x, viewportWinPos.y, viewportWinSize.x, viewportWinSize.y );
	g_EntityEditor->displayEditorWindow( g_FreeCamera->getData(), viewportWidgetBounds );

	ImGui::End();
	ImGui::Render();

	renderModule->unlock();
}

void EditorInterface::loadCachedResources( GraphicsAssetCache* graphicsAssetCache )
{
#if DUSK_USE_RENDERDOC
	renderDocWidget->loadCachedResources( graphicsAssetCache );
#endif
}

void EditorInterface::displayMenuBar()
{
	if ( ImGui::BeginMainMenuBar() ) {
		menuBarHeight = ImGui::GetWindowSize().y;

		displayFileMenu();
		displayEditMenu();
		displayDebugMenu();
		displayGraphicsMenu();
		displayWindowMenu();

		if ( g_RenderDocHelper->isAvailable() && ImGui::ImageButton( renderDocWidget->getIconBitmap(), ImVec2( menuBarHeight - 2, menuBarHeight - 2 ), ImVec2( 0, 0 ), ImVec2( 1, 1 ), 1 ) ) {
			renderDocWidget->openWindow();
		}
	}
	ImGui::EndMainMenuBar();
}

void EditorInterface::displayWindowMenu()
{
	if ( ImGui::BeginMenu( "Window" ) ) {
		if ( ImGui::MenuItem( "RenderDoc", nullptr, nullptr, g_RenderDocHelper->isAvailable() ) ) {
			renderDocWidget->openWindow();
		}

		if ( ImGui::MenuItem( ICON_MD_TEXTURE " Material Editor" ) ) {
			g_MaterialEditor->openEditorWindow();
		}

		if ( ImGui::MenuItem( ICON_MD_ZOOM_IN " Inspector" ) ) {
			g_EntityEditor->openEditorWindow();
		}

		ImGui::EndMenu();
	}
}

void EditorInterface::displayGraphicsMenu()
{
	if ( ImGui::BeginMenu( "Graphics" ) ) {
		u32& MSAASamplerCount = *EnvironmentVariables::getVariable<u32>( DUSK_STRING_HASH( "MSAASamplerCount" ) );
		f32& ImageQuality = *EnvironmentVariables::getVariable<f32>( DUSK_STRING_HASH( "ImageQuality" ) );
		bool& EnableVSync = *EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "EnableVSync" ) );
        bool* EnableTAA = EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "EnableTAA" ) );
		eWindowMode* WindowMode = EnvironmentVariables::getVariable<eWindowMode>( DUSK_STRING_HASH( "WindowMode" ) );

        if ( ImGui::BeginMenu( "Display Mode" ) ) {
            if ( ImGui::MenuItem( "Windowed", nullptr, *WindowMode == eWindowMode::WINDOWED_MODE ) ) {
                *WindowMode = eWindowMode::WINDOWED_MODE;
                g_DisplaySurface->changeDisplayMode( eDisplayMode::WINDOWED );
            }
            if ( ImGui::MenuItem( "Fullscreen", nullptr, *WindowMode == eWindowMode::FULLSCREEN_MODE ) ) {
                *WindowMode = eWindowMode::FULLSCREEN_MODE;
                g_DisplaySurface->changeDisplayMode( eDisplayMode::FULLSCREEN );
            }
            if ( ImGui::MenuItem( "Borderless", nullptr, *WindowMode == eWindowMode::BORDERLESS_MODE ) ) {
                *WindowMode = eWindowMode::BORDERLESS_MODE;
                g_DisplaySurface->changeDisplayMode( eDisplayMode::BORDERLESS );
            }
            ImGui::EndMenu();
        }

		if ( ImGui::BeginMenu( "MSAA" ) ) {
			if ( ImGui::MenuItem( "x1", nullptr, MSAASamplerCount == 1 ) ) {
				MSAASamplerCount = 1;
				g_FreeCamera->setMSAASamplerCount( MSAASamplerCount );
			}
			if ( ImGui::MenuItem( "x2", nullptr, MSAASamplerCount == 2 ) ) {
				MSAASamplerCount = 2;
				g_FreeCamera->setMSAASamplerCount( MSAASamplerCount );
			}
			if ( ImGui::MenuItem( "x4", nullptr, MSAASamplerCount == 4 ) ) {
				MSAASamplerCount = 4;
				g_FreeCamera->setMSAASamplerCount( MSAASamplerCount );
			}
			if ( ImGui::MenuItem( "x8", nullptr, MSAASamplerCount == 8 ) ) {
				MSAASamplerCount = 8;
				g_FreeCamera->setMSAASamplerCount( MSAASamplerCount );
			}
			ImGui::EndMenu();
		}

		if ( ImGui::SliderFloat( "Image Quality", &ImageQuality, 0.01f, 4.0f ) ) {
			g_FreeCamera->setImageQuality( ImageQuality );
		}

		if ( ImGui::Checkbox( "VSync", &EnableVSync ) ) {
			g_RenderDevice->enableVerticalSynchronisation( EnableVSync );
		}

		ImGui::Checkbox( "Enable Temporal AntiAliasing", EnableTAA );

		ImGui::EndMenu();
	}
}

void EditorInterface::displayDebugMenu()
{
	if ( ImGui::BeginMenu( "Debug" ) ) {
		bool* DrawDebugSphere = EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "DisplayBoundingSphere" ) );

		ImGui::Checkbox( "Display Geometry BoundingSphere", DrawDebugSphere );

		ImGui::EndMenu();
	}
}

void EditorInterface::displayEditMenu()
{
	if ( ImGui::BeginMenu( "Edit" ) ) {
		extern TransactionHandler* g_TransactionHandler;

		if ( ImGui::MenuItem( ICON_MD_UNDO " Undo", g_TransactionHandler->getPreviousActionName(), false, g_TransactionHandler->canUndo() ) ) {
			g_TransactionHandler->undo();
		}

		if ( ImGui::MenuItem( ICON_MD_REDO " Redo", g_TransactionHandler->getNextActionName(), false, g_TransactionHandler->canRedo() ) ) {
			g_TransactionHandler->redo();
		}

		ImGui::Separator();

		if ( ImGui::MenuItem( ICON_MD_DELETE " Delete", nullptr, false, ( g_PickedEntity.getIdentifier() != Entity::INVALID_ID ) ) ) {
			g_World->releaseEntity( g_PickedEntity );
			g_PickedEntity.setIdentifier( Entity::INVALID_ID );
		}

		ImGui::EndMenu();
	}
}

void EditorInterface::displayFileMenu()
{
	if ( ImGui::BeginMenu( "File" ) ) {
		ImGui::EndMenu();
	}
}
