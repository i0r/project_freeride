/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

class FrameGraphResources;
class GraphicsProfiler;
class FrameGraphBuilder;
class PipelineStateCache;
class VirtualFileSystem;

struct Buffer;
struct Image;
struct DrawCmd;

#include <unordered_map>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>

#include <Framework/Cameras/Camera.h>
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "DrawCommand.h"

#include "ShaderHeaders/MaterialRuntimeEd.h"

template<typename T>
using dkPassSetup_t = std::function< void( FrameGraphBuilder&, T& ) >;

template<typename T>
using dkPassCallback_t = std::function< void( const T&, const FrameGraphResources*, CommandList*, PipelineStateCache* ) >;

static constexpr dkStringHash_t PerViewBufferHashcode = DUSK_STRING_HASH( "PerViewBuffer" );
static constexpr dkStringHash_t PerPassBufferHashcode = DUSK_STRING_HASH( "PerPassBuffer" );
static constexpr dkStringHash_t PerWorldBufferHashcode = DUSK_STRING_HASH( "PerWorldBuffer" );
static constexpr dkStringHash_t MaterialEditorBufferHashcode = DUSK_STRING_HASH( "MaterialEditorBuffer" );
static constexpr dkStringHash_t InstanceVectorBufferHashcode = DUSK_STRING_HASH( "InstanceVectorBuffer" );

// Return dispatch size of a given thread group and a given number of element to be processed by a kernel.
// Note that this is a one dimensional function (it should be called for each dimension of the thread grid).
static constexpr u32 DispatchSize( const u32 threadGroupSize, const u32 numElements )
{
    u32 dispatchSize = numElements / threadGroupSize;
    dispatchSize += ( ( numElements % threadGroupSize ) > 0 ) ? 1 : 0;

    return dispatchSize;
}

// PerView buffer content. You must force RenderPass baking whenever the content (or the order
// of this structure) is updated!
struct PerViewBufferData
{
    // View Projection Matrix.
    dkMat4x4f   ViewProjectionMatrix;

    // Inverse View Projection Matrix.
    dkMat4x4f   InverseViewProjectionMatrix;

    // Previous frame View Projection Matrix.
    dkMat4x4f   PreviousViewProjectionMatrix;

    // Orthographic Projection Matrix (for HUD/screenspace passes).
    dkMat4x4f   OrthoProjectionMatrix;

    // View Matrix.
    dkMat4x4f   ViewMatrix;

	// Projection Matrix.
	dkMat4x4f   ProjectionMatrix;

	// Projection Matrix (with finite far).
	dkMat4x4f   ProjectionMatrixFiniteFar;

    // Inverse projection matrix (with finite far).
    dkMat4x4f   InverseProjectionMatrix;

    // Inverse view matrix.
    dkMat4x4f   InverseViewMatrix;

    dkMat4x4f   ScreenToWorldMatrix;

    // Viewport Size. This is the size of the viewport showing the world view.
    // THIS VALUE MIGHT BE DIFFERENT FROM THE ACTUAL SCREEN SIZE (e.g. in editor).
    dkVec2f     ViewportSize;

    // Inverse viewport size (1.0f/ViewportSize).
    dkVec2f     InverseViewportSize;

    // World Position of the view.
    dkVec3f     WorldPosition;

    // Index of the frame being rendered.
    i32         FrameIndex;

    // View direction of the camera (normalized).
    dkVec3f     ViewDirection;

    // SSAA factor applied during the geometry renderpass.
    f32         ImageQuality;

    // Jittering offset applied to avoid temporal artifacts.
    dkVec2f     CameraJitteringOffset;

    // Mouse coordinates (in screen space). Uses top-left referential.
    dkVec2u     MouseCoordinates;

    dkVec3f     UpVector;

    f32         FieldOfView;

    dkVec3f     RightVector;

    f32         AspectRatio;

    f32         NearPlane;

    f32         FarPlane;

    u32         __PADDING__[2];
};
DUSK_IS_MEMORY_ALIGNED_STATIC( PerViewBufferData, 16 );

// Strongly typed handle for anything frame graph related (resources, render passes, etc.).
// Graph handles are immutable 
struct FGHandle 
{
private:
    u32 identifier;

public:
    FGHandle( const u32 id = FGHandle::Invalid )
        : identifier( id )
    {

    }

    bool operator == ( FGHandle& r ) const
    {
        return identifier == r.identifier;
    }

    bool operator != ( FGHandle& r ) const
    {
        return !( *this == r );
    }

    operator u32() const 
    {
        return identifier; 
    }

public:
    static const FGHandle Invalid;
};

struct FrameGraphRenderPass
{
    using Handle_t = u32;

