/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "WorldRenderer.h"

#include "PrimitiveCache.h"
#include "FrameGraph.h"
#include "GraphicsAssetCache.h"

#include <Framework/Cameras/Camera.h>
#include <Core/Allocators/LinearAllocator.h>

#include <Rendering/CommandList.h>

#include "LightGrid.h"
#include "EnvironmentProbeStreaming.h"

#include "RenderModules/AtmosphereRenderModule.h"
#include "RenderModules/PresentRenderPass.h"
#include "RenderModules/MSAAResolvePass.h"
#include "RenderModules/FrameCompositionModule.h"
#include "RenderModules/AutomaticExposure.h"
#include "RenderModules/TextRenderingModule.h"
#include "RenderModules/GlareRenderModule.h"
#include "RenderModules/LineRenderingModule.h"
#include "RenderModules/WorldRenderModule.h"
#include "RenderModules/IBLUtilitiesModule.h"
#include "RenderModules/CascadedShadowRenderModule.h"

DUSK_ENV_VAR( EnableTAA, false, bool ); // "Enable Temporal AntiAliasing [false/true]

static constexpr size_t MAX_DRAW_CMD_COUNT = 4096;

void RadixSort( DrawCmd* DUSK_RESTRICT _keys, DrawCmd* DUSK_RESTRICT _tempKeys, const size_t size )
{
    static constexpr size_t RADIXSORT_BITS = 11;
    static constexpr size_t RADIXSORT_HISTOGRAM_SIZE = ( 1 << RADIXSORT_BITS );
    static constexpr size_t RADIXSORT_BIT_MASK = ( RADIXSORT_HISTOGRAM_SIZE - 1 );

    DrawCmd * keys = _keys;
    DrawCmd * tempKeys = _tempKeys;

    uint32_t histogram[RADIXSORT_HISTOGRAM_SIZE];
    uint16_t shift = 0;
    uint32_t pass = 0;
    for ( ; pass < 6; ++pass ) {
        memset( histogram, 0, sizeof( uint32_t ) * RADIXSORT_HISTOGRAM_SIZE );

        bool sorted = true;
        {
            uint64_t key = keys[0].key.value;
            uint64_t prevKey = key;
            for ( uint32_t ii = 0; ii < size; ++ii, prevKey = key ) {
                key = keys[ii].key.value;

                uint16_t index = ( ( key >> shift ) & RADIXSORT_BIT_MASK );
                ++histogram[index];

                sorted &= ( prevKey <= key );
            }
        }

        if ( sorted ) {
            goto done;
        }

        uint32_t offset = 0;
        for ( uint32_t ii = 0; ii < RADIXSORT_HISTOGRAM_SIZE; ++ii ) {
            uint32_t count = histogram[ii];
            histogram[ii] = offset;

            offset += count;
        }

        for ( uint32_t ii = 0; ii < size; ++ii ) {
            uint64_t key = keys[ii].key.value;
            uint16_t index = ( ( key >> shift ) & RADIXSORT_BIT_MASK );
            uint32_t dest = histogram[index]++;

            tempKeys[dest] = keys[ii];
        }

        DrawCmd * swapKeys = tempKeys;
        tempKeys = keys;
        keys = swapKeys;

        shift += RADIXSORT_BITS;
    }

done:
    if ( ( pass & 1 ) != 0 ) {
        memcpy( _keys, _tempKeys, size * sizeof( DrawCmd ) );
    }
}

