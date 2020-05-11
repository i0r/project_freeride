/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class DisplaySurface;
class CommandList;
class LinearAllocator;

struct RenderContext;
struct Image;
struct Buffer;
struct PipelineState;
struct Shader;
struct Sampler;
struct QueryPool;

#include <Rendering/Shared.h>

class RenderDevice
{
public:
#if DUSK_D3D11
    // NOTE Resource buffering is already done by the driver
    static constexpr i32        PENDING_FRAME_COUNT = 1;
#else
    static constexpr i32        PENDING_FRAME_COUNT = 3;
#endif

    static constexpr i32        CMD_LIST_POOL_CAPACITY = 32; // Capacity for each Cmdlist type

public:
    // Returns a pointer to the active RenderContext used by the RenderDevice
    // This should only be used by third-party APIs/libraries which requires access
    // to the underlying rendering API
    DUSK_INLINE RenderContext*  getRenderContext() const { return renderContext; }

public:
                                RenderDevice( BaseAllocator* allocator );
                                RenderDevice( RenderDevice& ) = delete;
                                RenderDevice& operator = ( RenderDevice& ) = delete;
                                ~RenderDevice();

    void                        create( DisplaySurface& displaySurface, const bool useDebugContext = false );
    void                        enableVerticalSynchronisation( const bool enabled );
    void                        present();

    void                        waitForPendingFrameCompletion();

    static const dkChar_t*      getBackendName();

    CommandList&                allocateGraphicsCommandList();
    CommandList&                allocateComputeCommandList();
    CommandList&                allocateCopyCommandList();

    void                        submitCommandList( CommandList& cmdList );
    void                        submitCommandLists( CommandList** cmdLists, const u32 cmdListCount );

    size_t                      getFrameIndex() const;
    Image*                      getSwapchainBuffer();

    Image*                      createImage( const ImageDesc& description, const void* initialData = nullptr, const size_t initialDataSize = 0 );
    Buffer*                     createBuffer( const BufferDesc& description, const void* initialData = nullptr );
    Shader*                     createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize );
    Sampler*                    createSampler( const SamplerDesc& description );
    PipelineState*              createPipelineState( const PipelineStateDesc& description );
    QueryPool*                  createQueryPool( const eQueryType type, const u32 poolCapacity );

    void                        destroyImage( Image* image );
    void                        destroyBuffer( Buffer* buffer );
    void                        destroyShader( Shader* shader );
    void                        destroyPipelineState( PipelineState* pipelineState );
    void                        destroyPipelineStateCache( PipelineState* pipelineState );
    void                        destroySampler( Sampler* sampler );
    void                        destroyQueryPool( QueryPool* queryPool );

    void                        setDebugMarker( Image& image, const dkChar_t* objectName );
    void                        setDebugMarker( Buffer& buffer, const dkChar_t* objectName );

    f64                         convertTimestampToMs( const u64 timestamp ) const;
    void                        getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize );

private:
    BaseAllocator*              memoryAllocator;
    LinearAllocator*            pipelineStateCacheAllocator;
    RenderContext*              renderContext;
    Image*                      swapChainImage;
    size_t                      frameIndex;

    LinearAllocator*            graphicsCmdListAllocator[PENDING_FRAME_COUNT];
    LinearAllocator*            computeCmdListAllocator[PENDING_FRAME_COUNT];
};