    // Callback with PassData binded.
    using BindedCallback_t = std::function< void( CommandList*, PipelineStateCache* ) >;

    // Buffer for PassData storage.
    char                Data[1024];

    // Callback for binded lambda execution.
    BindedCallback_t    Execute;

    // Name of the FrameGraphRenderPass (for debug/profiling).
    std::string         Name;

    // FrameGraph internal handle (used for RenderPass culling).
    Handle_t            Handle;
};

struct RenderPassExecutionInfos 
{
    // Maximum dependency count per RenderPass.
    static constexpr i32 MAX_DEP_COUNT = 8;

    enum State {
        // Job has been added to the Scheduler and is waiting for execution.
        RP_EXECUTION_STATE_WAITING = 0,

        // Job is being executed by a FrameGraphRenderThread.
        RP_EXECUTION_STATE_IN_PROGRESS,

        // Job is completed.
        RP_EXECUTION_STATE_DONE,
    };
    using State_t = std::atomic<RenderPassExecutionInfos::State>;

    // Constant reference to the RenderPass to execute (should be owned by the FrameGraph).
    const FrameGraphRenderPass*         RenderPass;
    
    // Current execution state (see RenderPassExecutionInfos::State).
    State_t                             ExecutionState;

    // Dependencies for this RenderPass (the FrameGraphRenderThread will spin until all the dependencies are done).
    State_t*                            Dependencies[MAX_DEP_COUNT];

    // Dependencies count.
    u32                                 DependencyCount;

private:
    DUSK_INLINE void CopyDependencies( State_t* const * dependencies )
    {
        DUSK_RAISE_FATAL_ERROR( DependencyCount < MAX_DEP_COUNT, "DependencyCount: too many dependencies for this RenderPass!" );
        memcpy( Dependencies, dependencies, sizeof( State_t* ) * DependencyCount );
    }

public:
    RenderPassExecutionInfos( const FrameGraphRenderPass* renderPass = nullptr, State_t** dependencies = nullptr, const u32 dependencyCount = 0u )
        : RenderPass( renderPass )
        , ExecutionState( RP_EXECUTION_STATE_WAITING )
        , DependencyCount( dependencyCount )
    {
        CopyDependencies( dependencies );
    }

    RenderPassExecutionInfos& operator = ( const RenderPassExecutionInfos& r )
    {
        RenderPass = r.RenderPass;
        ExecutionState = RP_EXECUTION_STATE_WAITING;
        DependencyCount = r.DependencyCount;

        CopyDependencies( static_cast<State_t* const *>( r.Dependencies ) );
    
        return *this;
    }
};

#if DUSK_DEVBUILD
struct FGBufferInfosEditor
{
    // The description for this buffer.
	BufferDesc  Description;

    // List of stages this buffer is binded to during this frame.
	u32         ShaderStageBinding;

    // Number of reference for this buffer.
	u32         ReferenceCount;
};

struct FGImageInfosEditor
{
    ImageDesc   Description;

	u32         GraphFlags;
	
    u32         ReferenceCount;

    Image*      AllocatedImage;
};
#endif

class FrameGraphRenderThread
{
public:
    enum State {
        // Thread is ready and waiting for work.
        FGRAPH_THREAD_STATE_READY = 0,

        // Thread has received job to do.
        FGRAPH_THREAD_STATE_HAS_JOB_TO_DO,

        // Thread is executing one or several render passes.
        FGRAPH_THREAD_STATE_BUSY,

        // Thread has received a shutdown signal (e.g. dtor has been called) and should shutdown as soon as possible.
        FGRAPH_THREAD_STATE_WAITING_SHUTDOWN,
    };

    // Maximum number of RenderPassExecutionInfos per FrameGraphRenderThread (for a single frame).
    static constexpr i32        MAX_RENDERPASS_COUNT = 32;

public:
                                FrameGraphRenderThread( BaseAllocator* allocator, RenderDevice* renderDevice, VirtualFileSystem* virtualFileSys );
                                FrameGraphRenderThread( FrameGraphRenderThread& ) = default;
                                FrameGraphRenderThread& operator = ( FrameGraphRenderThread& ) = default;
                                ~FrameGraphRenderThread();

    // (Thread Safe) Return true if the FrameGraphRenderThread is available; false otherwise.
    bool                        isReady();

    // Returns true if there is at least one enqueued RenderPass; false otherwise.
    bool                        hasWorkTodo();

    // Execute enqueued RenderPassExecutionInfos with a CommandList allocated at higher level.
    // FrameGraphRenderThread is not responsible for CommandList submit/begin/end calls.
    void                        flush( CommandList* cmdList );