WorldRenderer::WorldRenderer( BaseAllocator* allocator )
    : automaticExposure( dk::core::allocate<AutomaticExposureModule>( allocator ) )
    , glareRendering( dk::core::allocate<GlareRenderModule>( allocator ) )
    , frameComposition( dk::core::allocate<FrameCompositionModule>( allocator ) )
    , atmosphereRendering( dk::core::allocate<AtmosphereRenderModule>( allocator ) )
    , WorldRendering( dk::core::allocate<WorldRenderModule>( allocator ) )
    , memoryAllocator( allocator )
    , primitiveCache( dk::core::allocate<PrimitiveCache>( allocator ) )
    , drawCmdAllocator( dk::core::allocate<LinearAllocator>( allocator, sizeof( DrawCmd ) * MAX_DRAW_CMD_COUNT, allocator->allocate( sizeof( DrawCmd ) * MAX_DRAW_CMD_COUNT ) ) )
	, gpuShadowCullAllocator( dk::core::allocate<LinearAllocator>( allocator, sizeof( GPUShadowDrawCmd )* MAX_DRAW_CMD_COUNT, allocator->allocate( sizeof( GPUShadowDrawCmd )* MAX_DRAW_CMD_COUNT ) ) )
    , frameGraph( nullptr )
    , frameDrawCmds( dk::core::allocateArray<DrawCmd>( allocator, sizeof( DrawCmd )* MAX_DRAW_CMD_COUNT ) )
    , needResourcePrecompute( true )
    , wireframeMaterial( nullptr )
    , brdfDfgLut( nullptr )
    , lightGrid( dk::core::allocate<LightGrid>( allocator, allocator ) )
    , resolvedDepth( 0u )
    , environmentProbeStreaming( dk::core::allocate<EnvironmentProbeStreaming>( allocator, allocator ) )
    , cascadedShadowMapRendering( dk::core::allocate<CascadedShadowRenderModule>( allocator, allocator ) )
{

}

WorldRenderer::~WorldRenderer()
{
    dk::core::free( memoryAllocator, automaticExposure );
    dk::core::free( memoryAllocator, glareRendering );
    dk::core::free( memoryAllocator, frameComposition );
	dk::core::free( memoryAllocator, atmosphereRendering );
    dk::core::free( memoryAllocator, WorldRendering );
    dk::core::free( memoryAllocator, primitiveCache );
	dk::core::free( memoryAllocator, drawCmdAllocator );
	dk::core::free( memoryAllocator, gpuShadowCullAllocator );
    dk::core::free( memoryAllocator, frameGraph );
    dk::core::free( memoryAllocator, frameDrawCmds );
    dk::core::free( memoryAllocator, lightGrid );
    dk::core::free( memoryAllocator, environmentProbeStreaming );
    dk::core::free( memoryAllocator, cascadedShadowMapRendering );
    
    memoryAllocator = nullptr;
}

void WorldRenderer::destroy( RenderDevice* renderDevice )
{
    // Wait for FrameGraph completion.
    frameGraph->waitPendingFrameCompletion();

    primitiveCache->destroy( renderDevice );
    frameGraph->destroy( renderDevice );

    if ( brdfDfgLut != nullptr ) {
        renderDevice->destroyImage( brdfDfgLut );
        brdfDfgLut = nullptr;
    }

    environmentProbeStreaming->destroyResources( *renderDevice );
    cascadedShadowMapRendering->destroy( *renderDevice );

    automaticExposure->destroy( *renderDevice );
    glareRendering->destroy( *renderDevice );
    atmosphereRendering->destroy( *renderDevice );
    WorldRendering->destroy( *renderDevice );
}

void WorldRenderer::loadCachedResources( RenderDevice* renderDevice, ShaderCache* shaderCache, GraphicsAssetCache* graphicsAssetCache, VirtualFileSystem* virtualFileSystem )
{
    frameGraph = dk::core::allocate<FrameGraph>( memoryAllocator, memoryAllocator, renderDevice, virtualFileSystem );

    primitiveCache->createCachedGeometry( renderDevice );
    
    automaticExposure->loadCachedResources( *renderDevice );
    glareRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );
    frameComposition->loadCachedResources( *graphicsAssetCache );
    atmosphereRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );
    WorldRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );

    environmentProbeStreaming->createResources( *renderDevice );
    cascadedShadowMapRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );

    // Debug resources.
    wireframeMaterial = graphicsAssetCache->getMaterial( DUSK_STRING( "GameData/materials/wireframe.mat" ), true );

    // Create BRDF DFG LUT (TODO allow offline compute to avoid recomputing it each launch).
    ImageDesc brdfLUTDesc;
    brdfLUTDesc.dimension = ImageDesc::DIMENSION_2D;
    brdfLUTDesc.format = eViewFormat::VIEW_FORMAT_R16G16_FLOAT;
    brdfLUTDesc.width = BRDF_LUT_SIZE;
    brdfLUTDesc.height = BRDF_LUT_SIZE;
    brdfLUTDesc.bindFlags = eResourceBind::RESOURCE_BIND_UNORDERED_ACCESS_VIEW | eResourceBind::RESOURCE_BIND_SHADER_RESOURCE;
    brdfLUTDesc.usage = eResourceUsage::RESOURCE_USAGE_DEFAULT;

    brdfDfgLut = renderDevice->createImage( brdfLUTDesc );

