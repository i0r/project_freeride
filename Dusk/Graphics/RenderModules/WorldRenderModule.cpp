/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "WorldRenderModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Graphics/Mesh.h>
#include <Graphics/Model.h>
#include <Graphics/Material.h>

#include <Core/ViewFormat.h>

#include <Rendering/CommandList.h>
#include <Framework/Entity.h>

#include <Graphics/RenderModules/Generated/BuiltIn.generated.h>

#include "CascadedShadowRenderModule.h"

struct PerPassData
{
    dkMat4x4f   SunShadowMatrix;
	f32         StartVector;
	f32         VectorPerInstance;
	u32         __PADDING__[2];
};

static constexpr dkStringHash_t PickingBufferHashcode = DUSK_STRING_HASH( "PickingBuffer" );
static constexpr dkStringHash_t BrdfDfgLUTHascode = DUSK_STRING_HASH( "BrdfDfgLut" );
static constexpr dkStringHash_t IBLDiffuseHascode = DUSK_STRING_HASH( "IBLDiffuse" );
static constexpr dkStringHash_t IBLSpecularHascode = DUSK_STRING_HASH( "IBLSpecular" );
static constexpr dkStringHash_t BRDFInputSamplerHashcode = DUSK_STRING_HASH( "TextureSampler" );

#define TEXTURE_FILTERING_OPTION_LIST( option )\
    option( BILINEAR )\
    option( TRILINEAR )\
    option( ANISOTROPIC_8 )\
    option( ANISOTROPIC_16 )

DUSK_ENV_OPTION_LIST( TextureFiltering, TEXTURE_FILTERING_OPTION_LIST )

DUSK_ENV_VAR( TextureFiltering, BILINEAR, eTextureFiltering ) // "Defines texture filtering quality [Bilinear/Trilinear/Anisotropic (8)/Anisotropic (16)]"

WorldRenderModule::WorldRenderModule()
    : pickingBuffer( nullptr )
    , pickingReadbackBuffer( nullptr )
    , pickingFrameIndex( ~0 )
    , pickedEntityId( Entity::INVALID_ID )
    , isResultAvailable( false )
    , brdfDfgLut( nullptr )
{

}

WorldRenderModule::~WorldRenderModule()
{

}

void WorldRenderModule::destroy( RenderDevice& renderDevice )
{
    if ( pickingBuffer != nullptr ) {
        renderDevice.destroyBuffer( pickingBuffer );
        pickingBuffer = nullptr;
	}

	if ( pickingReadbackBuffer != nullptr ) {
		renderDevice.destroyBuffer( pickingReadbackBuffer );
        pickingReadbackBuffer = nullptr;
	}
}

void WorldRenderModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
	// Create the picking buffer.
	BufferDesc pickingBufferDesc;
	pickingBufferDesc.BindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_RAW;
    pickingBufferDesc.SizeInBytes = 4 * sizeof( u32 );
	pickingBufferDesc.StrideInBytes = 4;
	pickingBufferDesc.Usage = RESOURCE_USAGE_DEFAULT;
    pickingBufferDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32_TYPELESS;

	pickingBuffer = renderDevice.createBuffer( pickingBufferDesc );

	// Create the staging buffer for results readback.
	pickingBufferDesc.BindFlags = 0;
	pickingBufferDesc.Usage = RESOURCE_USAGE_STAGING;

	pickingReadbackBuffer = renderDevice.createBuffer( pickingBufferDesc );

#if DUSK_DEVBUILD
	renderDevice.setDebugMarker( *pickingBuffer, DUSK_STRING( "PickingBuffer" ) );
	renderDevice.setDebugMarker( *pickingReadbackBuffer, DUSK_STRING( "ReadBackPickingBuffer" ) );
#endif
}

