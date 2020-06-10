/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "LineRenderingModule.h"

#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>
#include <Graphics/ShaderCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "Generated/HUD.generated.h"

static constexpr i32 LINE_RENDERING_MAX_LINE_COUNT = 32000;

LineRenderingModule::LineRenderingModule( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , indiceCount( 0 )
    , vertexBufferWriteIndex( 0 )
    , bufferIndex( 0 )
    , lineVertexBuffers{ nullptr, nullptr }
    , lineIndiceBuffer( nullptr )
    , buffer( nullptr )
{
    vertexBufferLock.store( false );
    vertexBufferRenderingLock.store( false );

    buffer = dk::core::allocateArray<f32>( memoryAllocator, LINE_RENDERING_MAX_LINE_COUNT );
    memset( buffer, 0, sizeof( f32 ) * LINE_RENDERING_MAX_LINE_COUNT );
}

LineRenderingModule::~LineRenderingModule()
{
    indiceCount = 0;
    vertexBufferWriteIndex = 0;  
    bufferIndex = 0;

    dk::core::freeArray( memoryAllocator, buffer );
    memoryAllocator = nullptr;
}

void LineRenderingModule::destroy( RenderDevice& renderDevice )
{
    for ( i32 i = 0; i < BUFFER_COUNT; i++ ) {
        renderDevice.destroyBuffer( lineVertexBuffers[i] );
        lineVertexBuffers[i] = nullptr;
    }

    renderDevice.destroyBuffer( lineIndiceBuffer );
    lineIndiceBuffer = nullptr;
}

ResHandle_t LineRenderingModule::renderLines( FrameGraph& frameGraph, ResHandle_t output )
{
    struct PassData {
        ResHandle_t Output;
        ResHandle_t PerViewBuffer;
    };

    constexpr PipelineStateDesc PipelineStateDefault = PipelineStateDesc(
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_LINELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc(),
        BlendStateDesc(
            true, true, false,
            false, false, false, false,
            { eBlendSource::BLEND_SOURCE_SRC_ALPHA, eBlendSource::BLEND_SOURCE_INV_SRC_ALPHA, eBlendOperation::BLEND_OPERATION_ADD },
            { eBlendSource::BLEND_SOURCE_INV_DEST_ALPHA, eBlendSource::BLEND_SOURCE_ONE, eBlendOperation::BLEND_OPERATION_ADD }
        ),
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
        { { RenderingHelpers::S_BilinearClampEdge }, 1 },
        { {
            { 0, VIEW_FORMAT_R32G32B32A32_FLOAT, 0, 0, 0, true, "POSITION" },
            { 0, VIEW_FORMAT_R32G32B32A32_FLOAT, 0, 0, 0, true, "COLOR" }
        } }
    );

    PassData& data = frameGraph.addRenderPass<PassData>(
        HUD::LineRendering_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
            passData.Output = builder.readImage( output );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            lockVertexBufferRendering();

            Image* outputTarget = resources->getImage( passData.Output );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( PipelineStateDefault, HUD::LineRendering_ShaderBinding );

            const Viewport* pipelineDimensions = resources->getMainViewport();
            const ScissorRegion* pipelineScissor = resources->getMainScissorRegion();

            cmdList->pushEventMarker( HUD::LineRendering_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->setViewport( *pipelineDimensions );
            cmdList->setScissor( *pipelineScissor );

            cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
            cmdList->updateBuffer( *lineVertexBuffers[vertexBufferWriteIndex], buffer, static_cast< size_t >( bufferIndex ) * sizeof( f32 ) );

            const Buffer* VertexBuffer[1] = { lineVertexBuffers[vertexBufferWriteIndex] };
            cmdList->bindVertexBuffer( VertexBuffer );

            cmdList->prepareAndBindResourceList( pipelineState );

            cmdList->setupFramebuffer( &outputTarget, nullptr );

            cmdList->draw( static_cast<u32>( indiceCount ), 1u );

            cmdList->popEventMarker();

            // Swap buffers
            vertexBufferWriteIndex = ( vertexBufferWriteIndex == 0 ) ? 1 : 0;
            
            // Reset buffers
            indiceCount = 0;
            bufferIndex = 0;

            unlockVertexBufferRendering();
        } 
    );

    return data.Output;
}

void LineRenderingModule::addLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness )
{
    lockVertexBuffer();

    if ( ( bufferIndex + 16 ) >= LINE_RENDERING_MAX_LINE_COUNT ) {
        return;
    }

    buffer[bufferIndex++] = ( p0.x );
    buffer[bufferIndex++] = ( p0.y );
    buffer[bufferIndex++] = ( p0.z );
    buffer[bufferIndex++] = ( lineThickness );

    buffer[bufferIndex++] = ( p0Color.x );
    buffer[bufferIndex++] = ( p0Color.y );
    buffer[bufferIndex++] = ( p0Color.z );
    buffer[bufferIndex++] = ( p0Color.w );

    buffer[bufferIndex++] = ( p1.x );
    buffer[bufferIndex++] = ( p1.y );
    buffer[bufferIndex++] = ( p1.z );
    buffer[bufferIndex++] = ( lineThickness );

    buffer[bufferIndex++] = ( p1Color.x );
    buffer[bufferIndex++] = ( p1Color.y );
    buffer[bufferIndex++] = ( p1Color.z );
    buffer[bufferIndex++] = ( p1Color.w );

    indiceCount += 2;

    unlockVertexBuffer();
}


