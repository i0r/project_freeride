/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "PresentRenderPass.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include "Generated/BuiltIn.generated.h"

void AddPresentRenderPass( FrameGraph& frameGraph, ResHandle_t imageToPresent )
{
    struct PassData {
        ResHandle_t input;
        ResHandle_t swapchain;
    };

#if DUSK_VULKAN
    constexpr eViewFormat SWAPCHAIN_VIEW_FORMAT = VIEW_FORMAT_B8G8R8A8_UNORM;
#else
    constexpr eViewFormat SWAPCHAIN_VIEW_FORMAT = VIEW_FORMAT_R8G8B8A8_UNORM;
#endif

    constexpr PipelineStateDesc DefaultPipelineState = PipelineStateDesc(
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc( CULL_MODE_BACK ),
        RenderingHelpers::BS_Disabled,
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_SWAPCHAIN, FramebufferLayoutDesc::DONT_CARE, SWAPCHAIN_VIEW_FORMAT ) ),
        { { RenderingHelpers::S_BilinearClampBorder }, 1 }
    );

    frameGraph.addRenderPass<PassData>(
        BuiltIn::PresentPass_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.input = builder.readImage( imageToPresent );
            passData.swapchain = builder.retrieveSwapchainBuffer();
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputTarget = resources->getPersitentImage( passData.swapchain );
            Image* inputTarget = resources->getImage( passData.input );

            PipelineState* passPipelineState = psoCache->getOrCreatePipelineState( DefaultPipelineState, BuiltIn::PresentPass_ShaderBinding );

            cmdList->pushEventMarker( BuiltIn::PresentPass_EventName );
            cmdList->bindPipelineState( passPipelineState );

            dkVec2u screenSize = resources->getScreenSize();

            Viewport screenVp;
            screenVp.X = 0;
            screenVp.Y = 0;
            screenVp.Width = static_cast<i32>( screenSize.x );
            screenVp.Height = static_cast< i32 >( screenSize.y );
            screenVp.MinDepth = 0.0f;
            screenVp.MaxDepth = 1.0f;

            ScissorRegion screenSr;
            screenSr.Top = 0;
            screenSr.Left = 0;
            screenSr.Right = static_cast< i32 >( screenSize.x );
            screenSr.Bottom = static_cast< i32 >( screenSize.y );

            cmdList->setViewport( screenVp );
            cmdList->setScissor( screenSr );

            cmdList->transitionImage( *inputTarget, eResourceState::RESOURCE_STATE_PIXEL_BINDED_RESOURCE );
            cmdList->transitionImage( *outputTarget, eResourceState::RESOURCE_STATE_SWAPCHAIN_BUFFER );

            cmdList->bindImage( BuiltIn::PresentPass_InputRenderTarget_Hashcode, inputTarget );

            cmdList->setupFramebuffer( &outputTarget, nullptr );
            cmdList->prepareAndBindResourceList( passPipelineState );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
        }
    );
}
