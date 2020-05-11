/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "PrimitiveLightingTest.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

PrimitiveLighting::PrimitiveLighting_Generic_Output AddPrimitiveLightTest( FrameGraph& frameGraph, BuiltPrimitive& primitive, ResHandle_t perSceneBuffer )
{
    struct PassData {
        ResHandle_t output;
        ResHandle_t depthBuffer;
        ResHandle_t PerPassBuffer;
        ResHandle_t PerViewBuffer;
        ResHandle_t PerSceneBuffer;
    };

    PassData& data = frameGraph.addRenderPass<PassData>(
        PrimitiveLighting::PrimitiveLighting_Generic_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            ImageDesc rtDesc;
            rtDesc.dimension = ImageDesc::DIMENSION_2D;
            rtDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            rtDesc.usage = RESOURCE_USAGE_DEFAULT;
            rtDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

            passData.output = builder.allocateImage( rtDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

            ImageDesc zBufferRenderTargetDesc;
            zBufferRenderTargetDesc.dimension = ImageDesc::DIMENSION_2D;
            zBufferRenderTargetDesc.format = eViewFormat::VIEW_FORMAT_R32_TYPELESS;
            zBufferRenderTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
            zBufferRenderTargetDesc.bindFlags = RESOURCE_BIND_DEPTH_STENCIL;
            zBufferRenderTargetDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_D32_FLOAT;

            passData.depthBuffer = builder.allocateImage( zBufferRenderTargetDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

            BufferDesc perPassBuffer;
            perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBuffer.SizeInBytes = sizeof( PrimitiveLighting::PrimitiveLighting_GenericRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( perPassBuffer, SHADER_STAGE_VERTEX | SHADER_STAGE_PIXEL );
            passData.PerViewBuffer = builder.retrievePerViewBuffer();

            passData.PerSceneBuffer = builder.readReadOnlyBuffer( perSceneBuffer );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputTarget = resources->getImage( passData.output );
            Image* zbufferTarget = resources->getImage( passData.depthBuffer );
            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            Buffer* perWorldBuffer = resources->getBuffer( passData.PerSceneBuffer );

            PipelineStateDesc DefaultPipelineState( PipelineStateDesc::GRAPHICS );
            DefaultPipelineState.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            DefaultPipelineState.DepthStencilState.EnableDepthWrite = true;
            DefaultPipelineState.DepthStencilState.EnableDepthTest = true;
            DefaultPipelineState.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_GREATER;

            DefaultPipelineState.RasterizerState.CullMode = CULL_MODE_FRONT;
            DefaultPipelineState.RasterizerState.UseTriangleCCW = true;

            DefaultPipelineState.FramebufferLayout.declareRTV( 0, VIEW_FORMAT_R16G16B16A16_FLOAT, FramebufferLayoutDesc::CLEAR );
            DefaultPipelineState.FramebufferLayout.declareDSV( VIEW_FORMAT_D32_FLOAT, FramebufferLayoutDesc::CLEAR );
            DefaultPipelineState.samplerCount = resources->getMainCamera()->msaaSamplerCount;
            DefaultPipelineState.InputLayout.Entry[0] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 0, 0, false, "POSITION" };
            DefaultPipelineState.InputLayout.Entry[1] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 1, 0, true, "NORMAL" };
            DefaultPipelineState.InputLayout.Entry[2] = { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 2, 0, true, "TEXCOORD" };
            DefaultPipelineState.depthClearValue = 0.0f;

            PipelineState* passPipelineState = psoCache->getOrCreatePipelineState( DefaultPipelineState, PrimitiveLighting::PrimitiveLighting_Generic_ShaderBinding );

            cmdList->pushEventMarker( PrimitiveLighting::PrimitiveLighting_Generic_EventName );
            cmdList->bindPipelineState( passPipelineState );
            
            cmdList->updateBuffer( *perPassBuffer, &dkMat4x4f::Identity, sizeof( dkMat4x4f ) );

            cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
            cmdList->bindConstantBuffer( PerWorldBufferHashcode, perWorldBuffer );

            cmdList->setViewport( *resources->getMainViewport() );
            cmdList->setScissor( *resources->getMainScissorRegion() );

            cmdList->setupFramebuffer( &outputTarget, zbufferTarget );
            cmdList->prepareAndBindResourceList( passPipelineState );

            cmdList->bindVertexBuffer( (const Buffer**)primitive.VertexAttributeBuffer, 3 );
            cmdList->bindIndiceBuffer( primitive.IndiceBuffer, true );

            cmdList->drawIndexed( primitive.IndiceCount, 1u );

            cmdList->popEventMarker();
        }
    );

    PrimitiveLighting::PrimitiveLighting_Generic_Output output;
    output.OutputRenderTarget = data.output;
    output.OutputDepthTarget = data.depthBuffer;
    return output;
}
