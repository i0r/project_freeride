/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class BaseAllocator;

struct NativeCommandList;
struct PipelineState;
struct Buffer;
struct QueryPool;
struct Image;
struct Sampler;

#include <Core/ViewFormat.h>
#include <Core/Types.h>
#include "RenderDevice.h"

struct ScissorRegion
{
    union {
        struct {
            i32     Left;
            i32     Top;
            i32     Right;
            i32     Bottom;
        };

        u64     SortKey;
    };

    constexpr bool operator == ( const ScissorRegion& r ) const
    {
        return ( SortKey ^ r.SortKey ) == 0;
    }

    constexpr bool operator != ( const ScissorRegion& r ) const
    {
        return !( *this == r );
    }
};

struct Viewport
{
    union {
        struct {
            i32     X;
            i32     Y;
            i32     Width;
            i32     Height;

            f32     MinDepth;
            f32     MaxDepth;
        };

        u64     SortKey[2];
    };

    const bool operator == ( const Viewport& r ) const
    {
        return ( SortKey[0] ^ r.SortKey[0] ) == 0
            && ( SortKey[1] ^ r.SortKey[1] ) == 0;
    }

    const bool operator != ( const Viewport& r ) const
    {
        return !( *this == r );
    }
};

enum eResourceState
{
    RESOURCE_STATE_UNKNOWN,
    RESOURCE_STATE_VERTEX_BUFFER,
    RESOURCE_STATE_CBUFFER,
    RESOURCE_STATE_RENDER_TARGET,
    RESOURCE_STATE_UAV,
    RESOURCE_STATE_DEPTH_WRITE,
    RESOURCE_STATE_DEPTH_READ,
    RESOURCE_STATE_PIXEL_BINDED_RESOURCE,
    RESOURCE_STATE_ALL_BINDED_RESOURCE,
    RESOURCE_STATE_INDIRECT_ARGS,
    RESOURCE_STATE_COPY_DESTINATION,
    RESOURCE_STATE_COPY_SOURCE,
    RESOURCE_STATE_RESOLVE_DESTINATION,
    RESOURCE_STATE_RESOLVE_SOURCE,
    RESOURCE_STATE_SWAPCHAIN_BUFFER ,
    RESOURCE_STATE_INDEX_BUFFER,
};

static constexpr u32 QUERY_START_AT_RETRIEVAL_MARK = 0xffffffff;
static constexpr u32 QUERY_COUNT_WHOLE_POOL = 0xffffffff;
static constexpr u32 BUFFER_MAP_WHOLE_MEMORY = 0;
static constexpr u32 IMAGE_UPDATE_WHOLE_MIPCHAIN = 0;
static constexpr u32 MAX_VERTEX_BUFFER_BIND_COUNT = 8;

class CommandList
{
public:
    enum class Type {
        GRAPHICS,
        COMPUTE
    };

    enum TransitionType {
        TRANSITION_SAME_QUEUE = 0,
        TRANSITION_GRAPHICS_TO_COMPUTE,
        TRANSITION_COMPUTE_TO_GRAPHICS,
        TRANSITION_GRAPHICS_TO_PRESENT,
        TRANSITION_COMPUTE_TO_PRESENT
    };

public:
    DUSK_INLINE NativeCommandList*  getNativeCommandList() const { return nativeCommandList; }
    DUSK_INLINE void                setNativeCommandList( NativeCommandList* cmdList ) { nativeCommandList = cmdList; }
    DUSK_INLINE Type                getCommandListType() const { return commandListType; }
	DUSK_INLINE i32                 getCommandListPooledIndex() const { return commandListPoolIndex; }
	DUSK_INLINE i32                 getFrameIndex() const { return frameIndex; }

public:
                                    CommandList( const Type cmdListType );
                                    CommandList( CommandList& ) = delete;
                                    CommandList& operator = ( CommandList& ) = delete;
                                    ~CommandList();

