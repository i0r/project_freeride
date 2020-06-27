/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "TextRenderingModule.h"

#include <Io/FontDescriptor.h>

#include <Graphics/FrameGraph.h>
#include <Graphics/GraphicsAssetCache.h>
#include <Graphics/ShaderCache.h>

#include <Core/ViewFormat.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "Generated/HUD.generated.h"

TextRenderingModule::TextRenderingModule()
    : fontAtlas( nullptr )
    , fontDescriptor( nullptr )
    , indiceCount( 0 )
    , vertexBufferIndex( 0 )
    , glyphVertexBuffers{ nullptr, nullptr }
    , glyphIndiceBuffer( nullptr )
    , bufferOffset( 0 )
{
    memset( buffer, 0, sizeof( f32 ) * TEXT_RENDERING_MAX_GLYPH_COUNT );

    vertexBufferLock.store( false );
    vertexBufferRenderingLock.store( false );
}

TextRenderingModule::~TextRenderingModule()
{
    fontAtlas = nullptr;
    fontDescriptor = nullptr;

    indiceCount = 0;
    vertexBufferIndex = 0;

    bufferOffset = 0;
}

void TextRenderingModule::destroy( RenderDevice& renderDevice )
{
    for ( i32 i = 0; i < BUFFER_COUNT; i++ ) {
        renderDevice.destroyBuffer( glyphVertexBuffers[i] );
    }

    renderDevice.destroyBuffer( glyphIndiceBuffer );
}

ResHandle_t TextRenderingModule::renderText( FrameGraph& frameGraph, ResHandle_t output )
{
    struct PassData {
        ResHandle_t Output;
        ResHandle_t PerViewBuffer;
    };

    constexpr PipelineStateDesc PipelineStateDefault = PipelineStateDesc( 
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
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
            { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 0,  0, true, "TEXCOORD" },
            { 0, VIEW_FORMAT_R32G32B32A32_FLOAT, 0, 0, 0, true, "COLOR" }
        } }
    );

    PassData& data = frameGraph.addRenderPass<PassData>(
        HUD::RenderText_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            passData.PerViewBuffer = builder.retrievePerViewBuffer();
            passData.Output = builder.readImage( output );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            lockVertexBufferRendering();
            // Render Pass
            Image* outputTarget = resources->getImage( passData.Output );
            Buffer* perViewBuffer = resources->getPersistentBuffer( passData.PerViewBuffer );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( PipelineStateDefault, HUD::RenderText_ShaderBinding );

            const Viewport* pipelineDimensions = resources->getMainViewport();
            const ScissorRegion* pipelineScissor = resources->getMainScissorRegion();

            cmdList->pushEventMarker( HUD::RenderText_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->setViewport( *pipelineDimensions );
            cmdList->setScissor( *pipelineScissor );

            cmdList->bindConstantBuffer( PerViewBufferHashcode, perViewBuffer );
            cmdList->bindImage( HUD::RenderText_FontAtlasTexture_Hashcode, fontAtlas );
            cmdList->prepareAndBindResourceList();

            cmdList->updateBuffer( *glyphVertexBuffers[vertexBufferIndex], buffer, static_cast< size_t >( bufferOffset ) * sizeof( f32 ) );

            // Swap buffers
            vertexBufferIndex = ( vertexBufferIndex == 0 ) ? 1 : 0;

            const Buffer* vertexBuffer[1] = { glyphVertexBuffers[vertexBufferIndex] };
            cmdList->bindVertexBuffer( vertexBuffer );
            cmdList->bindIndiceBuffer( glyphIndiceBuffer, true );

            cmdList->setupFramebuffer( &outputTarget, nullptr );
            cmdList->drawIndexed( static_cast< u32 >( indiceCount ), 1u );

            cmdList->popEventMarker();

            // Reset buffers
            indiceCount = 0;
            bufferOffset = 0;

            unlockVertexBufferRendering();
        } 
    );

    return data.Output;
}

void TextRenderingModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    // Load Default Font
    fontDescriptor = graphicsAssetCache.getFont( DUSK_STRING( "GameData/fonts/SegoeUI.fnt" ) );
    DUSK_RAISE_FATAL_ERROR( fontDescriptor != nullptr, "Could not load default font!" );

    fontAtlas = graphicsAssetCache.getImage( fontDescriptor->Name.c_str() );

    // Create static indice buffer
    static constexpr i32 IndexStride = sizeof( u32 );

    // Indices per glyph (2 triangles; 3 indices per triangle)
    static constexpr i32 indexBufferSize = MaxCharactersPerLine * MaxCharactersLines * 6 * sizeof( u32 );
    static constexpr i32 indexBufferLength = indexBufferSize / IndexStride;

    u32 indexBufferData[indexBufferLength];

    u32 i = 0;
    for ( u32 c = 0; c < MaxCharactersPerLine * MaxCharactersLines; c++ ) {
        indexBufferData[i + 0] = c * 4 + 0;
        indexBufferData[i + 1] = c * 4 + 1;
        indexBufferData[i + 2] = c * 4 + 2;

        indexBufferData[i + 3] = c * 4 + 0;
        indexBufferData[i + 4] = c * 4 + 2;
        indexBufferData[i + 5] = c * 4 + 3;

        i += 6;
    }

    BufferDesc indiceBufferDescription;
    indiceBufferDescription.Usage = RESOURCE_USAGE_STATIC;
    indiceBufferDescription.BindFlags = RESOURCE_BIND_INDICE_BUFFER;
    indiceBufferDescription.SizeInBytes = indexBufferSize;

    glyphIndiceBuffer = renderDevice.createBuffer( indiceBufferDescription, indexBufferData );

    // Create dynamic vertex buffer
    // 4 + 2 + 4
    constexpr std::size_t SINGLE_VERTEX_SIZE = sizeof( f32 ) * 10;

    // XYZ Position + RGBA Color + XY TexCoords (w/ 6 vertices per glyph)
    constexpr std::size_t VERTEX_BUFFER_SIZE = SINGLE_VERTEX_SIZE * TEXT_RENDERING_MAX_GLYPH_COUNT;

    BufferDesc bufferDescription;
    bufferDescription.Usage = RESOURCE_USAGE_DYNAMIC;
    bufferDescription.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    bufferDescription.SizeInBytes = VERTEX_BUFFER_SIZE;
    bufferDescription.StrideInBytes = SINGLE_VERTEX_SIZE;

    for ( i32 i = 0; i < BUFFER_COUNT; i++ ) {
        glyphVertexBuffers[i] = renderDevice.createBuffer( bufferDescription );
    }
}

