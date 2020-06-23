/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "PrimitiveLightingTest.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Graphics/Mesh.h>
#include <Graphics/Model.h>
#include <Graphics/Material.h>

#include <Core/ViewFormat.h>

#include <Rendering/CommandList.h>

struct PerPassData 
{
    dkMat4x4f PerModelMatrix;
};

LightPassOutput AddPrimitiveLightTest( FrameGraph& frameGraph, ResHandle_t perSceneBuffer, Material::RenderScenario scenario )
{
    struct PassData {
        ResHandle_t output;
        ResHandle_t velocityBuffer;
        ResHandle_t depthBuffer;
        ResHandle_t PerPassBuffer;
        ResHandle_t PerViewBuffer;
        ResHandle_t PerSceneBuffer;
        ResHandle_t MaterialEdBuffer;
    };

    PassData& data = frameGraph.addRenderPass<PassData>(
        "Primitive Light",
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
            zBufferRenderTargetDesc.bindFlags = RESOURCE_BIND_DEPTH_STENCIL | RESOURCE_BIND_SHADER_RESOURCE;
            zBufferRenderTargetDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_D32_FLOAT;

            passData.depthBuffer = builder.allocateImage( zBufferRenderTargetDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

            ImageDesc velocityRtDesc;
            velocityRtDesc.dimension = ImageDesc::DIMENSION_2D;
            velocityRtDesc.format = eViewFormat::VIEW_FORMAT_R16G16_FLOAT;
            velocityRtDesc.usage = RESOURCE_USAGE_DEFAULT;
            velocityRtDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

            passData.velocityBuffer = builder.allocateImage( velocityRtDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

            BufferDesc perPassBuffer;
            perPassBuffer.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBuffer.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBuffer.SizeInBytes = sizeof( PerPassData );

            passData.PerPassBuffer = builder.allocateBuffer( perPassBuffer, SHADER_STAGE_VERTEX | SHADER_STAGE_PIXEL );
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
            passData.MaterialEdBuffer = builder.retrieveMaterialEdBuffer();

            passData.PerSceneBuffer = builder.readReadOnlyBuffer( perSceneBuffer );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputTarget = resources->getImage( passData.output );
            Image* velocityTarget = resources->getImage( passData.velocityBuffer );
            Image* zbufferTarget = resources->getImage( passData.depthBuffer );
            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            Buffer* perWorldBuffer = resources->getBuffer( passData.PerSceneBuffer );
            Buffer* materialEdBuffer = resources->getPersistentBuffer( passData.MaterialEdBuffer );

            cmdList->pushEventMarker( DUSK_STRING( "Forward+ Light Pass" ) );

            constexpr f32 ClearValue[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            Image* Framebuffer[2] = { outputTarget, velocityTarget };
            cmdList->clearRenderTargets( Framebuffer, 2u, ClearValue );
            cmdList->clearDepthStencil( zbufferTarget, 0.0f );

            // Update viewport (using image quality scaling)
            const CameraData* camera = resources->getMainCamera();

            dkVec2f scaledViewportSize = camera->viewportSize * camera->imageQuality;

            Viewport vp;
            vp.X = 0;
            vp.Y = 0;
            vp.Width = static_cast< i32 >( scaledViewportSize.x );
            vp.Height = static_cast< i32 >( scaledViewportSize.y );
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            ScissorRegion sr;
            sr.Top = 0;
            sr.Left = 0;
            sr.Right = static_cast< i32 >( scaledViewportSize.x );
            sr.Bottom = static_cast< i32 >( scaledViewportSize.y );

            cmdList->setViewport( vp );
            cmdList->setScissor( sr );

            const u32 samplerCount = camera->msaaSamplerCount;

            const FrameGraphResources::DrawCmdBucket& bucket = resources->getDrawCmdBucket( DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT );
            for ( const DrawCmd& cmd : bucket ) {
                const DrawCommandInfos& cmdInfos = cmd.infos;
                const Material* material = cmdInfos.material;

                PipelineState* pipelineState = const_cast<Material*>( material )->bindForScenario( scenario, cmdList, psoCache, samplerCount );

                cmdList->updateBuffer( *perPassBuffer, cmdInfos.modelMatrix, sizeof( dkMat4x4f ) );

                // TODO We need to rebind the cbuffer every time the pso might have changed... this is bad.
                cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
                cmdList->bindConstantBuffer( PerWorldBufferHashcode, perWorldBuffer );

                if ( scenario == Material::RenderScenario::Default_Editor ) {
                    cmdList->bindConstantBuffer( MaterialEditorBufferHashcode, materialEdBuffer );
                }

                cmdList->setupFramebuffer( Framebuffer, zbufferTarget );
                cmdList->prepareAndBindResourceList( pipelineState );

                const Buffer* bufferList[3] = { 
                    cmdInfos.vertexBuffers[eMeshAttribute::Position],
                    cmdInfos.vertexBuffers[eMeshAttribute::Normal],
                    cmdInfos.vertexBuffers[eMeshAttribute::UvMap_0]
                };

                // TODO Support vertex buffer offset
                cmdList->bindVertexBuffer( ( const Buffer** )bufferList, 3, 0 );
                cmdList->bindIndiceBuffer( cmdInfos.indiceBuffer, !cmdInfos.useShortIndices );

                cmdList->drawIndexed( cmdInfos.indiceBufferCount, cmdInfos.instanceCount, cmdInfos.indiceBufferOffset );
            }

            cmdList->popEventMarker();
        }
    );

    LightPassOutput output;
    output.OutputRenderTarget = data.output;
    output.OutputDepthTarget = data.depthBuffer;
    output.OutputVelocityTarget = data.velocityBuffer;

    return output;
}