#ifdef DUSK_DEVBUILD
    renderDevice->setDebugMarker( *brdfDfgLut, DUSK_STRING( "BRDF LUT (Runtime Computed)" ) );
#endif

    // Precompute resources (might worth being done offline?).
    FrameGraph& graph = *frameGraph;
    graph.waitPendingFrameCompletion();
    
    glareRendering->precomputePipelineResources( graph );
    atmosphereRendering->triggerLutRecompute();
    AddBrdfDfgLutComputePass( graph, brdfDfgLut );

    // Execute precompute step.
    graph.execute( renderDevice, 0.0f );

    // TODO This is shit and there is probably 
    WorldRendering->setDefaultBrdfDfgLut( brdfDfgLut );
}

void WorldRenderer::drawDebugSphere( CommandList& cmdList )
{
    const PrimitiveCache::Entry& sphere = primitiveCache->getSphereEntry();

    cmdList.bindVertexBuffer( (const Buffer**)sphere.vertexBuffers, 3 );
    cmdList.bindIndiceBuffer( sphere.indiceBuffer );
    cmdList.drawIndexed( sphere.indiceCount, 1 );
}

void WorldRenderer::drawWorld( RenderDevice* renderDevice, const f32 deltaTime )
{
    // Sort this frame draw commands.
    DrawCmd* drawCmds = static_cast< DrawCmd* >( drawCmdAllocator->getBaseAddress() );
    const size_t drawCmdCount = drawCmdAllocator->getAllocationCount();

    RadixSort( drawCmds, frameDrawCmds, drawCmdCount );

    // Submit commands to each render queue.
    frameGraph->submitAndDispatchDrawCmds( drawCmds, drawCmdCount );

    // Execute current frame graph.
    frameGraph->execute( renderDevice, deltaTime );

    // Reset DrawCmd Pool.
    drawCmdAllocator->clear();
    gpuShadowCullAllocator->clear();
}

DrawCmd& WorldRenderer::allocateDrawCmd()
{
    return *dk::core::allocate<DrawCmd>( drawCmdAllocator );
}

DrawCmd& WorldRenderer::allocateSpherePrimitiveDrawCmd()
{
    DrawCmd& cmd = allocateDrawCmd();

    const PrimitiveCache::Entry& sphere = primitiveCache->getSphereEntry();

    DrawCommandInfos& infos = cmd.infos;
    infos.vertexBuffers = (const Buffer**)sphere.vertexBuffers;
    infos.indiceBuffer = sphere.indiceBuffer;
    infos.indiceBufferOffset = sphere.indiceBufferOffset;
    infos.indiceBufferCount = sphere.indiceCount;
    infos.vertexBufferCount = 3;
    infos.alphaDitheringValue = 1.0f;
    infos.useShortIndices = true;

    return cmd;
}

GPUShadowDrawCmd& WorldRenderer::allocateGPUShadowCullDrawCmd()
{
	return *dk::core::allocate<GPUShadowDrawCmd>( gpuShadowCullAllocator );
}

FrameGraph& WorldRenderer::prepareFrameGraph( const Viewport& viewport, const ScissorRegion& scissor, const CameraData* camera /*= nullptr */ )
{
    FrameGraph& graph = *frameGraph;
    graph.waitPendingFrameCompletion();
    graph.setViewport( viewport, scissor, camera );

    if ( camera != nullptr ) {
        graph.setImageQuality( camera->imageQuality );
        graph.setMSAAQuality( camera->msaaSamplerCount );
    }

    return graph;
}

