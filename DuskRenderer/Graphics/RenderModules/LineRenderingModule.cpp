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

//ResHandle_t LineRenderingModule::addLineRenderPass( RenderPipeline* renderPipeline, ResHandle_t output )
//{
//    struct PassData {
//        MutableResHandle_t  output;
//
//        ResHandle_t         screenBuffer;
//    };
//
//    PassData& data = renderPipeline->addRenderPass<PassData>(
//        "Line Rendering Pass",
//        [&]( RenderPipelineBuilder& renderPipelineBuilder, PassData& passData ) {
//        renderPipelineBuilder.setUncullablePass();
//
//            // Passthrough rendertarget
//            passData.output = renderPipelineBuilder.readRenderTarget( output );
//
//            BufferDesc screenBufferDesc;
//            screenBufferDesc.type = BufferDesc::CONSTANT_BUFFER;
//            screenBufferDesc.size = sizeof( nyaMat4x4f );
//
//            passData.screenBuffer = renderPipelineBuilder.allocateBuffer( screenBufferDesc, SHADER_STAGE_VERTEX );
//        },
//        [=]( const PassData& passData, const RenderPipelineResources& renderPipelineResources, RenderDevice* renderDevice ) {
//            // Render Pass
//            RenderTarget* outputTarget = renderPipelineResources.getRenderTarget( passData.output );
//
//            RenderPass renderPass;
//            renderPass.attachement[0] = { outputTarget, 0, 0 };
//
//            Buffer* screenBuffer = renderPipelineResources.getBuffer( passData.screenBuffer );
//
//            const CameraData* cameraData = renderPipelineResources.getMainCamera();
//
//            Viewport vp;
//            vp.X = 0;
//            vp.Y = 0;
//            vp.Width = static_cast<int>( cameraData->viewportSize.x );
//            vp.Height = static_cast<int>( cameraData->viewportSize.y );
//            vp.MinDepth = 0.0f;
//            vp.MaxDepth = 1.0f;
//
//            nyaMat4x4f orthoMatrix = nya::maths::MakeOrtho( 0.0f, cameraData->viewportSize.x, cameraData->viewportSize.y, 0.0f, -1.0f, 1.0f );
//
//            ResourceList resourceList;
//            resourceList.resource[0].buffer = screenBuffer;
//            renderDevice->updateResourceList( renderLinePso, resourceList );
//
//            CommandList& cmdList = renderDevice->allocateGraphicsCommandList();
//            {
//                cmdList.begin();
//
//                cmdList.setViewport( vp );
//
//                cmdList.updateBuffer( screenBuffer, &orthoMatrix, sizeof( nyaMat4x4f ) );
//                cmdList.updateBuffer( lineVertexBuffers[vertexBufferIndex], buffer, static_cast<size_t>( bufferIndex ) * sizeof( float ) );
//
//                // Pipeline State
//                cmdList.beginRenderPass( renderLinePso, renderPass );
//                {
//                    cmdList.bindVertexBuffer( lineVertexBuffers[vertexBufferIndex] );
//                    cmdList.bindIndiceBuffer( lineIndiceBuffer );
//
//                    cmdList.bindPipelineState( renderLinePso );
//
//                    cmdList.draw( static_cast< unsigned int >( indiceCount ) );
//                }
//                cmdList.endRenderPass();
//
//                cmdList.end();
//            }
//
//            renderDevice->submitCommandList( &cmdList );
//
//            // Swap buffers
//            vertexBufferIndex = ( vertexBufferIndex == 0 ) ? 1 : 0;
//
//            // Reset buffers
//            indiceCount = 0;
//            bufferIndex = 0;
//        } 
//    );
//
//    return data.output;
//}

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
    while ( !vertexBufferRenderingLock.compare_exchange_weak( expected, true, std::memory_order_acquire ) ) {
        expected = false;
    }
}

void LineRenderingModule::unlockVertexBuffer()
{
    vertexBufferLock.store( false, std::memory_order_release );
}

void LineRenderingModule::lockVertexBufferRendering()
{
    bool expected = false;
    while ( !vertexBufferLock.compare_exchange_weak( expected, true, std::memory_order_acquire ) ) {
        expected = false;
    }
}

void LineRenderingModule::unlockVertexBufferRendering()
{
    vertexBufferRenderingLock.store( false, std::memory_order_release );
}
