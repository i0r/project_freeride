/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "VolumetricLightingModule.h"

#include "Graphics/FrameGraph.h"
#include "Graphics/ShaderCache.h"
#include "Graphics/GraphicsAssetCache.h"
#include "Graphics/ShaderHeaders/Light.h"
#include "Graphics/RenderModules/Generated/DepthPyramid.generated.h"
#include "Graphics/RenderModules/Generated/SSR.generated.h"

#include "Maths/Helpers.h"

DUSK_DEV_VAR( VolumeTileDimension, "Dimension of a volume tile (in pixels). Should be smaller than the size of a light tile.", 8, i32 );
DUSK_DEV_VAR( VolumeDepthResolution, "Resolution of froxels depth for volumetric rendering (in pixels).", 64, i32 );

VolumetricLightModule::VolumetricLightModule()
{

}

VolumetricLightModule::~VolumetricLightModule()
{

}

FGHandle VolumetricLightModule::addVolumeScatteringPass( FrameGraph& frameGraph, FGHandle resolvedDepthBuffer, const u32 width, const u32 height )
{
    struct PassData
    {
        // VBuffer[0] : Scattering RGB  | Extinction A
        // VBuffer[1] : Emissive RGB    | Phase A
        FGHandle    VBuffer[2];
    };

	return FGHandle();
 //   const i32 invVolumeTileDim = ( 1.0f / VolumeTileDimension );
 //   i32 textureWidth = width * invVolumeTileDim;
 //   i32 textureHeight = height * invVolumeTileDim;
 //   i32 textureDepth = VolumeDepthResolution;

 //   frameGraph.addRenderPass<PassData>(
	//	"Volume Scattering (REPLACE ME)",
	//	[&]( FrameGraphBuilder& builder, PassData& passData ) {
	//		builder.useAsyncCompute();
	//		builder.setUncullablePass();

 //           ImageDesc vbufferDesc;
	//		vbufferDesc.dimension = ImageDesc::DIMENSION_3D;
	//		vbufferDesc.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
	//		vbufferDesc.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
	//		vbufferDesc.usage = RESOURCE_USAGE_DEFAULT;
	//		vbufferDesc.width = textureWidth;
 //           vbufferDesc.height = textureHeight;
 //           vbufferDesc.depth = textureDepth;

 //           passData.VBuffer[0] = builder.allocateImage( vbufferDesc );
 //           passData.VBuffer[1] = builder.allocateImage( vbufferDesc );
	//	},
	//	[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
	//	} 
	//);
}

void VolumetricLightModule::destroy( RenderDevice& renderDevice )
{

}

void VolumetricLightModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{

}