const Material* WorldRenderer::getWireframeMaterial() const
{
    return wireframeMaterial;
}

ResHandle_t WorldRenderer::buildDefaultGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize, RenderWorld* renderWorld )
{
    f32 imageQuality = frameGraph.getImageQuality();
    u32 msaaSamplerCount = frameGraph.getMSAASamplerCount();

    automaticExposure->importResourcesToGraph( frameGraph );

    environmentProbeStreaming->updateProbeCapture( frameGraph, atmosphereRendering );

    // Update clusters.
    LightGrid::Data lightGridData = lightGrid->updateClusters( frameGraph );

    ResHandle_t prepassTarget = WorldRendering->addDepthPrepass( frameGraph );

    resolvedDepth = AddMSAADepthResolveRenderPass( frameGraph, prepassTarget, msaaSamplerCount );

    // Rescale the main render target for post-fx (if SSAA is used to down/upscale).
    if ( imageQuality != 1.0f ) {
        resolvedDepth = AddSSAAResolveRenderPass( frameGraph, resolvedDepth, true );
    }

    cascadedShadowMapRendering->submitGPUShadowCullCmds( static_cast<GPUShadowDrawCmd*>( gpuShadowCullAllocator->getBaseAddress() ), gpuShadowCullAllocator->getAllocationCount() );

    cascadedShadowMapRendering->captureShadowMap( frameGraph, resolvedDepth, viewportSize, *lightGrid->getDirectionalLightData(), renderWorld );

    // LightPass.
    WorldRenderModule::LightPassOutput primRenderPass = WorldRendering->addPrimitiveLightPass( frameGraph, lightGridData.PerSceneBuffer, prepassTarget, scenario, environmentProbeStreaming->getReadDistantProbeIrradiance(), environmentProbeStreaming->getReadDistantProbeRadiance(), cascadedShadowMapRendering->getGlobalShadowMatrix() );
    
    primRenderPass.OutputRenderTarget = atmosphereRendering->renderAtmosphere( frameGraph, primRenderPass.OutputRenderTarget, primRenderPass.OutputDepthTarget );

    // AntiAliasing resolve. (we merge both TAA and MSAA in a single pass to avoid multiple dispatch).
    ResHandle_t resolvedColor = primRenderPass.OutputRenderTarget;
    if ( msaaSamplerCount > 1 || EnableTAA ) {
        resolvedColor = AddMSAAResolveRenderPass( frameGraph, primRenderPass.OutputRenderTarget, primRenderPass.OutputVelocityTarget, primRenderPass.OutputDepthTarget, msaaSamplerCount, EnableTAA );
    }

    if ( EnableTAA ) {
        frameGraph.saveLastFrameRenderTarget( resolvedColor );
    }

    // Rescale the main render target for post-fx (if SSAA is used to down/upscale).
    if ( imageQuality != 1.0f ) {
        resolvedColor = AddSSAAResolveRenderPass( frameGraph, resolvedColor, false );
    }

    // Atmosphere Rendering.
    ResHandle_t presentRt = resolvedColor;

    // Glare Rendering.
    FFTPassOutput frequencyDomainRt = AddFFTComputePass( frameGraph, presentRt, viewportSize.x, viewportSize.y );
    FFTPassOutput convolutedFFT = glareRendering->addGlareComputePass( frameGraph, frequencyDomainRt );
    ResHandle_t inverseFFT = AddInverseFFTComputePass( frameGraph, convolutedFFT, viewportSize.x, viewportSize.y );

    // Automatic Exposure.
    automaticExposure->computeExposure( frameGraph, presentRt, dkVec2u( static_cast< u32 >( viewportSize.x ), static_cast< u32 >( viewportSize.y ) ) );

    // Frame composition.
    ResHandle_t composedFrame = frameComposition->addFrameCompositionPass( frameGraph, presentRt, inverseFFT );

    return composedFrame;
}

ResHandle_t WorldRenderer::getResolvedDepth()
{
    return resolvedDepth;
}
