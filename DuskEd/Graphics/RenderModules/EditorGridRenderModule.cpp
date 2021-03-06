/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "EditorGridRenderModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include <Graphics/RenderModules/Generated/EditorGrid.generated.h>

EditorGridModule::EditorGridModule()
{

}

EditorGridModule::~EditorGridModule()
{

}

FGHandle EditorGridModule::addEditorGridPass( FrameGraph& frameGraph, FGHandle outputRenderTarget, FGHandle depthBuffer )
{
	struct PassData
	{
		FGHandle Output;
        FGHandle InputDepth;
        FGHandle PerPassBuffer;
        FGHandle PerViewBuffer;
	};

	constexpr PipelineStateDesc PipelineStateDefault = PipelineStateDesc(
		PipelineStateDesc::GRAPHICS,
		ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		DepthStencilStateDesc(),
		RasterizerStateDesc( CULL_MODE_NONE ),
		RenderingHelpers::BS_SpriteBlend,
		FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, VIEW_FORMAT_R16G16B16A16_FLOAT ) )
	);

    PassData passData = frameGraph.addRenderPass<PassData>(
        EditorGrid::RenderGrid_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.Output = builder.readImage( outputRenderTarget );
            passData.InputDepth = builder.readImage( depthBuffer );

            BufferDesc perPassBuffer;
            perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBuffer.SizeInBytes = sizeof( EditorGrid::RenderGridRuntimeProperties );
            perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.PerPassBuffer = builder.allocateBuffer( perPassBuffer, SHADER_STAGE_VERTEX | SHADER_STAGE_PIXEL );
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputTarget = resources->getImage( passData.Output );
            Image* depthBuffer = resources->getImage( passData.InputDepth );

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );

            PipelineState* passPipelineState = psoCache->getOrCreatePipelineState( PipelineStateDefault, EditorGrid::RenderGrid_ShaderBinding );

            cmdList->pushEventMarker( EditorGrid::RenderGrid_EventName );
            cmdList->bindPipelineState( passPipelineState );

            cmdList->setViewport( *resources->getMainViewport() );
            cmdList->setScissor( *resources->getMainScissorRegion() );

            FramebufferAttachment output( outputTarget );
            cmdList->setupFramebuffer( &output );

            EditorGrid::RenderGridRuntimeProperties properties;
            properties.GridInfos = dkVec4f( 4096.0f, 2.0f, 0.0f, 0.0f );
			properties.ThickLineColor = dkVec4f( 1.0f, 1.0f, 1.0f, 1.0f );
			properties.ThinLineColor = dkVec4f( 0.5f, 0.5f, 0.5f, 1.0f );

            cmdList->updateBuffer( *perPassBuffer, &properties, sizeof( EditorGrid::RenderGridRuntimeProperties ) );

            cmdList->bindImage( EditorGrid::RenderGrid_DepthBuffer_Hashcode, depthBuffer );
			cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
			cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            cmdList->prepareAndBindResourceList();

            cmdList->draw( 6u, 1 );

            cmdList->popEventMarker();
        }
    );

    return passData.Output;
}