void TextRenderingModule::addOutlinedText( const char* text, f32 size, f32 x, f32 y, const dkVec4f& textColor, const f32 outlineThickness )
{
    lockVertexBuffer();

    // TODO We should simply check if the renderpass is active in order to figure out if we should do an early exit to
    // avoid a buffer overflow (and avoid wrapping its index in case no flush call has been done for several frames)
    bufferOffset = ( bufferOffset % TEXT_RENDERING_MAX_GLYPH_COUNT );

    const f32 baseX = x, localSize = size;
    f32 localX = x, localY = y;

    const auto localTextColor = textColor;

    i32 charIdx = 0;
    for ( const char* p = text; *p != '\0'; p++, charIdx++ ) {
        auto& g = fontDescriptor->Glyphes[static_cast<size_t>( *p )];

        f32 gx = localX + static_cast<f32>( g.OffsetX ) * localSize;
        f32 gy = -localY - static_cast<f32>( g.OffsetY ) * localSize;
        f32 gw = static_cast<f32>( g.Width ) * localSize;
        f32 gh = static_cast<f32>( g.Height ) * localSize;

        localX += g.AdvanceX * localSize;

        if ( *p == '\n' ) {
            localY += 38 * localSize;
            localX = baseX;
            charIdx = 0;
            continue;
        } else if ( *p == '\t' ) {
            localX += 28 * localSize * ( ( charIdx + 1 ) % 4 );
            continue;
        }

        if ( gw <= 0.0f || gh <= 0.0f )
            continue;

        f32 u1 = static_cast<f32>( g.PositionX ) / static_cast<f32>( fontDescriptor->AtlasWidth );
        f32 u2 = u1 + ( static_cast<f32>( g.Width ) / static_cast<f32>( fontDescriptor->AtlasWidth ) );
        f32 v1 = static_cast<f32>( g.PositionY ) / static_cast<f32>( fontDescriptor->AtlasHeight );
        f32 v2 = v1 + ( static_cast<f32>( g.Height ) / static_cast<f32>( fontDescriptor->AtlasHeight ) );

        buffer[bufferOffset++] = ( gx );
        buffer[bufferOffset++] = ( gy );
        buffer[bufferOffset++] = ( 1.0f );
        buffer[bufferOffset++] = ( outlineThickness );
        buffer[bufferOffset++] = ( u1 );
        buffer[bufferOffset++] = ( v1 );
        buffer[bufferOffset++] = ( localTextColor.x );
        buffer[bufferOffset++] = ( localTextColor.y );
        buffer[bufferOffset++] = ( localTextColor.z );
        buffer[bufferOffset++] = ( localTextColor.w );

        buffer[bufferOffset++] = ( gx );
        buffer[bufferOffset++] = ( gy - gh );
        buffer[bufferOffset++] = ( 1.0f );
        buffer[bufferOffset++] = ( outlineThickness );
        buffer[bufferOffset++] = ( u1 );
        buffer[bufferOffset++] = ( v2 );
        buffer[bufferOffset++] = ( localTextColor.x );
        buffer[bufferOffset++] = ( localTextColor.y );
        buffer[bufferOffset++] = ( localTextColor.z );
        buffer[bufferOffset++] = ( localTextColor.w );

        buffer[bufferOffset++] = ( gx + gw );
        buffer[bufferOffset++] = ( gy - gh );
        buffer[bufferOffset++] = ( 1.0f );
        buffer[bufferOffset++] = ( outlineThickness );
        buffer[bufferOffset++] = ( u2 );
        buffer[bufferOffset++] = ( v2 );
        buffer[bufferOffset++] = ( localTextColor.x );
        buffer[bufferOffset++] = ( localTextColor.y );
        buffer[bufferOffset++] = ( localTextColor.z );
        buffer[bufferOffset++] = ( localTextColor.w );

        buffer[bufferOffset++] = ( gx + gw );
        buffer[bufferOffset++] = ( gy );
        buffer[bufferOffset++] = ( 1.0f );
        buffer[bufferOffset++] = ( outlineThickness );
        buffer[bufferOffset++] = ( u2 );
        buffer[bufferOffset++] = ( v1 );
        buffer[bufferOffset++] = ( localTextColor.x );
        buffer[bufferOffset++] = ( localTextColor.y );
        buffer[bufferOffset++] = ( localTextColor.z );
        buffer[bufferOffset++] = ( localTextColor.w );

        indiceCount += 6;
    }

    unlockVertexBuffer();
}

void TextRenderingModule::lockVertexBuffer()
{
    bool expected = false;
    while ( !vertexBufferRenderingLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    vertexBufferLock.store( true );
}

void TextRenderingModule::unlockVertexBuffer()
{
    vertexBufferLock.store( false, std::memory_order_release );
}

void TextRenderingModule::lockVertexBufferRendering()
{
    bool expected = false;
    while ( !vertexBufferLock.compare_exchange_weak( expected, false ) ) {
        expected = false;
    }

    vertexBufferRenderingLock.store( true );
}

void TextRenderingModule::unlockVertexBufferRendering()
{
    vertexBufferRenderingLock.store( false, std::memory_order_release );
}
