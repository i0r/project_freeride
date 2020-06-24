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
	f32         StartVector;
	f32         VectorPerInstance;
	u32         __PADDING__[2];
};

static constexpr size_t MAX_VECTOR_PER_INSTANCE = 1024;

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
		ResHandle_t VectorDataBuffer;
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

			BufferDesc vectorDataBufferDesc;
            vectorDataBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE;
			vectorDataBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			vectorDataBufferDesc.SizeInBytes = sizeof( dkVec4f ) * MAX_VECTOR_PER_INSTANCE;
			vectorDataBufferDesc.StrideInBytes = MAX_VECTOR_PER_INSTANCE;
            vectorDataBufferDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;

			passData.VectorDataBuffer = builder.allocateBuffer( vectorDataBufferDesc, SHADER_STAGE_VERTEX );

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
            Buffer* vectorBuffer = resources->getBuffer( passData.VectorDataBuffer );

            cmdList->pushEventMarker( DUSK_STRING( "Forward+ Light Pass" ) );

            // Clear render targets at the begining of the pass.
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

			// Upload buffer data
			const void* vectorBufferData = resources->getVectorBufferData();
			cmdList->updateBuffer( *vectorBuffer, vectorBufferData, sizeof( dkVec4f ) * MAX_VECTOR_PER_INSTANCE );

            const u32 samplerCount = camera->msaaSamplerCount;
            const FrameGraphResources::DrawCmdBucket& bucket = resources->getDrawCmdBucket( DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT );

            PerPassData perPassData;
            perPassData.StartVector = bucket.instanceDataStartOffset;
            perPassData.VectorPerInstance = bucket.vectorPerInstance;

            for ( const DrawCmd& cmd : bucket ) {
                const DrawCommandInfos& cmdInfos = cmd.infos;
                const Material* material = cmdInfos.material;
                const bool useInstancing = ( cmdInfos.instanceCount > 1 );

                // Upload vector buffer offset
				cmdList->updateBuffer( *perPassBuffer, &perPassData, sizeof( PerPassData ) );

                // Retrieve the PipelineState for the given RenderScenario.
                PipelineState* pipelineState = const_cast<Material*>( material )->bindForScenario( scenario, cmdList, psoCache, useInstancing, samplerCount );
                
				cmdList->bindBuffer( DUSK_STRING_HASH( "InstanceVectorBuffer" ), vectorBuffer );
                cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );
                cmdList->bindConstantBuffer( PerWorldBufferHashcode, perWorldBuffer );
                if ( scenario == Material::RenderScenario::Default_Editor ) {
                    cmdList->bindConstantBuffer( MaterialEditorBufferHashcode, materialEdBuffer );
                }

                // Re-setup the framebuffer (some permutations have a different framebuffer layout).
                cmdList->setupFramebuffer( Framebuffer, zbufferTarget );
                cmdList->prepareAndBindResourceList( pipelineState );

                const Buffer* bufferList[3] = { 
                    cmdInfos.vertexBuffers[eMeshAttribute::Position],
                    cmdInfos.vertexBuffers[eMeshAttribute::Normal],
                    cmdInfos.vertexBuffers[eMeshAttribute::UvMap_0]
                };

                // Bind vertex buffers
                cmdList->bindVertexBuffer( ( const Buffer** )bufferList, 3, 0);
                cmdList->bindIndiceBuffer( cmdInfos.indiceBuffer, !cmdInfos.useShortIndices );

				cmdList->drawIndexed( cmdInfos.indiceBufferCount, cmdInfos.instanceCount, cmdInfos.indiceBufferOffset );

				// Update vector buffer offset data
				perPassData.StartVector += ( cmdInfos.instanceCount * bucket.vectorPerInstance );
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