    // Enqueue a renderpass for execution
    void                        enqueueRenderPass( RenderPassExecutionInfos* renderpass );

private:
    // Allocator owning the memory of this FrameGraphRenderThread instance.
    BaseAllocator*              memoryAllocator;

    // Thread handle for this FrameGraphRenderThread instance.
    std::thread                 threadHandle;
    
    // Thread local PSO cache.
    PipelineStateCache*         pipelineStateCache;

    // Binded CommandList for this thread (allocated by the Job Dispatcher thread).
    CommandList*                commandList;

    // The current state of this FrameGraphRenderThread instance (see FrameGraphRenderThread::State).
    std::atomic<State>          currentState;

    // RenderPasses to execute this frame.
    RenderPassExecutionInfos*   renderpassToExecute[MAX_RENDERPASS_COUNT];

    // RenderPasses to execute count.
    std::atomic<u32>            rpToExecuteCount;

    // RenderPasses to execute count.
    std::atomic<u32>            rpInExecutionCount;

private:
    // Internal function executing incoming RenderPasses.
    void                        workerThread();
};

class FrameGraphScheduler     
{
public:
    // Maximum number of FrameGraphRenderPass scheduled per frame.
    static constexpr i32        MAX_RENDERPASS_PER_FRAME = 96;

public:
    // Return a pointer to the PerViewBuffer persistent buffer.
    DUSK_INLINE Buffer*         getPerViewPersistentBuffer() const { return perViewBuffer; }
    
#if DUSKED
    // Return a pointer to the MaterialEd persistent buffer.
    DUSK_INLINE Buffer*         getMaterialEdPersistentBuffer() const { return materialEditorBuffer; }
#endif

    // Return a pointer to the vector data persistent buffer.
    DUSK_INLINE Buffer*         getVectorDataBuffer() const { return vectorDataBuffer; }

public:
                                FrameGraphScheduler( BaseAllocator* allocator, RenderDevice* renderDevice, VirtualFileSystem* virtualFileSys );
                                FrameGraphScheduler( FrameGraphScheduler& ) = default;
                                FrameGraphScheduler& operator = ( FrameGraphScheduler& ) = default;
                                ~FrameGraphScheduler();

    // Enqueue a FrameGraphRenderPass for execution (with or without dependencies).
    void                        addRenderPass( const FrameGraphRenderPass& renderPass, const FrameGraphRenderPass::Handle_t* dependencies = nullptr, const u32 dependencyCount = 0u );

    // Enqueue an asynchronous FrameGraphRenderPass for execution (with or without dependencies). The Renderpass must run
    // on a GPU compute pipeline.
    void                        addAsyncComputeRenderPass( const FrameGraphRenderPass& renderPass, const FrameGraphRenderPass::Handle_t* dependencies = nullptr, const u32 dependencyCount = 0u );

    void                        updateMaterialEdBuffer( const MaterialEdData* matEdData );

    // Dispatch enqueued RenderPassExecutionInfos to FrameGraphRenderThreads. PerViewBufferData is a pointer to a persistent
    // resource containing the data for the current view (can be nil if the scheduled render passes don't need those infos)
    void                        dispatch( const PerViewBufferData* perViewData = nullptr, const void* vectorBufferData = nullptr );

    // (Thread Safe) Return true if the scheduler is ready to receive RenderPass; false otherwise.
    bool                        isReady();

private:
    enum State {
        // The scheduler is ready to receive RenderPass to execute.
        SCHEDULER_STATE_READY = 0,

        // External notification for RenderPass dispatching.
        SCHEDULER_STATE_HAS_JOB_TO_DO,

        // Waiting for job completion.
        SCHEDULER_STATE_WAITING_JOB_COMPLETION,

        // Scheduler has received a shutdown signal (e.g. dtor has been called) and should shutdown as soon as possible.
        SCHEDULER_STATE_WAITING_SHUTDOWN,
    };

    static constexpr i32        RENDER_THREAD_COUNT = 3;

private:
    // Allocator owning this instance.
    BaseAllocator*              memoryAllocator;

    // Thread executing RenderPasses from the FrameGraph.
    FrameGraphRenderThread*     workers;

    // Pointer to the active RenderDevice.
    RenderDevice*               renderDevice;

    // Persistent buffer (matches PerViewBuffer).
    Buffer*                     perViewBuffer;

#if DUSKED
    // Persistent buffer (matches MaterialEditorBuffer).
    Buffer*                     materialEditorBuffer;
#endif

	// Persistent buffer holding raw vector data.
	Buffer*                     vectorDataBuffer;

    // Thread responsible for CommandList submission to RenderDevice.
    std::thread                 dispatcherThread;

