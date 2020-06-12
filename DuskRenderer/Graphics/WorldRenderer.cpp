/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "WorldRenderer.h"

#include "PrimitiveCache.h"
#include "FrameGraph.h"

#include <Framework/Cameras/Camera.h>
#include <Core/Allocators/LinearAllocator.h>

#include <Rendering/CommandList.h>

#include "RenderModules/BrunetonSkyModel.h"
#include "RenderModules/PresentRenderPass.h"
#include "RenderModules/MSAAResolvePass.h"
#include "RenderModules/FinalPostFxRenderPass.h"
#include "RenderModules/AutomaticExposure.h"
#include "RenderModules/TextRenderingModule.h"
#include "RenderModules/GlareRenderModule.h"
#include "RenderModules/LineRenderingModule.h"
#include "RenderModules/FinalPostFxRenderPass.h"

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
    : BrunetonSky( dk::core::allocate<BrunetonSkyRenderModule>( allocator ) )
    , AutomaticExposure( dk::core::allocate<AutomaticExposureModule>( allocator ) )
    , TextRendering( dk::core::allocate<TextRenderingModule>( allocator ) )
    , GlareRendering( dk::core::allocate<GlareRenderModule>( allocator ) )
    , LineRendering( dk::core::allocate<LineRenderingModule>( allocator, allocator ) )
    , FrameComposition( dk::core::allocate<FrameCompositionModule>( allocator ) )
    , memoryAllocator( allocator )
    , primitiveCache( dk::core::allocate<PrimitiveCache>( allocator ) )
    , drawCmdAllocator( dk::core::allocate<LinearAllocator>( allocator, sizeof( DrawCmd )* MAX_DRAW_CMD_COUNT, allocator->allocate( sizeof( DrawCmd ) * MAX_DRAW_CMD_COUNT ) ) )
    , frameGraph( nullptr )
    , needResourcePrecompute( true )
{

}

WorldRenderer::~WorldRenderer()
{
    dk::core::free( memoryAllocator, BrunetonSky );
    dk::core::free( memoryAllocator, AutomaticExposure );
    dk::core::free( memoryAllocator, TextRendering );
    dk::core::free( memoryAllocator, GlareRendering );
    dk::core::free( memoryAllocator, LineRendering );
    dk::core::free( memoryAllocator, primitiveCache );
    dk::core::free( memoryAllocator, drawCmdAllocator );
    dk::core::free( memoryAllocator, frameGraph );

    memoryAllocator = nullptr;
}

void WorldRenderer::destroy( RenderDevice* renderDevice )
{
    primitiveCache->destroy( renderDevice );
    frameGraph->destroy( renderDevice );

    BrunetonSky->destroy( *renderDevice );
    AutomaticExposure->destroy( *renderDevice );
    TextRendering->destroy( *renderDevice );
    GlareRendering->destroy( *renderDevice );
    LineRendering->destroy( *renderDevice );
}

void WorldRenderer::loadCachedResources( RenderDevice* renderDevice, ShaderCache* shaderCache, GraphicsAssetCache* graphicsAssetCache, VirtualFileSystem* virtualFileSystem )
{
    frameGraph = dk::core::allocate<FrameGraph>( memoryAllocator, memoryAllocator, renderDevice, virtualFileSystem );

    primitiveCache->createCachedGeometry( renderDevice );
    
    // TODO Deprecate old Bruneton sky module so that we can get rid of the global shader cache dependency...
    BrunetonSky->loadCachedResources( *renderDevice, *shaderCache, *graphicsAssetCache );

    AutomaticExposure->loadCachedResources( *renderDevice );
    TextRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );
    GlareRendering->loadCachedResources( *renderDevice, *graphicsAssetCache );
    LineRendering->createPersistentResources( *renderDevice );
    FrameComposition->loadCachedResources( *graphicsAssetCache );

    // Precompute resources (might worth being done offline?).
    FrameGraph& graph = *frameGraph;
    graph.waitPendingFrameCompletion();
    
    GlareRendering->precomputePipelineResources( graph );

    // Execute precompute step.
    graph.execute( renderDevice, 0.0f );
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
    /* DrawCmd* drawCmds = static_cast< DrawCmd* >( drawCmdAllocator->getBaseAddress() );
     const size_t drawCmdCount = drawCmdAllocator->getAllocationCount();

     DrawCmd tmpDrawCmds[MAX_DRAW_CMD_COUNT];
     RadixSort( drawCmds, tmpDrawCmds, drawCmdCount );

     if ( frameGraphCount > 1 ) {
         for ( size_t i = 0; i < drawCmdCount; i++ ) {

         }
     }*/

    // Execute current frame graph
    frameGraph->execute( renderDevice, deltaTime );

    // Reset DrawCmd Pool
    drawCmdAllocator->clear();
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

    return cmd;
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
