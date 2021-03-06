/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_D3D11
#include <d3d11.h>
#include <queue>
#include <Core/Allocators/LinearAllocator.h>

struct PipelineState;
struct Buffer;
struct Image;

// Because of D3D11 deferred context crappiness, we use "software" command buffers as an alternative (we record the cmds
// in a multithreaded context and replay the commands in the immediate context at the end of the frame).
struct NativeCommandList
{
    // Pointer to RenderDevice immediate context.
    // (some commands require immediate flushing (e.g. buffer mapping for memcpy))
    ID3D11DeviceContext*                    ImmediateContext;

    // Memory allocator for CommandList packets.
    LinearAllocator*                        CommandPacketAllocator;

    // Queue holding this command list packets. Each cmd is a pointer to the head of the packet (i.e. its identifier).
    std::queue<u32*>                        Commands;

    // Active PipelineState.
    PipelineState*                          BindedPipelineState;

    NativeCommandList( LinearAllocator* cmdPacketAllocator )
        : ImmediateContext( nullptr )
        , CommandPacketAllocator( cmdPacketAllocator )
        , BindedPipelineState( nullptr )
    {

    }

    ~NativeCommandList()
    {
        ImmediateContext = nullptr;
        BindedPipelineState = nullptr;
    }
};

namespace CommandPacket
{
    // Internal packets for multithreaded commands recording.
    struct BindVertexBuffer 
    {
        u32             Identifier;
        u32             BufferCount;
        u32             StartBindIndex;

        ID3D11Buffer*   Buffers[MAX_VERTEX_BUFFER_BIND_COUNT];
        UINT            Strides[MAX_VERTEX_BUFFER_BIND_COUNT];
        UINT            Offsets[MAX_VERTEX_BUFFER_BIND_COUNT];
    };

    struct BindIndiceBuffer 
    {
        u32             Identifier;
        Buffer*         Buffer;
        DXGI_FORMAT     ViewFormat;
        UINT            Offset;
    };

    struct UpdateBuffer 
    {
        u32             Identifier;
        ID3D11Buffer*   BufferObject;
        size_t          DataSize;
        void*           Data;
    };

	struct CopyResource
	{
		u32             Identifier;
		ID3D11Resource* SourceResource;
		ID3D11Resource* DestResource;
	};

	struct MultiDrawIndexedInstancedIndirect
	{
		u32             Identifier;
		u32             DrawCount;
		ID3D11Buffer*   ArgsBuffer;
		u32             BufferAlignmentInBytes;
		u32             ArgumentsSizeInBytes;
	};

    struct SetupFramebuffer
    {
        u32                                    Identifier;
        FramebufferAttachment                  RenderTargetView[8];
        FramebufferAttachment                  DepthStencilView;
    };

    struct BindPipelineState
    {
        u32                     Identifier;
        PipelineState*          PipelineStateObject;
    };
    
    struct BindConstantBuffer
    {
        u32                     Identifier;
        Buffer*                 BufferObject;
        dkStringHash_t          ObjectHashcode;
    };

    struct BindResource
    {
        u32                     Identifier;

        union {
            Image*              ImageObject;
            Buffer*             BufferObject;
            Sampler*            SamplerObject;
        };

        dkStringHash_t          ObjectHashcode;
        u64                     ViewKey;
    };

    struct SetViewport
    {
        u32                     Identifier;
        Viewport                ViewportObject;
    };

    struct SetScissor
    {
        u32                     Identifier;
        ScissorRegion           ScissorObject;
    };

    struct Draw
    {
        u32                     Identifier;
        u32                     VertexCount;
        u32                     InstanceCount;
        u32                     VertexOffset;
        u32                     InstanceOffset;
    };

    struct DrawIndexed
    {
        u32                     Identifier;
        u32                     IndexCount;
        u32                     InstanceCount;
        u32                     IndexOffset;
        u32                     VertexOffset;
        u32                     InstanceOffset;
    };

    struct Dispatch
    {
        u32                     Identifier;
        u32                     ThreadCountX;
        u32                     ThreadCountY;
        u32                     ThreadCountZ;
    };

    struct PushEvent
    {
        u32                     Identifier;
        const dkChar_t*         EventName;
    };

    struct ClearImage 
    {
        u32                     Identifier;
        u32                     ImageCount;
        Image*                  RenderTargetView[8];
        f32                     ClearValue[4];
    };

    struct ClearDepthStencil
    {
        u32                     Identifier;
        Image*                  DepthStencil;
        f32                     ClearValue;
        u8                      ClearStencil;
        u8                      ClearStencilValue;
    };
    
    struct CopyImage
    {
        u32                     Identifier;
        u32                     SourceResourceIndex;
        ID3D11Resource*         SourceImage;
        ID3D11Resource*         DestImage;
        u32                     DestResourceIndex;
    };

    struct ResolveImage 
    {
        u32                     Identifier;
        DXGI_FORMAT             Format;
        ID3D11Resource*         SourceImage;
        ID3D11Resource*         DestImage;
    };

    struct ArgumentLessPacket
    {
        u32                     Identifier;
    };
}

enum CommandPacketIdentifier 
{
    // Default/invalid command
    CPI_UNKNOWN = 0,

    // Bind one or several vertex buffer.
    CPI_BIND_VERTEX_BUFFER,

    // Bind a single indice buffer.
    CPI_BIND_INDICE_BUFFER,

    // Update a single buffer.
    CPI_UPDATE_BUFFER,

    // Bind color attachments/depth attachment. Number of attachment is defined by the active pipeline state.
    CPI_SETUP_FRAMEBUFFER,

    // Bind a pipeline state.
    CPI_BIND_PIPELINE_STATE,

    // Prepare and bind resources.
    CPI_PREPARE_AND_BIND_RESOURCES,

    // Bind a constant buffer.
    CPI_BIND_CBUFFER,

    // Bind a image view.
    CPI_BIND_IMAGE,

    // Bind a buffer view.
    CPI_BIND_BUFFER,

    // Set rasterizer viewport.
    CPI_SET_VIEWPORT,

    // Set rasterizer scissor.
    CPI_SET_SCISSOR,

    // Draw one or several instance.
    CPI_DRAW,

    // Draw one or several indexed instance.
    CPI_DRAW_INDEXED,

    // Dispatch a compute command.
    CPI_DISPATCH,

    // Push a debug event marker.
    CPI_PUSH_EVENT,

    // Pop a debug event marker.
    CPI_POP_EVENT,

    // Ends a command list.
    CPI_END,

    // Clear Image Views.
    CPI_CLEAR_IMAGES,

    // Clear Depth stencil view (stencil clear is optional).
    CPI_CLEAR_DEPTH_STENCIL,

    // Copy one resource to another one.
    CPI_COPY_RESOURCE,

    // Bind a sampler state.
    CPI_BIND_SAMPLER,

    // Copy Image (src to dst).
    CPI_COPY_IMAGE,

    // Perform an hardware resource resolve.
    CPI_RESOLVE_IMAGE,

    CPI_MULTI_DRAW_INDEXED_INSTANCED_INDIRECT,

    // CommandPacketIdentifier enum count. Not for use.
    CPI_COUNT
};
#endif