    // Enqueued FrameGraphRenderPass waiting for execution.
    RenderPassExecutionInfos    enqueuedRenderPass[MAX_RENDERPASS_PER_FRAME];

    // Enqueued FrameGraphRenderPass waiting for asynchronous execution.
    RenderPassExecutionInfos    enqueuedAsyncRenderPass[MAX_RENDERPASS_PER_FRAME];

    // Enqueued FrameGraphRenderPass count.
    u32                         enqueuedRenderPassCount;

    // Enqueued asynchronous FrameGraphRenderPass count.
    u32                         enqueuedAsyncRenderPassCount;

    // Scheduler current state.
    std::atomic<State>          currentState;

    // PerViewBufferData for the current frame (this is a copy of FrameGraph data).
    PerViewBufferData           perViewBufferData;

#if DUSKED
    // PerViewBufferData for the current frame (this is a copy of FrameGraph data).
    MaterialEdData              materialEdData;
#endif

	u8*                         instanceBufferData;

private:
    // Internal function for CommandList allocation/submit and RenderDevice present.
    void                        jobDispatcherThread();

    // (Thread Safe) Wait until all the FrameGraphRenderThread are ready.
    // This is a blocking call.
    void                        spinUntilCompletion();
};

class FrameGraphBuilder
{
public:
    enum eImageFlags {
        // Use builder viewport dimensions (override width/height).
        USE_PIPELINE_DIMENSIONS     = 1 << 0,

        // Use builder sampler count (override samplerCount).
        USE_PIPELINE_SAMPLER_COUNT  = 1 << 1,

        // Set sampler count to 1 (useful for image copy).
        NO_MULTISAMPLE              = 1 << 2,

        // Use builder viewport dimensions with image quality set to 1.0 (override width/height).
        USE_PIPELINE_DIMENSIONS_ONE = 1 << 3,

        // Use ScreenSize (THIS IS NOT THE VIEWPORT SIZE, this is the display surface size) (override width/height).
        USE_SCREEN_SIZE = 1 << 4,

        // Allocate one resource view per mip level allocable.
        REQUEST_PER_MIP_RESOURCE_VIEW = 1 << 5,
    };

    // Maximum of RenderPass count (per frame).
    static constexpr i32 MAX_RENDER_PASS_COUNT          = 64;

    // Maximum number of a single resource type allocable per frame.
    static constexpr i32 MAX_RESOURCES_HANDLE_PER_FRAME = 64;

public:
    // Set FrameGraph viewport.
    DUSK_INLINE void setPipelineViewport( const Viewport& viewport )    { frameViewport = viewport; }

    // Set FrameGraph scissor region.
    DUSK_INLINE void setPipelineScissor( const ScissorRegion& scissor ) { frameScissor = scissor; }

    // Set FrameGraph default sampler count for image allocation.
    DUSK_INLINE void setMSAAQuality( const u32 samplerCount = 1 )       { frameSamplerCount = samplerCount; }

    DUSK_INLINE const dkVec2u& getScreenSize() const                    { return frameScreenSize; }

    DUSK_INLINE void setScreenSize( const dkVec2u& screenSize )         { frameScreenSize = screenSize; }

    // Set FrameGraph default super scaling scale (e.g. 2.0 mean that image resolution will use a scale factor of 2).
    DUSK_INLINE void setImageQuality( const float imageQuality = 1.0f ) { frameImageQuality = imageQuality; }

    // Enable asynchronous compute for the renderpass being recorded (if the active graphics API supports it).
    DUSK_INLINE void useAsyncCompute()                                  { passRefs[( renderPassCount - 1 )].useAsyncCompute = true; }

    // Declare the renderpass being recorded as uncullable. This is useful if nothing consumes this renderpass output.
    // (e.g. Present renderpass; IBL convolution renderpass; etc.)
    DUSK_INLINE void setUncullablePass()                                { passRefs[( renderPassCount - 1 )].isUncullable = true; }

public:
                                    FrameGraphBuilder();
                                    FrameGraphBuilder( FrameGraphBuilder& ) = default;
                                    FrameGraphBuilder& operator = ( FrameGraphBuilder& ) = default;
                                    ~FrameGraphBuilder();

    // Allocate resources requested by the render passes.
    void        compile( RenderDevice* renderDevice, FrameGraphResources& resources );

    // Cull any render pass that has no impact on the final frame (unless the renderpass has been declared as uncullable).
    void        cullRenderPasses( FrameGraphRenderPass* renderPassList, i32& renderPassCount );

    // Schedule render pass recording on one or several threads (using the FrameGraphScheduler).
    void        scheduleRenderPasses( FrameGraphScheduler& graphScheduler, FrameGraphRenderPass* renderPassList, const i32 renderPassCount );

