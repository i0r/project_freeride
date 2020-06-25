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

#include <Graphics/RenderPasses/Headers/Light.h>

LineRenderingModule::LineRenderingModule( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , linePointsConstantBuffer( nullptr )
    , linePointsToRender( nullptr )
    , lineCount( 0 )
    , cbufferLock( false )
    , cbufferRenderingLock( false )
{
    linePointsToRender = dk::core::allocateArray<LineInfos>( memoryAllocator, LINE_RENDERING_MAX_LINE_COUNT );
    memset( linePointsToRender, 0, sizeof( LineInfos ) * LINE_RENDERING_MAX_LINE_COUNT );
}

LineRenderingModule::~LineRenderingModule()
{
    lineCount = 0;

    dk::core::freeArray( memoryAllocator, linePointsToRender );
    memoryAllocator = nullptr;
}

void LineRenderingModule::destroy( RenderDevice& renderDevice )
{
    renderDevice.destroyBuffer( linePointsConstantBuffer );
    linePointsConstantBuffer = nullptr;
}

ResHandle_t LineRenderingModule::renderLines( FrameGraph& frameGraph, ResHandle_t output )
{
    struct PassData
    {
        ResHandle_t Output;
        ResHandle_t PerViewBuffer;
    };

    constexpr PipelineStateDesc PipelineStateDefault = PipelineStateDesc(
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc(),
        BlendStateDesc(),
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, VIEW_FORMAT_R16G16B16A16_FLOAT ) )
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

        cmdList->pushEventMarker( HUD::LineRendering_EventName );

        cmdList->bindPipelineState( pipelineState );

        cmdList->setViewport( *resources->getMainViewport() );
        cmdList->setScissor( *resources->getMainScissorRegion() );

        cmdList->updateBuffer( *linePointsConstantBuffer, linePointsToRender, static_cast< size_t >( lineCount ) * sizeof( LineInfos ) );

        cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
        cmdList->bindConstantBuffer( PerPassBufferHashcode, linePointsConstantBuffer );

        cmdList->prepareAndBindResourceList( pipelineState );

        cmdList->setupFramebuffer( &outputTarget, nullptr );

        cmdList->draw( 6u, lineCount );

        cmdList->popEventMarker();

        // Reset buffers
        lineCount = 0;

        unlockVertexBufferRendering();
    }
    );

    return data.Output;
}

void LineRenderingModule::addLine( const dkVec3f& p0, const dkVec4f& p0Color, const dkVec3f& p1, const dkVec4f& p1Color, const f32 lineThickness )
{
    lockVertexBuffer();

    if ( lineCount >= LINE_RENDERING_MAX_LINE_COUNT ) {
        unlockVertexBuffer();
        return;
    }

    LineInfos& infos = linePointsToRender[lineCount];
    infos.P0 = p0;
    infos.P1 = p1;
    infos.Color = p0Color;
    infos.Width = lineThickness;

    lineCount++;

    unlockVertexBuffer();
}

void LineRenderingModule::createPersistentResources( RenderDevice& renderDevice )
{
    constexpr i32 BufferSize = LINE_RENDERING_MAX_LINE_COUNT * sizeof( LineInfos );

    BufferDesc linePointsBufferDesc;
    linePointsBufferDesc.SizeInBytes = BufferSize;
    linePointsBufferDesc.StrideInBytes = 0;
    linePointsBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
    linePointsBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;

    linePointsConstantBuffer = renderDevice.createBuffer( linePointsBufferDesc );
}

void LineRenderingModule::lockVertexBuffer()
{
    bool expected = false;
    while ( !cbufferRenderingLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    cbufferLock.store( true );
}

void LineRenderingModule::unlockVertexBuffer()
{
    cbufferLock.store( false, std::memory_order_release );
}

void LineRenderingModule::lockVertexBufferRendering()
{
    bool expected = false;
    while ( !cbufferLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    cbufferRenderingLock.store( true );
}

void LineRenderingModule::unlockVertexBufferRendering()
{
    cbufferRenderingLock.store( false, std::memory_order_release );
}
