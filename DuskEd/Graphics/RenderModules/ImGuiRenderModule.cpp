/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_DEVBUILD
#if DUSK_USE_IMGUI
#include "ImGuiRenderModule.h"

#include <Maths/MatrixTransformations.h>

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include <Graphics/RenderModules/Generated/dkImGui.generated.h>

#include "imgui.h"

enum eImGuiLockType : i32
{
    IMGUI_LOCK_READY = 0,
    IMGUI_LOCK_UPLOAD = 1,
    IMGUI_LOCK_RENDERING = 2
};

ImGuiRenderModule::ImGuiRenderModule()
    : imguiCmdListCount( 0 )
    , imguiCmdLists( nullptr )
    , fontAtlas( nullptr )
    , displaySize( dkVec2f::Zero )
    , displayPos( dkVec2f::Zero )
    , vertexBuffer( nullptr )
    , indiceBuffer( nullptr )
    , renderingLock( 0 )
{

}

ImGuiRenderModule::~ImGuiRenderModule()
{

}

void ImGuiRenderModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Dusk::ImGuiRenderModule";

    // Create static vbo/ibo
    BufferDesc bufferDescription;
    bufferDescription.Usage = RESOURCE_USAGE_DYNAMIC;
    bufferDescription.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    bufferDescription.SizeInBytes = sizeof( ImDrawVert ) * 32000;
    bufferDescription.StrideInBytes = sizeof( ImDrawVert );
    vertexBuffer = renderDevice.createBuffer( bufferDescription );

    BufferDesc indiceBufferDescription;
    indiceBufferDescription.Usage = RESOURCE_USAGE_DYNAMIC;
    indiceBufferDescription.BindFlags = RESOURCE_BIND_INDICE_BUFFER;
    indiceBufferDescription.SizeInBytes = 32000 * sizeof( ImDrawIdx );
    indiceBufferDescription.StrideInBytes = sizeof( ImDrawIdx );
    indiceBuffer = renderDevice.createBuffer( indiceBufferDescription );

    // Build texture atlas
    unsigned char* pixels;
    i32 width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height );

    ImageDesc textureDesc;
    textureDesc.dimension = ImageDesc::DIMENSION_2D;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = eViewFormat::VIEW_FORMAT_R8G8B8A8_UNORM;
    textureDesc.arraySize = 1;
    textureDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
    textureDesc.depth = 0;
    textureDesc.mipCount = 1;
    textureDesc.samplerCount = 1;
    textureDesc.usage = RESOURCE_USAGE_STATIC;
    
    fontAtlas = renderDevice.createImage( textureDesc, pixels, static_cast<size_t>( width * height * 4 ) );

    ImGui::GetIO().Fonts->TexID = static_cast< ImTextureID >( fontAtlas );
}

void ImGuiRenderModule::destroy( RenderDevice& renderDevice )
{
	renderDevice.destroyImage( fontAtlas );
	renderDevice.destroyBuffer( vertexBuffer );
	renderDevice.destroyBuffer( indiceBuffer );

    ImGui::DestroyContext();
}