WorldRenderModule::LightPassOutput WorldRenderModule::addPrimitiveLightPass( FrameGraph& frameGraph, ResHandle_t perSceneBuffer, ResHandle_t depthPrepassBuffer, Material::RenderScenario scenario, Image* iblDiffuse, Image* iblSpecular, const dkMat4x4f& globalShadowMatrix )
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
        ResHandle_t MaterialInputSampler;
		ResHandle_t CSMSliceBuffer;
		ResHandle_t CSMSlices;
	};

	const bool isPickingRequested = ( scenario == Material::RenderScenario::Default_Picking
								   || scenario == Material::RenderScenario::Default_Picking_Editor );

    // We need to clear the UAV buffer first. This is done with a cheap compute shader.
    if ( isPickingRequested ) {
        clearPickingBuffer( frameGraph );
    }

    PassData& data = frameGraph.addRenderPass<PassData>(
        "Forward+ Light Pass",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            ImageDesc rtDesc;
            rtDesc.dimension = ImageDesc::DIMENSION_2D;
            rtDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            rtDesc.usage = RESOURCE_USAGE_DEFAULT;
            rtDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

            passData.output = builder.allocateImage( rtDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

            passData.depthBuffer = builder.readReadOnlyImage( depthPrepassBuffer );

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

			SamplerDesc materialSamplerDesc;
			materialSamplerDesc.addressU = eSamplerAddress::SAMPLER_ADDRESS_WRAP;
			materialSamplerDesc.addressV = eSamplerAddress::SAMPLER_ADDRESS_WRAP;
			materialSamplerDesc.addressW = eSamplerAddress::SAMPLER_ADDRESS_WRAP;

			switch ( TextureFiltering ) {
			case eTextureFiltering::ANISOTROPIC_16:
				materialSamplerDesc.filter = eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_16;
				break;

			case eTextureFiltering::ANISOTROPIC_8:
				materialSamplerDesc.filter = eSamplerFilter::SAMPLER_FILTER_ANISOTROPIC_8;
				break;

			case eTextureFiltering::TRILINEAR:
				materialSamplerDesc.filter = eSamplerFilter::SAMPLER_FILTER_TRILINEAR;
				break;

			default:
			case eTextureFiltering::BILINEAR:
				materialSamplerDesc.filter = eSamplerFilter::SAMPLER_FILTER_BILINEAR;
				break;
			}

			passData.MaterialInputSampler = builder.allocateSampler( materialSamplerDesc );
            passData.CSMSliceBuffer = builder.retrievePersistentBuffer( CascadedShadowRenderModule::SliceBufferHashcode );
            passData.CSMSlices = builder.retrievePersistentImage( CascadedShadowRenderModule::SliceImageHashcode );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* outputTarget = resources->getImage( passData.output );
            Image* velocityTarget = resources->getImage( passData.velocityBuffer );
            Image* zbufferTarget = resources->getImage( passData.depthBuffer );
            Image* sliceShadow = resources->getPersitentImage( passData.CSMSlices );

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            Buffer* perWorldBuffer = resources->getBuffer( passData.PerSceneBuffer );
            Buffer* materialEdBuffer = resources->getPersistentBuffer( passData.MaterialEdBuffer );
            Buffer* vectorBuffer = resources->getBuffer( passData.VectorDataBuffer );
            Buffer* sliceBuffer = resources->getPersistentBuffer( passData.CSMSliceBuffer );

            Sampler* materialSampler = resources->getSampler( passData.MaterialInputSampler );

            cmdList->pushEventMarker( DUSK_STRING( "Forward+ Light Pass" ) );

            // Clear render targets at the begining of the pass.
            constexpr f32 ClearValue[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            Image* Framebuffer[2] = { outputTarget, velocityTarget };
            FramebufferAttachment FramebufferAttachments[2] = { FramebufferAttachment( outputTarget ), FramebufferAttachment( velocityTarget ) };
            cmdList->clearRenderTargets( Framebuffer, 2u, ClearValue );

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
            const bool isInMaterialEdition = ( scenario == Material::RenderScenario::Default_Editor
                                            || scenario == Material::RenderScenario::Default_Picking_Editor );

            // Retrieve draw commands for the pass.
            const FrameGraphResources::DrawCmdBucket& bucket = resources->getDrawCmdBucket( DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT );

            PerPassData perPassData;
            perPassData.StartVector = bucket.instanceDataStartOffset;
			perPassData.VectorPerInstance = bucket.vectorPerInstance;
			perPassData.SunShadowMatrix = globalShadowMatrix;

            for ( const DrawCmd& cmd : bucket ) {
                const DrawCommandInfos& cmdInfos = cmd.infos;
                const Material* material = cmdInfos.material;

                // Upload vector buffer offset
				cmdList->updateBuffer( *perPassBuffer, &perPassData, sizeof( PerPassData ) );

                // Retrieve the PipelineState for the given RenderScenario.
                // TODO Cache the current PSO binded (if the PSO is the same; don't rebind anything).
                // TODO We need to make material mutable (since the scenario bind updates the streaming/caching).
                //      It simply require some refactoring at higher level (Mesh struct; gfx cache; etc.)
                const_cast<Material*>( material )->bindForScenario( scenario, cmdList, psoCache, samplerCount );
                
                // NOTE Since buffer registers are cached those calls have a low cost on the CPU side.
				cmdList->bindBuffer( InstanceVectorBufferHashcode, vectorBuffer );
                cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                if ( !material->skipLighting() ) {
                    cmdList->bindConstantBuffer( PerWorldBufferHashcode, perWorldBuffer );
                    cmdList->bindSampler( BRDFInputSamplerHashcode, materialSampler );
                }

                cmdList->bindImage( BrdfDfgLUTHascode, brdfDfgLut );
                cmdList->bindImage( IBLDiffuseHascode, iblDiffuse );
                cmdList->bindImage( IBLSpecularHascode, iblSpecular );
                cmdList->bindImage( CascadedShadowRenderModule::SliceImageHashcode, sliceShadow );

                if ( isInMaterialEdition ) {
                    cmdList->bindConstantBuffer( MaterialEditorBufferHashcode, materialEdBuffer );
                }

                if ( isPickingRequested ) {
                    cmdList->bindBuffer( PickingBufferHashcode, pickingBuffer );
                }

				cmdList->bindBuffer( CascadedShadowRenderModule::SliceBufferHashcode, sliceBuffer );

                // Re-setup the framebuffer (some permutations have a different framebuffer layout).
                cmdList->setupFramebuffer( FramebufferAttachments, FramebufferAttachment( zbufferTarget ) );
                cmdList->prepareAndBindResourceList();

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

            // TODO Might worth moving the copy call to a copy command queue?
			// (maybe have a separate pass dedicated to buffer readback)
			i32 frameIndex = cmdList->getFrameIndex();
            if ( pickingFrameIndex != ~0 && ( frameIndex - pickingFrameIndex ) > 3 ) {
                void* readBackData = cmdList->mapBuffer( *pickingReadbackBuffer );

                if ( readBackData != nullptr ) {
                    u8* dataPointer = reinterpret_cast< u8* >( readBackData );
                    pickedEntityId = *reinterpret_cast< u32* >( dataPointer + sizeof( u32 ) );
                    cmdList->unmapBuffer( *pickingReadbackBuffer );

                    isResultAvailable = true;
                    pickingFrameIndex = ~0;
                }
            }

			if ( isPickingRequested ) {
				cmdList->copyBuffer( pickingBuffer, pickingReadbackBuffer );
                pickingFrameIndex = frameIndex;
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

ResHandle_t WorldRenderModule::addDepthPrepass( FrameGraph& frameGraph )
{
    struct PassData {
        ResHandle_t DepthBuffer;
        ResHandle_t PerPassBuffer;
        ResHandle_t PerViewBuffer;
        ResHandle_t VectorDataBuffer;
    };

    PassData& data = frameGraph.addRenderPass<PassData>(
        "Depth PrePass",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            ImageDesc zBufferRenderTargetDesc;
            zBufferRenderTargetDesc.dimension = ImageDesc::DIMENSION_2D;
            zBufferRenderTargetDesc.format = eViewFormat::VIEW_FORMAT_R32_TYPELESS;
            zBufferRenderTargetDesc.usage = RESOURCE_USAGE_DEFAULT;
            zBufferRenderTargetDesc.bindFlags = RESOURCE_BIND_DEPTH_STENCIL | RESOURCE_BIND_SHADER_RESOURCE;
            zBufferRenderTargetDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_D32_FLOAT;

            passData.DepthBuffer = builder.allocateImage( zBufferRenderTargetDesc, FrameGraphBuilder::USE_PIPELINE_DIMENSIONS | FrameGraphBuilder::USE_PIPELINE_SAMPLER_COUNT );

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
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* zbufferTarget = resources->getImage( passData.DepthBuffer );

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );
            Buffer* vectorBuffer = resources->getBuffer( passData.VectorDataBuffer );

            cmdList->pushEventMarker( DUSK_STRING( "Depth PrePass" ) );

            // Clear render targets at the beginning of the pass.
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
            // TODO THIS IS DONE TWICE (Once at the zprepass and another time at light pass).
			const void* vectorBufferData = resources->getVectorBufferData();
			cmdList->updateBuffer( *vectorBuffer, vectorBufferData, sizeof( dkVec4f ) * MAX_VECTOR_PER_INSTANCE );

            const u32 samplerCount = camera->msaaSamplerCount;

            // Retrieve draw commands for the pass.
            const FrameGraphResources::DrawCmdBucket& bucket = resources->getDrawCmdBucket( DrawCommandKey::LAYER_DEPTH, DrawCommandKey::DEPTH_VIEWPORT_LAYER_DEFAULT );

            PerPassData perPassData;
            perPassData.StartVector = bucket.instanceDataStartOffset;
            perPassData.VectorPerInstance = bucket.vectorPerInstance;

            for ( const DrawCmd& cmd : bucket ) {
                const DrawCommandInfos& cmdInfos = cmd.infos;
                const Material* material = cmdInfos.material;

                // Upload vector buffer offset
				cmdList->updateBuffer( *perPassBuffer, &perPassData, sizeof( PerPassData ) );

                // Retrieve the PipelineState for the given RenderScenario.
                // TODO Cache the current PSO binded (if the PSO is the same; don't rebind anything).
                // TODO We need to make material mutable (since the scenario bind updates the streaming/caching).
                //      It simply require some refactoring at higher level (Mesh struct; gfx cache; etc.)
                const_cast<Material*>( material )->bindForScenario( Material::RenderScenario::Depth_Only, cmdList, psoCache, samplerCount );
                
                // NOTE Since buffer registers are cached those calls have a low cost on the CPU side.
				cmdList->bindBuffer( InstanceVectorBufferHashcode, vectorBuffer );
                cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                // Re-setup the framebuffer (some permutations have a different framebuffer layout).
                cmdList->setupFramebuffer( nullptr, FramebufferAttachment( zbufferTarget ) );
                cmdList->prepareAndBindResourceList();

                const Buffer* bufferList[1] = { 
                    cmdInfos.vertexBuffers[eMeshAttribute::Position]
                };

                // Bind vertex buffers
                cmdList->bindVertexBuffer( ( const Buffer** )bufferList, 1, 0);
                cmdList->bindIndiceBuffer( cmdInfos.indiceBuffer, !cmdInfos.useShortIndices );

				cmdList->drawIndexed( cmdInfos.indiceBufferCount, cmdInfos.instanceCount, cmdInfos.indiceBufferOffset );

				// Update vector buffer offset data
				perPassData.StartVector += ( cmdInfos.instanceCount * bucket.vectorPerInstance );
            }

            cmdList->popEventMarker();
        }
    );

    return data.DepthBuffer;
}

void WorldRenderModule::clearPickingBuffer( FrameGraph& frameGraph )
{
	struct PassDataDummy {};

	frameGraph.addRenderPass<PassDataDummy>(
		BuiltIn::ClearPickingBuffer_Name,
		[&]( FrameGraphBuilder& builder, PassDataDummy& passData ) {
		    builder.setUncullablePass();
		    builder.useAsyncCompute();
	    },
	    [=]( const PassDataDummy& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
		    cmdList->pushEventMarker( BuiltIn::ClearPickingBuffer_EventName );

		    PipelineState* pso = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, BuiltIn::ClearPickingBuffer_ShaderBinding );

		    cmdList->bindPipelineState( pso );
		    cmdList->bindBuffer( BuiltIn::ClearPickingBuffer_PickingBuffer_Hashcode, pickingBuffer );
		    cmdList->prepareAndBindResourceList();

		    cmdList->dispatchCompute( 1, 1, 1 );

		    cmdList->popEventMarker();
	    }
	);
}

void WorldRenderModule::setDefaultBrdfDfgLut( Image* brdfDfgLut )
{
    this->brdfDfgLut = brdfDfgLut;
}