void LineRenderingModule::createPersistentResources( RenderDevice& renderDevice )
{
    // Create static indice buffer
    constexpr i32 IndexStride = sizeof( u32 );
    constexpr i32 IndiceCountPerPrimitive = 2;

    // Indices per line (2 triangles; 3 indices per triangle)
    constexpr i32 IndexBufferLength = LINE_RENDERING_MAX_LINE_COUNT * IndiceCountPerPrimitive;
    constexpr i32 IndexBufferSize = IndexBufferLength * IndexStride;

    // TODO This sucks (we trash the memory and create fragmentation if allocating from the global application
    // allocator). Create a module-local allocator to avoid this.
    u32* indexBufferData = dk::core::allocateArray<u32>( memoryAllocator, IndexBufferLength );

    DUSK_RAISE_FATAL_ERROR( indexBufferData, "Memory allocation failed! (the application ran out of memory)" );

    for ( u32 c = 0; c < LINE_RENDERING_MAX_LINE_COUNT; c++ ) {
        indexBufferData[c * 2] = c;
        indexBufferData[( c * 2 ) + 1] = ( c + 1 );
    }

    BufferDesc indiceBufferDescription;
    indiceBufferDescription.BindFlags = RESOURCE_BIND_INDICE_BUFFER;
    indiceBufferDescription.SizeInBytes = IndexBufferSize;
    indiceBufferDescription.Usage = RESOURCE_USAGE_STATIC;
    indiceBufferDescription.StrideInBytes = IndexStride;

    lineIndiceBuffer = renderDevice.createBuffer( indiceBufferDescription, indexBufferData );

    dk::core::freeArray( memoryAllocator, indexBufferData );

    // Create dynamic vertex buffer.
    constexpr u32 VertexSize = static_cast<u32>( sizeof( f32 ) * 8 );

    // XYZW Position + RGBA Color (w/ 2 vertices per line)
    constexpr u32 VertexBufferSize = static_cast< u32 >( VertexSize * LINE_RENDERING_MAX_LINE_COUNT * 2 );

    BufferDesc bufferDescription;
    bufferDescription.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    bufferDescription.SizeInBytes = VertexBufferSize;
    bufferDescription.StrideInBytes = VertexSize;
    bufferDescription.Usage = RESOURCE_USAGE_DYNAMIC;

    for ( i32 i = 0; i < BUFFER_COUNT; i++ ) {
        lineVertexBuffers[i] = renderDevice.createBuffer( bufferDescription );
    }

}

void LineRenderingModule::lockVertexBuffer()
{
    bool expected = false;
    while ( !vertexBufferRenderingLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    vertexBufferLock.store( true, std::memory_order_acquire );
}

void LineRenderingModule::unlockVertexBuffer()
{
    vertexBufferLock.store( false, std::memory_order_release );
}

void LineRenderingModule::lockVertexBufferRendering()
{
    bool expected = false;
    while ( !vertexBufferLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    vertexBufferRenderingLock.store( true, std::memory_order_acquire );
}

void LineRenderingModule::unlockVertexBufferRendering()
{
    vertexBufferRenderingLock.store( false, std::memory_order_release );
}

