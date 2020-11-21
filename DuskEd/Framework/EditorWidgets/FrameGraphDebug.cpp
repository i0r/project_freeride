/*
	Dusk Source Code
	Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "FrameGraphDebug.h"

#if DUSK_USE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#endif

#include "Graphics/FrameGraph.h"
#include "Graphics/GraphicsAssetCache.h"

#include "Core/Environment.h"
#include "Core/StringHelpers.h"

// Test a bit in a given bitfield. If the bit is set, append flagAsString to the builtString.
template<u32 BitToTest>
static DUSK_INLINE void TestAndAppendFlagToString( std::string& builtString, const u32 bitfield, const char* bitValueAsString )
{
	if ( bitfield & BitToTest ) {
		if ( !builtString.empty() ) {
			builtString.append( " | " );
		}

		builtString.append( bitValueAsString );
	}
}

// Convert a bitfield of eResourceBind flags to string.
static std::string BuildBindingString( const u32 bindFlags )
{
	std::string bindFlagsString;

	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_UNBINDABLE>( bindFlagsString, bindFlags, "RESOURCE_BIND_UNBINDABLE" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_CONSTANT_BUFFER>( bindFlagsString, bindFlags, "RESOURCE_BIND_CONSTANT_BUFFER" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_VERTEX_BUFFER>( bindFlagsString, bindFlags, "RESOURCE_BIND_VERTEX_BUFFER" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_INDICE_BUFFER>( bindFlagsString, bindFlags, "RESOURCE_BIND_INDICE_BUFFER" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW>( bindFlagsString, bindFlags, "RESOURCE_BIND_UNORDERED_ACCESS_VIEW" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_STRUCTURED_BUFFER>( bindFlagsString, bindFlags, "RESOURCE_BIND_STRUCTURED_BUFFER" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_INDIRECT_ARGUMENTS>( bindFlagsString, bindFlags, "RESOURCE_BIND_INDIRECT_ARGUMENTS" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_SHADER_RESOURCE>( bindFlagsString, bindFlags, "RESOURCE_BIND_SHADER_RESOURCE" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_RENDER_TARGET_VIEW>( bindFlagsString, bindFlags, "RESOURCE_BIND_RENDER_TARGET_VIEW" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_DEPTH_STENCIL>( bindFlagsString, bindFlags, "RESOURCE_BIND_DEPTH_STENCIL" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_APPEND_STRUCTURED_BUFFER>( bindFlagsString, bindFlags, "RESOURCE_BIND_APPEND_STRUCTURED_BUFFER" );
	TestAndAppendFlagToString<eResourceBind::RESOURCE_BIND_RAW>( bindFlagsString, bindFlags, "RESOURCE_BIND_RAW" );

	return bindFlagsString;
}

// Convert a bitfield of eShaderStage flags to string.
static DUSK_INLINE std::string BuildStageBindingString( const u32 bindFlags )
{
	std::string bindFlagsString;

	TestAndAppendFlagToString<eShaderStage::SHADER_STAGE_VERTEX>( bindFlagsString, bindFlags, "SHADER_STAGE_VERTEX" );
	TestAndAppendFlagToString<eShaderStage::SHADER_STAGE_TESSELATION_CONTROL>( bindFlagsString, bindFlags, "SHADER_STAGE_TESSELATION_CONTROL" );
	TestAndAppendFlagToString<eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION>( bindFlagsString, bindFlags, "SHADER_STAGE_TESSELATION_EVALUATION" );
	TestAndAppendFlagToString<eShaderStage::SHADER_STAGE_PIXEL>( bindFlagsString, bindFlags, "SHADER_STAGE_PIXEL" );
	TestAndAppendFlagToString<eShaderStage::SHADER_STAGE_COMPUTE>( bindFlagsString, bindFlags, "SHADER_STAGE_COMPUTE" );

	return bindFlagsString;
}

static const char* GPUResourceUsageToString( const eResourceUsage usage )
{
	switch ( usage ) {
	case RESOURCE_USAGE_DEFAULT:
		return "Default";
	case RESOURCE_USAGE_STATIC:
		return "Static";
	case RESOURCE_USAGE_DYNAMIC:
		return "Dynamic";
	case RESOURCE_USAGE_STAGING:
		return "Staging";
	default:
		return "Unknown";
	}
}

static DUSK_INLINE std::string BuildFrameGraphFlags( const u32 bindFlags )
{
	std::string bindFlagsString;

	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS>( bindFlagsString, bindFlags, "USE_PIPELINE_DIMENSIONS" );
	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::USE_PIPELINE_SAMPLER_COUNT>( bindFlagsString, bindFlags, "USE_PIPELINE_SAMPLER_COUNT" );
	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE>( bindFlagsString, bindFlags, "NO_MULTISAMPLE" );
	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE>( bindFlagsString, bindFlags, "USE_PIPELINE_DIMENSIONS_ONE" );
	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::USE_SCREEN_SIZE>( bindFlagsString, bindFlags, "USE_SCREEN_SIZE" );
	TestAndAppendFlagToString<FrameGraphBuilder::eImageFlags::REQUEST_PER_MIP_RESOURCE_VIEW>( bindFlagsString, bindFlags, "REQUEST_PER_MIP_RESOURCE_VIEW" );

	return bindFlagsString;
}

static DUSK_INLINE std::string BuildMiscFlagsString( const u32 bindFlags )
{
	std::string bindFlagsString;

	TestAndAppendFlagToString<ImageDesc::IS_CUBE_MAP>( bindFlagsString, bindFlags, "IS_CUBE_MAP" );
	TestAndAppendFlagToString<ImageDesc::ENABLE_HARDWARE_MIP_GENERATION>( bindFlagsString, bindFlags, "ENABLE_HARDWARE_MIP_GENERATION" );

	return bindFlagsString;
}

static const char* ImageDimensionToString( const u32 dimension )
{
	switch ( dimension ) {
	case ImageDesc::DIMENSION_1D:
		return "1D";
	case ImageDesc::DIMENSION_2D:
		return "2D";
	case ImageDesc::DIMENSION_3D:
		return "3D";
	default:
		return "??";
	}
}

FrameGraphDebugWidget::FrameGraphDebugWidget()
	: frameGraphInstance( nullptr )
	, isOpen( false )
	, selectedBufferIndex( ~0u )
	, selectedImageIndex( ~0u )
{

}

FrameGraphDebugWidget::~FrameGraphDebugWidget()
{
	frameGraphInstance = nullptr;
}

void FrameGraphDebugWidget::displayEditorWindow( const FrameGraph* frameGraph )
{
	if ( !isOpen ) {
		return;
	}

	frameGraphInstance = frameGraph;

	std::vector<FGBufferInfosEditor> bufferInfos;
	frameGraph->retrieveBufferEditorInfos( bufferInfos );

	std::vector<FGImageInfosEditor> imageInfos;
	frameGraph->retrieveImageEditorInfos( imageInfos );

	if ( ImGui::Begin( "FrameGraph Infos", &isOpen ) ) {
		ImGui::Text( "MSAA Sampler Count: %u", frameGraphInstance->getMSAASamplerCount() );
		ImGui::Text( "Image Quality: %f", frameGraphInstance->getImageQuality() );
        ImGui::Text( "Buffer memory usage: %u bytes", frameGraphInstance->getBufferMemoryUsage() );

		const i32 renderPassCount = frameGraphInstance->getRenderPassCount();
        ImGui::Text( "RenderPass count: %i", renderPassCount );

        ImGui::Separator();
		if ( ImGui::TreeNode( "RenderPasses" ) ) {
			for ( i32 passIdx = 0; passIdx < renderPassCount; passIdx++ ) {
				ImGui::TreeNodeEx( frameGraphInstance->getRenderPassName( passIdx ) );
			}
		}

		ImGui::Separator();
		if ( ImGui::TreeNode( "Buffers" ) ) {
			u32 bufferIdx = 0u;
			for ( const FGBufferInfosEditor& bufferInfosEd : bufferInfos ) {
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
				const bool isSelected = ( bufferIdx == selectedBufferIndex );

				if ( isSelected )
					flags |= ImGuiTreeNodeFlags_Selected;

				std::string bufferName = "Buffer_" + std::to_string( bufferIdx );
				bool isOpened = ImGui::TreeNodeEx( bufferName.c_str(), flags );

				if ( ImGui::IsItemClicked() ) {
					selectedBufferIndex = bufferIdx;
				}

				if ( isOpened ) {
					ImGui::TreePop();
				}

				bufferIdx++;
			}

			// Reset the selected buffer index if the buffer list has changed
			// (for safety sake; this won't take in account real changes).
			if ( selectedBufferIndex >= bufferIdx ) {
				selectedBufferIndex = ~0;
			}

			ImGui::TreePop();
		}

		ImGui::Separator();

		if ( ImGui::TreeNode( "Images" ) ) {
			u32 imageIdx = 0u;
			for ( const FGImageInfosEditor& imageInfosEd : imageInfos ) {
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
				const bool isSelected = ( imageIdx == selectedImageIndex );

				if ( isSelected )
					flags |= ImGuiTreeNodeFlags_Selected;

				std::string bufferName = "Image_" + std::to_string( imageIdx );
				bool isOpened = ImGui::TreeNodeEx( bufferName.c_str(), flags );

				if ( ImGui::IsItemClicked() ) {
					selectedImageIndex = imageIdx;
				}

				if ( isOpened ) {
					ImGui::TreePop();
				}

				imageIdx++;
			}

			// Reset the selected buffer index if the buffer list has changed
			// (for safety sake; this won't take in account real changes).
			if ( selectedImageIndex >= imageIdx ) {
				selectedImageIndex = ~0;
			}

			ImGui::TreePop();
		}
		ImGui::End();
	}

	if ( selectedBufferIndex != ~0u ) {
		if ( ImGui::Begin( "FrameGraph Buffer Infos", &isOpen ) ) {
			const FGBufferInfosEditor& selectedBufferInfos = bufferInfos[selectedBufferIndex];

			ImGui::Text( "Reference Count: %u", selectedBufferInfos.ReferenceCount );

			ImGui::Text( "Size: %u bytes", selectedBufferInfos.Description.SizeInBytes );
			ImGui::Text( "Stride: %u bytes", selectedBufferInfos.Description.StrideInBytes );
			ImGui::Text( "Usage: %s", GPUResourceUsageToString( selectedBufferInfos.Description.Usage ) );
			ImGui::Text( "BindFlags: %s", BuildBindingString( selectedBufferInfos.Description.BindFlags ).c_str() );
			ImGui::Text( "StageBinding: %s", BuildStageBindingString( selectedBufferInfos.ShaderStageBinding ).c_str() );

			ImGui::End();
		}
	}

	if ( selectedImageIndex != ~0u ) {
		if ( ImGui::Begin( "FrameGraph Image Infos", &isOpen ) ) {
			ImVec2 winSize = ImGui::GetWindowSize();

			const FGImageInfosEditor& selectedImageInfos = imageInfos[selectedImageIndex];

			ImGui::Text( "Reference Count: %u", selectedImageInfos.ReferenceCount );
			ImGui::Text( "FrameGraph Flags: %s", BuildFrameGraphFlags( selectedImageInfos.GraphFlags ).c_str() );

			const ImageDesc& description = selectedImageInfos.Description;
			ImGui::Text( "Dimension: %s", ImageDimensionToString( description.dimension ) );
			ImGui::Text( "Size (w x h x d): %u x %u x %u", description.width, description.height, description.depth );
			ImGui::Text( "Array Length: %u", description.arraySize );
			ImGui::Text( "Format: %s", VIEW_FORMAT_STRING[description.format] );
			ImGui::Text( "Mip Count: %u", description.mipCount );
			ImGui::Text( "Sampler Count: %u", description.samplerCount );
			ImGui::Text( "BindFlags: %s", BuildBindingString( description.bindFlags ).c_str() );
			ImGui::Text( "MiscFlags: %s", BuildMiscFlagsString( description.miscFlags ).c_str() );
			ImGui::Text( "Usage: %s", GPUResourceUsageToString( description.usage ) );

			dkVec2f sizeRatio = dkVec2f( static_cast<f32>( description.width ), static_cast<f32>( description.height ) ) / dkVec2f( winSize.x, winSize.y );
			dkVec2f ratioInv = 1.0f / sizeRatio;
			constexpr f32 WindowBorderOffset = 32.0f;
			ImGui::Image( selectedImageInfos.AllocatedImage, ImVec2( description.width * ratioInv.x - WindowBorderOffset, description.height * ratioInv.y - WindowBorderOffset ) );

			ImGui::End();
		}
	}
}

void FrameGraphDebugWidget::openWindow()
{
	isOpen = true;
}