    void                            setFrameIndex( const i32 deviceFrameIndex );
    void                            initialize( BaseAllocator* allocator, const i32 pooledIndex );

    void                            begin();
    void                            end();

    void                            bindVertexBuffer( const Buffer** buffers, const u32 bufferCount = 1, const u32 startBindIndex = 0 );
    void                            bindIndiceBuffer( const Buffer* buffer, const bool use32bitsIndices = false );

    void                            setupFramebuffer( Image** renderTargetViews, Image* depthStencilView = nullptr );
    void                            clearRenderTargets( Image** renderTargetViews, const u32 renderTargetCount, const f32 clearValues[4] );
    void                            clearDepthStencil( Image* depthStencilView, const f32 clearValue, const bool clearStencil = false, const u8 clearStencilValue = 0xff );

    void                            prepareAndBindResourceList();

    void                            bindPipelineState( PipelineState* pipelineState );
    void                            bindConstantBuffer( const dkStringHash_t hashcode, Buffer* buffer );
    void                            bindImage( const dkStringHash_t hashcode, Image* image, const ImageViewDesc viewDescription = ImageViewDesc() );
    void                            bindBuffer( const dkStringHash_t hashcode, Buffer* buffer, const eViewFormat viewFormat = VIEW_FORMAT_UNKNOWN );
    void                            bindSampler( const dkStringHash_t hashcode, Sampler* sampler );

    void                            setViewport( const Viewport& viewport );
    void                            setScissor( const ScissorRegion& scissorRegion );

    void                            draw( const u32 vertexCount, const u32 instanceCount, const u32 vertexOffset = 0u, const u32 instanceOffset = 0u );
    void                            drawIndexed( const u32 indiceCount, const u32 instanceCount, const u32 indiceOffset = 0u, const u32 vertexOffset = 0u, const u32 instanceOffset = 0u );
 
    void                            dispatchCompute( const u32 threadCountX, const u32 threadCountY, const u32 threadCountZ );

    void                            updateBuffer( Buffer& buffer, const void* data, const size_t dataSize );
    void*                           mapBuffer( Buffer& buffer, const u32 startOffsetInBytes = 0, const u32 sizeInBytes = BUFFER_MAP_WHOLE_MEMORY );
    void                            unmapBuffer( Buffer& buffer );

    u32                             allocateQuery( QueryPool& queryPool );
    void                            beginQuery( QueryPool& queryPool, const u32 queryIndex );
    void                            endQuery( QueryPool& queryPool, const u32 queryIndex );
    void                            writeTimestamp( QueryPool& queryPool, const u32 queryIndex );
    void                            retrieveQueryResults( QueryPool& queryPool, const u32 startQueryIndex = QUERY_START_AT_RETRIEVAL_MARK, const u32 queryCount = QUERY_COUNT_WHOLE_POOL );
    void                            getQueryResult( QueryPool& queryPool, u64* resultsArray, const u32 startQueryIndex = QUERY_START_AT_RETRIEVAL_MARK, const u32 queryCount = QUERY_COUNT_WHOLE_POOL );

    void                            pushEventMarker( const dkChar_t* eventName );
    void                            popEventMarker();

    void                            copyBuffer( Buffer* sourceBuffer, Buffer* destBuffer );

    // NOTE Transition should be handled by higher level APIs (RenderGraph, etc.)
    // Use explicit transition for special cases only
    void                            transitionImage( Image& image, const eResourceState state, const u32 mipIndex = 0, const TransitionType transitionType = TRANSITION_SAME_QUEUE );
    void                            transitionBuffer( Buffer& buffer, const eResourceState state );

    void                            insertComputeBarrier( Image& image );

private:
    NativeCommandList*              nativeCommandList;
    BaseAllocator*                  memoryAllocator;
    Type                            commandListType;
    i32                             resourceFrameIndex;
    i32                             frameIndex;

    // Internal pool index to figure out which external resource is bound
    // to the command list
    i32                             commandListPoolIndex;
};
