/*
	Dusk Source Code
	Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "RenderDocHelper.h"

#if DUSK_USE_IMGUI
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#endif

#include "Graphics/RenderDocHelper.h"
#include "Graphics/GraphicsAssetCache.h"

#include "Core/Environment.h"
#include "Core/StringHelpers.h"

static constexpr const dkChar_t* RENDERDOC_ICON_PATH = DUSK_STRING( "GameData/textures/renderdoc_icon_40.dds" );

RenderDocHelperWidget::RenderDocHelperWidget( RenderDocHelper* renderDocHelperInstance )
	: isOpen( false )
	, awaitingFrameCapture( false )
	, frameToCaptureCount( 1u )
	, renderDocIcon( nullptr )
	, renderDocHelper( renderDocHelperInstance )
{

}

RenderDocHelperWidget::~RenderDocHelperWidget()
{

}

void RenderDocHelperWidget::displayEditorWindow()
{
	if ( !isOpen ) {
		return;
	}

	if( ImGui::Begin( "RenderDoc", &isOpen ) ) {
		if ( awaitingFrameCapture ) {
			awaitingFrameCapture = !renderDocHelper->openLatestCapture();
		}

		ImGui::Image( renderDocIcon, ImVec2( 40, 40 ) );

		ImGui::SameLine();

		ImGui::Text( "RenderDoc v%s\nF11: Capture a Single Frame", renderDocHelper->getAPIVersion() );
		ImGui::SetNextItemWidth( 60.0f );
		ImGui::DragInt( "Frame Count", &frameToCaptureCount, 1.0f, 1, 60 );

		bool refAllResources = renderDocHelper->isReferencingAllResources();
		if ( ImGui::Checkbox( "Reference All Resources", &refAllResources ) ) {
			renderDocHelper->referenceAllResources( refAllResources );
		}

		bool captureAllCmdLists = renderDocHelper->isCapturingAllCmdLists();
		if ( ImGui::Checkbox( "Capture All Command Lists", &captureAllCmdLists ) ) {
			renderDocHelper->captureAllCmdLists( captureAllCmdLists );
		}

		if ( ImGui::Button( "Capture" ) ) {
			renderDocHelper->triggerCapture( frameToCaptureCount );
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Capture & Analyze" ) ) {
			renderDocHelper->triggerCapture( frameToCaptureCount );

			awaitingFrameCapture = true;
		}

		if ( ImGui::Button( "Delete captures (*.rdc)" ) ) {
			const char* storageFolder = renderDocHelper->getCaptureStorageFolder();

			std::vector<dkString_t> captureFiles;
            dk::core::GetFilesByExtension( StringToDuskString( storageFolder ), DUSK_STRING( "*.rdc*" ), captureFiles );

			for ( const dkString_t& captureFile : captureFiles ) {
				dk::core::DeleteFile( captureFile );
			}
		}

		ImGui::End();
	}
}

void RenderDocHelperWidget::openWindow()
{
	isOpen = true;
}

void RenderDocHelperWidget::loadCachedResources( GraphicsAssetCache* graphicsAssetCache )
{
	renderDocIcon = graphicsAssetCache->getImage( RENDERDOC_ICON_PATH, true );
}