    // Start the recording of a renderpass.
    void        addRenderPass();

    // Request an image allocation (with an optional combination of eImageFlags flags). 
    FGHandle allocateImage( ImageDesc& description, const u32 imageFlags = 0 );

    // Request a buffer allocation for one or several stage bind (required for buffer reuse in a same pass).
    FGHandle allocateBuffer( BufferDesc& description, const u32 shaderStageBinding );

    // Request a sampler allocation.
    FGHandle allocateSampler( const SamplerDesc& description );

    // Declare the read of a read-only image (used as a hint for render pass scheduling).
    FGHandle readReadOnlyImage( const FGHandle resourceHandle );

    // Declare the read of a read-only buffer (used as a hint for render pass scheduling).
    FGHandle readReadOnlyBuffer( const FGHandle resourceHandle );

    // Declare the read of a read/write image (will create an implicit dependency with the owner of the resource).
    // Use readReadOnlyImage if you won't write to this image.
    FGHandle readImage( const FGHandle resourceHandle );

    // Declare the read of a read/write buffer (will create an implicit dependency with the owner of the resource).
    // Use readReadOnlyImage if you won't write to this buffer.
    FGHandle readBuffer( const FGHandle resourceHandle );

    // Retrieve the swapchain buffer for the current frame being recorded.
    FGHandle retrieveSwapchainBuffer();

    // Retrieve the post-fx output image for the current frame being recorded.
    FGHandle retrievePresentImage();

    // Retrieve last frame image (pre post-fx and post resolve).
    FGHandle retrieveLastFrameImage();

    // Retrieve last frame SSR image (pre post-fx and post resolve).
    FGHandle retrieveSSRLastFrameImage();

    // Retrieve the PerView buffer for the current frame being recorded.
    // The data should be immutable (the buffer is updated at the beginning of the frame once).
    FGHandle retrievePerViewBuffer();

    // Retrieve the MaterialEd buffer for the current frame being recorded.
    // The data should be immutable (the buffer is updated at the beginning of the frame once).
    FGHandle retrieveMaterialEdBuffer();
    
	// Retrieve the VectorData buffer for the current frame being recorded.
	// The data should be immutable (the buffer is updated at the beginning of the frame once).
    FGHandle retrieveVectorDataBuffer();

    // Retrieve a persistent image.
    FGHandle retrievePersistentImage( const dkStringHash_t resourceHashcode );

    // Retrieve a persistent buffer.
    FGHandle retrievePersistentBuffer( const dkStringHash_t resourceHashcode );

    // Copy an image resource description to a new resource.
    // NOTE descRef is a WRITE only OPTIONAL parameter to retrieve the copied resource description (for modifications, e.g. modifying the bind flags).
    FGHandle copyImage( const FGHandle resourceToCopy, ImageDesc** descRef, const u32 imageFlags = 0 );

#if DUSKED
    // Fill a given vector with this frame buffre allocation infos.
    void fillBufferEditorInfos( std::vector<FGBufferInfosEditor>& bufferInfos ) const;

    void fillImageEditorInfos( std::vector<FGImageInfosEditor>& imageInfos, const FrameGraphResources* graphResources ) const;
#endif

private:
    // FrameGraph active viewport.
    Viewport        frameViewport;

    // FrameGraph active scissor region.
    ScissorRegion   frameScissor;

    // FrameGraph active sampler count (MSAA).
    u32             frameSamplerCount;

    // FrameGraph active image quality (SSAA).
    float           frameImageQuality;

    dkVec2u         frameScreenSize;

    // Recorded render pass count.
    i32             renderPassCount;

    // Requested image count.
    u32             imageCount;

    // Requested buffer count.
    u32             bufferCount;

    // Persistent buffer count.
    u32             persitentBufferCount;

    // Persistent image count.
    u32             persitentImageCount;

    // Requested sampler count.
    u32             samplerStateCount;

    // RenderPass infos. The infos struct is filled during the renderpass record.
    struct PassInfos {
        u32                             imageHandles[MAX_RESOURCES_HANDLE_PER_FRAME];
        u32                             imageCount;
        u32                             bufferHandles[MAX_RESOURCES_HANDLE_PER_FRAME];
        u32                             buffersCount;
        bool                            isUncullable;
        bool                            useAsyncCompute;
        u32                             dependencyCount;
        FrameGraphRenderPass::Handle_t  dependencies[12];
    } passRefs[MAX_RENDER_PASS_COUNT];

    struct ImageAllocInfo {
        u32             flags;
        u32             referenceCount;
        FrameGraphRenderPass::Handle_t    requestSource;
        ImageDesc       description;
    } images[MAX_RESOURCES_HANDLE_PER_FRAME];