FGHandle ImGuiRenderModule::render( FrameGraph& frameGraph, FGHandle renderTarget )
{
    struct PassData {
        FGHandle Output;
        FGHandle PerPassBuffer;
    };

    constexpr PipelineStateDesc PipelineDesc(
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc(),
        BlendStateDesc(
            true, false, false,
            true, true, true, false,
            { eBlendSource::BLEND_SOURCE_SRC_ALPHA, eBlendSource::BLEND_SOURCE_INV_SRC_ALPHA, eBlendOperation::BLEND_OPERATION_ADD },
            { eBlendSource::BLEND_SOURCE_INV_SRC_ALPHA, eBlendSource::BLEND_SOURCE_ZERO, eBlendOperation::BLEND_OPERATION_ADD }
        ),
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::CLEAR, VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
        { { RenderingHelpers::S_BilinearClampEdge }, 1 },
        { {
            { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 0, 0, false, "POSITION" },
            { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 0, 0, true, "TEXCOORD" },
            { 0, VIEW_FORMAT_R8G8B8A8_UNORM, 0, 0, 0, true, "COLOR" }
        } }
    );

    PassData& passData = frameGraph.addRenderPass<PassData>(
        dkImGui::ImGui_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            BufferDesc perPassBufferDesc;
            perPassBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassBufferDesc.SizeInBytes = sizeof( dkImGui::ImGuiRuntimeProperties );
            perPassBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            perPassBufferDesc.StrideInBytes = 0;
            passData.PerPassBuffer = builder.allocateBuffer( perPassBufferDesc, SHADER_STAGE_VERTEX | SHADER_STAGE_PIXEL );

            ImageDesc outputDesc;
            outputDesc.dimension = ImageDesc::DIMENSION_2D;
            outputDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
            outputDesc.usage = RESOURCE_USAGE_DEFAULT;
            outputDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

            passData.Output = builder.allocateImage( outputDesc, FrameGraphBuilder::eImageFlags::USE_SCREEN_SIZE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            // Spin Until the main thread has finished the RenderList update.
            lockForRendering();

            Buffer* parametersBuffer = resources->getBuffer( passData.PerPassBuffer );
            Image* outputTarget = resources->getImage( passData.Output );

            // Update ImGui internal resources
            update( *cmdList );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( PipelineDesc, dkImGui::ImGui_ShaderBinding );
            cmdList->pushEventMarker( dkImGui::ImGui_EventName );
            cmdList->bindPipelineState( pipelineState );

            // Do the render pass
            Viewport vp;
            vp.X = 0;
            vp.Y = 0;
            vp.Width = static_cast< i32 >( displaySize.x );
            vp.Height = static_cast< i32 >( displaySize.y );
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            cmdList->setViewport( vp );

            cmdList->bindConstantBuffer( PerPassBufferHashcode, parametersBuffer );
            cmdList->bindImage( dkImGui::ImGui_FontAtlasTexture_Hashcode, fontAtlas );
            cmdList->prepareAndBindResourceList();

            FramebufferAttachment output( outputTarget );
            cmdList->setupFramebuffer( &output );

            cmdList->updateBuffer( *parametersBuffer, &dkImGui::ImGuiProperties, sizeof( dkImGui::ImGuiRuntimeProperties ) );

            const Buffer* vbos[1] = { vertexBuffer };
            cmdList->bindVertexBuffer( vbos );
            cmdList->bindIndiceBuffer( indiceBuffer );

            ImTextureID activeTexId = static_cast< Image* >( fontAtlas );

            // Render command lists
            i32 vtx_offset = 0;
            i32 idx_offset = 0;
            for ( i32 n = 0; n < imguiCmdListCount; n++ ) {
                const ImDrawList* cmd_list = imguiCmdLists[n];
                for ( i32 cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ ) {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

                    ScissorRegion scissorRegion;
                    scissorRegion.Left = static_cast< i32 >( pcmd->ClipRect.x - displayPos.x );
                    scissorRegion.Top = static_cast< i32 >( pcmd->ClipRect.y - displayPos.y );
                    scissorRegion.Right = static_cast< i32 >( pcmd->ClipRect.z - displayPos.x );
                    scissorRegion.Bottom = static_cast< i32 >( pcmd->ClipRect.w - displayPos.y );
                    cmdList->setScissor( scissorRegion );

                    // Check if we need to update the resource binding.
                    ImTextureID cmdTexId = static_cast< ImTextureID >( pcmd->TextureId );
                    if ( cmdTexId != activeTexId ) {
                        Image* cmdImage = static_cast< Image* >( pcmd->TextureId );
                        
                        cmdList->bindImage( dkImGui::ImGui_FontAtlasTexture_Hashcode, cmdImage );
                        cmdList->prepareAndBindResourceList();

                        activeTexId = static_cast< ImTextureID >( cmdImage );
                    }

                    cmdList->drawIndexed( pcmd->ElemCount, 1, pcmd->IdxOffset + idx_offset, pcmd->VtxOffset + vtx_offset );
                }

                idx_offset += cmd_list->IdxBuffer.Size;
                vtx_offset += cmd_list->VtxBuffer.Size;
            }

            cmdList->popEventMarker();

            unlock();
        }
    );

    return passData.Output;
}

void ImGuiRenderModule::lockForUpload()
{
    while ( !acquireLock( IMGUI_LOCK_UPLOAD ) );
}

void ImGuiRenderModule::lockForRendering()
{
	while ( !acquireLock( IMGUI_LOCK_RENDERING ) );
}

void ImGuiRenderModule::update( CommandList& cmdList )
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImDrawVert* vtx_dst = static_cast< ImDrawVert* >( cmdList.mapBuffer( *vertexBuffer ) );
    ImDrawIdx* idx_dst = static_cast< ImDrawIdx* >( cmdList.mapBuffer( *indiceBuffer ) );
    for ( i32 n = 0; n < draw_data->CmdListsCount; n++ ) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy( vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
        memcpy( idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    cmdList.unmapBuffer( *indiceBuffer );
    cmdList.unmapBuffer( *vertexBuffer );

    dkImGui::ImGuiProperties.ProjectionMatrix = dk::maths::MakeOrtho(
        draw_data->DisplayPos.x,
        draw_data->DisplayPos.x + draw_data->DisplaySize.x,
        draw_data->DisplayPos.y + draw_data->DisplaySize.y,
        draw_data->DisplayPos.y,
        0.0f,
        1.0f
    );

    displaySize.x = draw_data->DisplaySize.x;
    displaySize.y = draw_data->DisplaySize.y;

    displayPos.x = draw_data->DisplayPos.x;
    displayPos.y = draw_data->DisplayPos.y;

    imguiCmdListCount = draw_data->CmdListsCount;
    imguiCmdLists = draw_data->CmdLists;
}

bool ImGuiRenderModule::acquireLock( const i32 nextState )
{
	i32 lockState =  IMGUI_LOCK_READY;
	return renderingLock.compare_exchange_weak( lockState, nextState, std::memory_order_acquire );
}

void ImGuiRenderModule::unlock()
{
    renderingLock.store( IMGUI_LOCK_READY, std::memory_order_release );
}
#endif
#endif