    struct BufferAllocInfo {
        u32             shaderStageBinding;
        u32             referenceCount;
        FrameGraphRenderPass::Handle_t    requestSource;
        BufferDesc      description;
    } buffers[MAX_RESOURCES_HANDLE_PER_FRAME];

    SamplerDesc     samplers[MAX_RESOURCES_HANDLE_PER_FRAME];

    // TODO Refacto: implement the persistent resources idea as a blackboard subsystem.
    dkStringHash_t  persitentBuffers[MAX_RESOURCES_HANDLE_PER_FRAME];
    dkStringHash_t  persitentImages[MAX_RESOURCES_HANDLE_PER_FRAME];

private:
    void ApplyImageDescriptionFlags( ImageDesc& description, const u32 imageFlags ) const;
    void updatePassDependency( PassInfos& passInfos, const FrameGraphRenderPass::Handle_t dependency );
};

class FrameGraphResources
{
public:
    struct DrawCmdBucket {
        DrawCmd*                    beginAddr;
        DrawCmd*                    endAddr;

        float                       vectorPerInstance;
        float                       instanceDataStartOffset;

        DUSK_INLINE DrawCmd*        begin()         { return beginAddr; }
        DUSK_INLINE const DrawCmd*  begin() const   { return beginAddr; }
        DUSK_INLINE DrawCmd*        end()           { return endAddr; }
        DUSK_INLINE const DrawCmd*  end() const     { return endAddr; }
    };

public:
                            FrameGraphResources( BaseAllocator* allocator );
                            FrameGraphResources( FrameGraphResources& ) = default;
                            FrameGraphResources& operator = ( FrameGraphResources& ) = default;
                            ~FrameGraphResources();

    void                    releaseResources( RenderDevice* renderDevice );
    void                    unacquireResources();

    void                    setPipelineViewport( const Viewport& viewport, const ScissorRegion& scissor, const CameraData* cameraData );
    void                    setImageQuality( const f32 imageQuality = 1.0f );

    void                    dispatchToBuckets( DrawCmd* drawCmds, const size_t drawCmdCount );
    void                    importPersistentImage( const dkStringHash_t resourceHashcode, Image* image );
    void                    importPersistentBuffer( const dkStringHash_t resourceHashcode, Buffer* buffer );

    const DrawCmdBucket&    getDrawCmdBucket( const DrawCommandKey::Layer layer, const uint8_t viewportLayer ) const;
    void*                   getVectorBufferData() const;

    const CameraData*       getMainCamera() const;
    const Viewport*         getMainViewport() const;
    const ScissorRegion*    getMainScissorRegion() const;

    void                    setScreenSize( const dkVec2u& screenSize );
    const dkVec2u&          getScreenSize() const;

    void                    updateDeltaTime( const float dt );
    f32                     getDeltaTime() const;

    Buffer*                 getBuffer( const FGHandle resourceHandle ) const;
    Image*                  getImage( const FGHandle resourceHandle ) const;
    Sampler*                getSampler( const FGHandle resourceHandle ) const;

    // TODO Find a clean way to merge persistent getters with regular resource getters?
    Buffer*                 getPersistentBuffer( const FGHandle resourceHandle ) const;
    Image*                  getPersitentImage( const FGHandle resourceHandle ) const;

    void                    allocateBuffer( RenderDevice* renderDevice, const FGHandle resourceHandle, const BufferDesc& description );
    void                    allocateImage( RenderDevice* renderDevice, const FGHandle resourceHandle, const ImageDesc& description, const u32 flags );
    void                    allocateSampler( RenderDevice* renderDevice, const FGHandle resourceHandle, const SamplerDesc& description );

    void                    bindPersistentBuffers( const FGHandle resourceHandle, const dkStringHash_t hashcode );
    void                    bindPersistentImages( const FGHandle resourceHandle, const  dkStringHash_t hashcode );

    bool                    isPersistentImageAvailable( const dkStringHash_t resourceHashcode ) const;
    bool                    isPersistentBufferAvailable( const dkStringHash_t resourceHashcode ) const;

private:
    static constexpr i32    MAX_ALLOCABLE_RESOURCE_TYPE = 96;

private:
    BaseAllocator*          memoryAllocator;
    DrawCmdBucket           drawCmdBuckets[4][8];
    CameraData              activeCameraData;
    Viewport                activeViewport;
    ScissorRegion           activeScissor;
    f32                     pipelineImageQuality;
    f32                     deltaTime;
    i32                     allocatedBuffersCount;
	i32                     allocatedImageCount;
	i32                     allocatedSamplerCount;
    dkVec2u                 activeScreenSize;

    void*                   instanceBufferData;

    Buffer*                 allocatedBuffers[MAX_ALLOCABLE_RESOURCE_TYPE];
    BufferDesc              buffersDesc[MAX_ALLOCABLE_RESOURCE_TYPE];
    bool                    isBufferFree[MAX_ALLOCABLE_RESOURCE_TYPE];

    Image*                  allocatedImages[MAX_ALLOCABLE_RESOURCE_TYPE];
    ImageDesc               imagesDesc[MAX_ALLOCABLE_RESOURCE_TYPE];
    bool                    isImageFree[MAX_ALLOCABLE_RESOURCE_TYPE];

	Sampler*                allocatedSamplers[MAX_ALLOCABLE_RESOURCE_TYPE];
	SamplerDesc             samplerDesc[MAX_ALLOCABLE_RESOURCE_TYPE];
	bool                    isSamplerFree[MAX_ALLOCABLE_RESOURCE_TYPE];

    Buffer*                 inUseBuffers[MAX_ALLOCABLE_RESOURCE_TYPE];
    Image*                  inUseImages[MAX_ALLOCABLE_RESOURCE_TYPE];
    Sampler*                inUseSamplers[MAX_ALLOCABLE_RESOURCE_TYPE];

    Buffer*                 persistentBuffers[MAX_ALLOCABLE_RESOURCE_TYPE];
    Image*                  persistentImages[MAX_ALLOCABLE_RESOURCE_TYPE];

    std::unordered_map<dkStringHash_t, Buffer*> persistentBuffersMap;
    std::unordered_map<dkStringHash_t, Image*>  persistentImagesMap;

private:
    void                    updateVectorBuffer( const DrawCmd& cmd, size_t& instanceBufferOffset );
};

class FrameGraph
{
public:
#if DUSKED
    // Fill a given vector with debug infos for buffers allocated by this graph.
    DUSK_INLINE void retrieveBufferEditorInfos( std::vector<FGBufferInfosEditor>& infos ) const { graphBuilder.fillBufferEditorInfos( infos ); }

    // Fill a given vector with debug infos for images allocated by this graph.
    DUSK_INLINE void retrieveImageEditorInfos( std::vector<FGImageInfosEditor>& infos ) const { graphBuilder.fillImageEditorInfos( infos, &graphResources ); }

    // Return the number of render passes enqueued for this frame. Note that this call should be
    // done after the FrameGraph acquire and before the renderpasses dispatch.
    DUSK_INLINE const i32 getRenderPassCount() const { return renderPassCount; }

    // Return the name of the renderpass stored at a given index.
    // This function DOES NOT check the sanity of the index.
    DUSK_INLINE const char* getRenderPassName( const i32 renderPassIndex ) const { return renderPasses[renderPassIndex].Name.c_str(); }
#endif

public:
            FrameGraph( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVfs );
            FrameGraph( FrameGraph& ) = default;
            FrameGraph& operator = ( FrameGraph& ) = default;
            ~FrameGraph();

    void    destroy( RenderDevice* renderDevice );
    void    enableProfiling( RenderDevice* renderDevice );

    // Spin until the pending frame is completed.
    // This is a blocking call.
    void    waitPendingFrameCompletion();
    
    void    execute( RenderDevice* renderDevice, const f32 deltaTime );

    void    submitAndDispatchDrawCmds( DrawCmd* drawCmds, const size_t drawCmdCount );
    void    setViewport( const Viewport& viewport, const ScissorRegion& scissorRegion, const CameraData* camera = nullptr );
    void    setMSAAQuality( const uint32_t samplerCount = 1 );
    void    setScreenSize( const dkVec2u& screenSize );

    // Update this application mouse coordinates (upload is deferred to next frame).
    void    updateMouseCoordinates( const dkVec2u& mouseCoordinates );

#ifdef DUSKED
    // (Editor Only) Submit Material Editor data for the current frame (will create a snapshot for the rendering thread).
    void    acquireCurrentMaterialEdData( const MaterialEdData* matEdData );
#endif

    // imageQuality = ( image Quality In Percent / 100.0f )
    // (e.g. 1.0f = 100% image quality)
    void    setImageQuality( const f32 imageQuality = 1.0f );

    // Return the current image quality of this pipeline.
    f32     getImageQuality() const;

    // Return the number of sample used for MSAA or 1 if MSAA is disabled/unused.
    u32     getMSAASamplerCount() const;

    // Return a pointer to the active camera data. Might be null if the graph is compute-only.
    // NOTE The pointer returned is ONLY guaranteed to be valid on the logic thread! (e.g. during renderpass setup)
    // Use FrameGraph resources getter otherwise.
    const CameraData* getActiveCameraData() const;

    void    importPersistentImage( const dkStringHash_t resourceHashcode, Image* image );
    void    importPersistentBuffer( const dkStringHash_t resourceHashcode, Buffer* buffer );

    // Copy the input rendertarget and store it in a given persistent resource.
    void    savePresentRenderTarget( FGHandle inputRenderTarget );

    // Copy the input rendertarget and store it in a given persistent resource.
    void    saveLastFrameRenderTarget( FGHandle inputRenderTarget );

    // Copy the input rendertarget and store it in a given persistent resource.
    void    saveLastFrameSSRRenderTarget( FGHandle inputRenderTarget );

    // Add a renderpass to this framegraph. T should be the datatype used to forward resource handles (or misc data) from
    // the setup step to the execution step.
    template<typename T>
    T& addRenderPass( const char* name, dkPassSetup_t<T> setup, dkPassCallback_t<T> execute ) {
        static_assert( sizeof( T ) <= sizeof( FGHandle ) * 128, "Pass data 128 resource limit hit!" );
        static_assert( sizeof( execute ) <= 1024 * 1024, "Execute lambda should be < 1ko!" );

        DUSK_RAISE_FATAL_ERROR( renderPassCount < FrameGraphBuilder::MAX_RENDER_PASS_COUNT, "RenderPass limit reached!" );

        FrameGraphRenderPass::Handle_t renderPassHandle = renderPassCount;
        
        FrameGraphRenderPass& renderPass = renderPasses[renderPassHandle];
        memset( &renderPass, 0, sizeof( FrameGraphRenderPass ) );

        renderPassCount++;

        T& passData = *( T* )renderPass.Data;

        graphBuilder.addRenderPass();
        setup( graphBuilder, passData );

        renderPass.Execute = std::bind(
            execute,
            passData,
            &graphResources,
            std::placeholders::_1,
            std::placeholders::_2
        );
        renderPass.Name = name;
        renderPass.Handle = renderPassHandle;

        return passData;
    }

#if DUSK_DEVBUILD
    const char* getProfilingSummary() const;

    // Return an estimation of the memory used by the buffer allocated by the graph.
    // Please note that this is an estimation which doesn't take in account hidden cost
    // (e.g. internal double buffering, DMA allocations, etc.).
    u32 getBufferMemoryUsage() const;
#endif

    // Return a const pointer to presentRenderTarget. Required if the viewport is different from the regular client one
    // (e.g. imgui editor viewport).
    Image*  getPresentRenderTarget() const;

private:
    // The memory allocator owning this instance.
    BaseAllocator*                      memoryAllocator;

    // Array of RenderPasses enqueued for this graph.
    FrameGraphRenderPass                renderPasses[FrameGraphBuilder::MAX_RENDER_PASS_COUNT];

    // Number of RenderPass stored in the renderPasses array.
    i32                                 renderPassCount;

    // The active SSAA factor applied for this graph.
    f32                                 pipelineImageQuality;

    // The size of the client screen used by this graph.
    dkVec2u                             graphScreenSize;

    // The number of sample used by any renderpass using MSAA (i.e. any render pass using
    // the graphics pipeline).
    u32                                 msaaSamplerCount;

    // A pointer to the main camera used by this graph.
    const CameraData*                   activeCamera;

    // The default viewport used by this graph.
    Viewport                            activeViewport;

    // True if the viewport has changed since the last execution of the graph.
    bool                                hasViewportChanged;

    // The local copy for PerView data forwarding (avoids explicit synchronization).
    PerViewBufferData                   perViewData;

#if DUSKED
    // The local copy for MaterialEd data forwarding (avoids explicit synchronization).
    MaterialEdData                      localMaterialEdData;
#endif

    // Last Frame (pre post-fx) render target. This is a persistent resource valid across the frames. There is no guarantee
    // of the content correctness.
    Image*                              lastFrameRenderTarget;

    // Last Frame SSR post resolve render target.
    Image*                              ssrLastFrameRenderTarget;

    // Current frame (post post-fx) render target. This is a persistent resource updated each frame.
    Image*                              presentRenderTarget;

    // The system responsible for resources caching/storing.
    FrameGraphResources                 graphResources;

	// The system responsible for resources allocation/render pass culling/etc...
    FrameGraphBuilder                   graphBuilder;

    // The system responsible for render passes scheduling.
    FrameGraphScheduler                 graphScheduler;

    GraphicsProfiler*                   graphicsProfiler;

private:
    // Destroy and re-create persistent resources for this graph. Can be used when the application context has changed
    // (e.g. screen resize, graphics quality changes, etc.).
    void                                recreatePersistentResources( RenderDevice* renderDevice );
};
