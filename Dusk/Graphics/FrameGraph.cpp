/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "FrameGraph.h"

#include "WorldRenderer.h"
#include "PipelineStateCache.h"

#include <Rendering/RenderDevice.h>
#include <Maths/MatrixTransformations.h>

#include <Graphics/RenderModules/Generated/BuiltIn.generated.h>

static constexpr dkStringHash_t SWAPCHAIN_BUFFER_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__SwapchainBuffer__" );
static constexpr dkStringHash_t PERVIEW_BUFFER_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__PerViewBuffer__" );
static constexpr dkStringHash_t PRESENT_IMAGE_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__PresentRenderTarget__" );
static constexpr dkStringHash_t LAST_FRAME_IMAGE_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__LastFrameRenderTarget__" );
static constexpr dkStringHash_t SSR_LAST_FRAME_IMAGE_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__SSR_LastFrameRenderTarget__" ); // TODO Should be per module allocable...
static constexpr dkStringHash_t MATERIALED_BUFFER_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__MaterialEditorBuffer__" );
static constexpr dkStringHash_t VECTORDATA_BUFFER_RESOURCE_HASHCODE = DUSK_STRING_HASH( "__VectorDataBuffer__" );

static constexpr size_t MAX_VECTOR_PER_INSTANCE = 1024;
static constexpr size_t VECTOR_BUFFER_SIZE = MAX_VECTOR_PER_INSTANCE * sizeof( dkVec4f );

const FGHandle FGHandle::Invalid = FGHandle( ~0 );

FrameGraphBuilder::FrameGraphBuilder()
    : frameSamplerCount( 1 )
    , frameImageQuality( 1.0f )
    , frameScreenSize( dkVec2u::Zero )
    , renderPassCount( 0 )
    , imageCount( 0 )
    , bufferCount( 0 )
    , persitentBufferCount( 0 )
    , persitentImageCount( 0 )
    , samplerStateCount( 0 )
{
    memset( &frameViewport, 0, sizeof( Viewport ) );
    memset( passRefs, 0, sizeof( PassInfos ) * MAX_RENDER_PASS_COUNT );
    memset( images, 0, sizeof( ImageAllocInfo ) * MAX_RESOURCES_HANDLE_PER_FRAME );
    memset( buffers, 0, sizeof( BufferAllocInfo ) * MAX_RESOURCES_HANDLE_PER_FRAME );
    memset( samplers, 0, sizeof( SamplerDesc ) * MAX_RESOURCES_HANDLE_PER_FRAME );
    memset( persitentBuffers, 0, sizeof( dkStringHash_t ) * MAX_RESOURCES_HANDLE_PER_FRAME );
    memset( persitentImages, 0, sizeof( dkStringHash_t ) * MAX_RESOURCES_HANDLE_PER_FRAME );
}

FrameGraphBuilder::~FrameGraphBuilder()
{

}

void FrameGraphBuilder::compile( RenderDevice* renderDevice, FrameGraphResources& resources )
{
    resources.unacquireResources();

    for ( u32 i = 0; i < imageCount; i++ ) {
        ImageAllocInfo& resToAlloc = images[i];
      /*  if ( resToAlloc.referenceCount == 0 ) {
            continue;
        }*/

        resources.allocateImage( renderDevice, i, resToAlloc.description, resToAlloc.flags );
    }

    for ( u32 i = 0; i < bufferCount; i++ ) {
        BufferAllocInfo& resToAlloc = buffers[i];
        resources.allocateBuffer( renderDevice, i, resToAlloc.description );
    }

    for ( u32 i = 0; i < samplerStateCount; i++ ) {
        SamplerDesc& resToAlloc = samplers[i];
        resources.allocateSampler( renderDevice, i, resToAlloc );
    }

    for ( u32 i = 0; i < persitentBufferCount; i++ ) {
        resources.bindPersistentBuffers( i, persitentBuffers[i] );
    }

    for ( u32 i = 0; i < persitentImageCount; i++ ) {
        resources.bindPersistentImages( i, persitentImages[i] );
    }

    renderPassCount = 0;
    imageCount = 0;
    bufferCount = 0;
    samplerStateCount = 0;
    persitentBufferCount = 0;
    persitentImageCount = 0;
}

void FrameGraphBuilder::cullRenderPasses( FrameGraphRenderPass* renderPassList, i32& renderPassCount )
{
    i32 tmpRenderPassCount = 0;

    for ( int32_t i = 0; i < renderPassCount; i++ ) {
        PassInfos& passInfos = passRefs[i];

        bool cullPass = true;

        if ( passInfos.isUncullable ) {
            cullPass = false;
        } else {
            for ( u32 j = 0; j < passInfos.imageCount; j++ ) {
                if ( images[passInfos.imageHandles[j]].referenceCount > 0 ) {
                    cullPass = false;
                    break;
                }
            }

            for ( u32 j = 0; j < passInfos.buffersCount; j++ ) {
                if ( buffers[passInfos.bufferHandles[j]].referenceCount > 0 ) {
                    cullPass = false;
                    break;
                }
            }
        }

        if ( !cullPass ) {
            renderPassList[tmpRenderPassCount++] = renderPassList[i];
        }
    }

    renderPassCount = tmpRenderPassCount;
}

void FrameGraphBuilder::scheduleRenderPasses( FrameGraphScheduler& graphScheduler, FrameGraphRenderPass* renderPassList, const i32 renderPassCount )
{
    for ( i32 i = 0; i < renderPassCount; i++ ) {
        const FrameGraphRenderPass& renderPass = renderPassList[i];
        const PassInfos& passInfo = passRefs[renderPass.Handle];

        if ( passInfo.useAsyncCompute ) {
            graphScheduler.addAsyncComputeRenderPass( renderPass, passInfo.dependencies, passInfo.dependencyCount );
        } else {
            graphScheduler.addRenderPass( renderPass, passInfo.dependencies, passInfo.dependencyCount );
        }
    }
}

void FrameGraphBuilder::addRenderPass()
{
    passRefs[renderPassCount].imageCount = 0;
    passRefs[renderPassCount].buffersCount = 0;
    passRefs[renderPassCount].isUncullable = false;
    passRefs[renderPassCount].useAsyncCompute = false;
    passRefs[renderPassCount].dependencyCount = 0;
    
    memset( passRefs[renderPassCount].dependencies, 0xff, sizeof( FrameGraphRenderPass::Handle_t ) * 12 );

    renderPassCount++;
}

void FrameGraphBuilder::ApplyImageDescriptionFlags( ImageDesc& description, const u32 imageFlags ) const
{
    if ( imageFlags & FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE ) {
        description.width = static_cast< u32 >( frameViewport.Width );
        description.height = static_cast< u32 >( frameViewport.Height );
    } else if ( imageFlags & FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS ) {
        description.width = static_cast< u32 >( frameViewport.Width * frameImageQuality );
        description.height = static_cast< u32 >( frameViewport.Height * frameImageQuality );
    } else if ( imageFlags & FrameGraphBuilder::eImageFlags::USE_SCREEN_SIZE ) {
        description.width = frameScreenSize.x;
        description.height = frameScreenSize.y;
    }

    if ( imageFlags & FrameGraphBuilder::eImageFlags::USE_PIPELINE_SAMPLER_COUNT ) {
        description.samplerCount = frameSamplerCount;
    }

    if ( imageFlags & FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE ) {
        description.samplerCount = 1;
    }
}

FGHandle FrameGraphBuilder::allocateImage( ImageDesc& description, const u32 imageFlags )
{
    DUSK_RAISE_FATAL_ERROR( imageCount + 1 < MAX_RESOURCES_HANDLE_PER_FRAME, "Limit reached!" );

    const FrameGraphRenderPass::Handle_t requesterHandle = ( renderPassCount - 1 );

    ApplyImageDescriptionFlags( description, imageFlags );

    images[imageCount].description = description;
    images[imageCount].flags = imageFlags;
    images[imageCount].referenceCount = 0u;
    images[imageCount].requestSource = requesterHandle;

    PassInfos& passInfos = passRefs[requesterHandle];
    passInfos.imageHandles[passInfos.imageCount++] = imageCount;

    return imageCount++;
}

FGHandle FrameGraphBuilder::copyImage( const FGHandle resourceToCopy, ImageDesc** descRef, const u32 imageFlags )
{
    images[imageCount].description = images[resourceToCopy].description;
    images[imageCount].flags = 0u;
    images[imageCount].referenceCount = 0u;
    images[imageCount].requestSource = ( renderPassCount - 1 );

    ApplyImageDescriptionFlags( images[imageCount].description, imageFlags );

    if ( descRef != nullptr ) {
        *descRef = &images[imageCount].description;
    }

    PassInfos& passInfos = passRefs[( renderPassCount - 1 )];
    passInfos.imageHandles[passInfos.imageCount++] = imageCount;

    return imageCount++;
}

#if DUSKED
void FrameGraphBuilder::fillBufferEditorInfos( std::vector<FGBufferInfosEditor>& bufferInfos ) const
{
    bufferInfos.resize( bufferCount );

    for ( u32 i = 0u; i < bufferCount; i++ ) {
        FGBufferInfosEditor& infos = bufferInfos[i];
        infos.Description = buffers[i].description;
        infos.ReferenceCount = buffers[i].referenceCount;
        infos.ShaderStageBinding = buffers[i].shaderStageBinding;
    }
}

void FrameGraphBuilder::fillImageEditorInfos( std::vector<FGImageInfosEditor>& imageInfos, const FrameGraphResources* graphResources ) const
{
    imageInfos.resize( imageCount );

	for ( u32 i = 0u; i < imageCount; i++ ) {
		FGImageInfosEditor& infos = imageInfos[i];
		infos.Description = images[i].description;
		infos.ReferenceCount = images[i].referenceCount;
		infos.GraphFlags = images[i].flags;

        if ( graphResources != nullptr ) {
            infos.AllocatedImage = graphResources->getImage( FGHandle( i ) );
        }
	}
}
#endif

FGHandle FrameGraphBuilder::allocateBuffer( BufferDesc& description, const u32 shaderStageBinding )
{
    const FrameGraphRenderPass::Handle_t requesterHandle = ( renderPassCount - 1 );

    buffers[bufferCount].shaderStageBinding = shaderStageBinding;
    buffers[bufferCount].referenceCount = 0u;
    buffers[bufferCount].requestSource = requesterHandle;
    buffers[bufferCount].description = description;

    PassInfos& passInfos = passRefs[requesterHandle];
    passInfos.bufferHandles[passInfos.buffersCount++] = bufferCount;

    return bufferCount++;
}

FGHandle FrameGraphBuilder::allocateSampler( const SamplerDesc& description )
{
    samplers[samplerStateCount] = description;
    return samplerStateCount++;
}

FGHandle FrameGraphBuilder::readReadOnlyImage( const FGHandle resourceHandle )
{
    images[resourceHandle].referenceCount++;
    return resourceHandle;
}

FGHandle FrameGraphBuilder::readReadOnlyBuffer( const FGHandle resourceHandle )
{
    buffers[resourceHandle].referenceCount++;
    return resourceHandle;
}

void FrameGraphBuilder::updatePassDependency( PassInfos& passInfos, const FrameGraphRenderPass::Handle_t dependency )
{
    for ( u32 i = 0; i < passInfos.dependencyCount; i++ ) {
        if ( passInfos.dependencies[i] == dependency ) {
            return;
        }
    }

    // If the dependency does not exist yet, add it
    passInfos.dependencies[passInfos.dependencyCount] = dependency;
    passInfos.dependencyCount++;
}

FGHandle FrameGraphBuilder::readImage( const FGHandle resourceHandle )
{
    images[resourceHandle].referenceCount++;

    ImageAllocInfo& imageResource = images[resourceHandle];

    updatePassDependency( passRefs[( renderPassCount - 1 )], imageResource.requestSource );
    imageResource.requestSource = ( renderPassCount - 1 );

    return resourceHandle;
}

FGHandle FrameGraphBuilder::readBuffer( const FGHandle resourceHandle )
{
    buffers[resourceHandle].referenceCount++;

    BufferAllocInfo& bufferResource = buffers[resourceHandle];

    updatePassDependency( passRefs[( renderPassCount - 1 )], bufferResource.requestSource );
    bufferResource.requestSource = ( renderPassCount - 1 );

    return resourceHandle;
}

FGHandle FrameGraphBuilder::retrieveSwapchainBuffer()
{
    persitentImages[persitentImageCount] = SWAPCHAIN_BUFFER_RESOURCE_HASHCODE;
    return persitentImageCount++;
}

FGHandle FrameGraphBuilder::retrievePresentImage()
{
    persitentImages[persitentImageCount] = PRESENT_IMAGE_RESOURCE_HASHCODE;
    return persitentImageCount++;
}

FGHandle FrameGraphBuilder::retrieveLastFrameImage()
{
    persitentImages[persitentImageCount] = LAST_FRAME_IMAGE_RESOURCE_HASHCODE;
    return persitentImageCount++;
}

FGHandle FrameGraphBuilder::retrieveSSRLastFrameImage()
{
    persitentImages[persitentImageCount] = SSR_LAST_FRAME_IMAGE_RESOURCE_HASHCODE;
    return persitentImageCount++;
}

FGHandle FrameGraphBuilder::retrievePerViewBuffer()
{
    persitentBuffers[persitentBufferCount] = PERVIEW_BUFFER_RESOURCE_HASHCODE;
    return persitentBufferCount++;
}

FGHandle FrameGraphBuilder::retrieveMaterialEdBuffer()
{
    persitentBuffers[persitentBufferCount] = MATERIALED_BUFFER_RESOURCE_HASHCODE;
    return persitentBufferCount++;
}

FGHandle FrameGraphBuilder::retrieveVectorDataBuffer()
{
	persitentBuffers[persitentBufferCount] = VECTORDATA_BUFFER_RESOURCE_HASHCODE;
	return persitentBufferCount++;
}

FGHandle FrameGraphBuilder::retrievePersistentImage( const dkStringHash_t resourceHashcode )
{
    persitentImages[persitentImageCount] = resourceHashcode;

    // NOTE If a render pass uses a persistent resource, it implicitly become uncullable (since persistent resources
    // have no reference tracking)
    setUncullablePass();

    return persitentImageCount++;
}

FGHandle FrameGraphBuilder::retrievePersistentBuffer( const dkStringHash_t resourceHashcode )
{
    persitentBuffers[persitentBufferCount] = resourceHashcode;

    // NOTE If a render pass uses a persistent resource, it implicitly become uncullable (since persistent resources
    // have no reference tracking)
    setUncullablePass();

    return persitentBufferCount++;
}

FrameGraphResources::FrameGraphResources( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , pipelineImageQuality( 1.0f )
    , deltaTime( 0.0f )
    , allocatedBuffersCount( 0 )
	, allocatedImageCount( 0 )
	, allocatedSamplerCount( 0 )
    , activeScreenSize( dkVec2u::Zero )
    , instanceBufferData( nullptr )
{
    memset( drawCmdBuckets, 0, sizeof( DrawCmdBucket ) * 3 * 4 );
    memset( &activeCameraData, 0, sizeof( CameraData ) );
    memset( &activeViewport, 0, sizeof( Viewport ) );

    memset( allocatedBuffers, 0, sizeof( Buffer* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
    memset( buffersDesc, 0, sizeof( BufferDesc ) * MAX_ALLOCABLE_RESOURCE_TYPE );
    memset( isBufferFree, 0, sizeof( bool ) * MAX_ALLOCABLE_RESOURCE_TYPE );

    memset( allocatedImages, 0, sizeof( Image* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
    memset( imagesDesc, 0, sizeof( ImageDesc ) * MAX_ALLOCABLE_RESOURCE_TYPE );
    memset( isImageFree, 0, sizeof( bool ) * MAX_ALLOCABLE_RESOURCE_TYPE );

	memset( allocatedSamplers, 0, sizeof( Sampler* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
	memset( samplerDesc, 0, sizeof( ImageDesc ) * MAX_ALLOCABLE_RESOURCE_TYPE );
	memset( isSamplerFree, 0, sizeof( bool ) * MAX_ALLOCABLE_RESOURCE_TYPE );

    memset( inUseBuffers, 0, sizeof( Buffer* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
	memset( inUseImages, 0, sizeof( Image* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
	memset( inUseSamplers, 0, sizeof( Sampler* ) * MAX_ALLOCABLE_RESOURCE_TYPE );

    memset( persistentBuffers, 0, sizeof( Buffer* ) * MAX_ALLOCABLE_RESOURCE_TYPE );
    memset( persistentImages, 0, sizeof( Image* ) * MAX_ALLOCABLE_RESOURCE_TYPE );

    instanceBufferData = dk::core::allocateArray<uint8_t>( allocator, VECTOR_BUFFER_SIZE );
    memset( instanceBufferData, 0, VECTOR_BUFFER_SIZE );
}

FrameGraphResources::~FrameGraphResources()
{
    dk::core::freeArray<u8>( memoryAllocator, static_cast<u8*>( instanceBufferData ) );
}

void FrameGraphResources::releaseResources( RenderDevice* renderDevice )
{
    for ( i32 bufferIdx = 0; bufferIdx < allocatedBuffersCount; bufferIdx++ ) {
        renderDevice->destroyBuffer( allocatedBuffers[bufferIdx] );
    }

    for ( i32 imageIdx = 0; imageIdx < allocatedImageCount; imageIdx++ ) {
        renderDevice->destroyImage( allocatedImages[imageIdx] );
	}

	for ( i32 samplerIdx = 0; samplerIdx < allocatedSamplerCount; samplerIdx++ ) {
		renderDevice->destroySampler( allocatedSamplers[samplerIdx] );
	}
}

void FrameGraphResources::unacquireResources()
{
    memset( isBufferFree, 1, sizeof( bool ) * static_cast<u32>( allocatedBuffersCount ) );
	memset( isImageFree, 1, sizeof( bool ) * static_cast< u32 >( allocatedImageCount ) );
	memset( isSamplerFree, 1, sizeof( bool ) * static_cast< u32 >( allocatedSamplerCount ) );
}

void FrameGraphResources::setPipelineViewport( const Viewport& viewport, const ScissorRegion& scissor, const CameraData* cameraData )
{
    activeCameraData = *cameraData;
    activeViewport = viewport;
    activeScissor = scissor;
}

void FrameGraphResources::setImageQuality( const f32 imageQuality )
{
    pipelineImageQuality = imageQuality;
}

// Copy instance data to shared buffer
void FrameGraphResources::updateVectorBuffer( const DrawCmd& cmd, size_t& instanceBufferOffset )
{
    switch ( cmd.key.bitfield.layer ) {
    case DrawCommandKey::LAYER_DEPTH: {
       /* const size_t instancesDataSize = sizeof( DrawCommandInfos::InstanceData ) * cmd.infos.instanceCount;

        dkMat4x4f modelViewProjection = *cmd.infos.modelMatrix * activeCameraData.shadowViewMatrix[cmd.key.bitfield.viewportLayer - 1u];
        memcpy( reinterpret_cast<u8*>( instanceBufferData ) + instanceBufferOffset, &modelViewProjection, sizeof( dkMat4x4f ) );

        instanceBufferOffset += instancesDataSize;*/
    } break;

    case DrawCommandKey::LAYER_WORLD:
    default: {
        const size_t instancesDataSize = sizeof( DrawCommandInfos::InstanceData ) * cmd.infos.instanceCount;

        memcpy( reinterpret_cast<u8*>( instanceBufferData ) + instanceBufferOffset, cmd.infos.modelMatrix, instancesDataSize );

        instanceBufferOffset += instancesDataSize;
    } break;
    }
}

void FrameGraphResources::dispatchToBuckets( DrawCmd* drawCmds, const size_t drawCmdCount )
{
    memset( &drawCmdBuckets, 0, sizeof( DrawCmdBucket ) * 4 * 8 );

    if ( drawCmdCount == 0 ) {
        return;
    }

    const auto& firstDrawCmdKey = drawCmds[0].key.bitfield;

    DrawCommandKey::Layer layer = firstDrawCmdKey.layer;
    uint8_t viewportLayer = firstDrawCmdKey.viewportLayer;

    drawCmdBuckets[layer][viewportLayer].beginAddr = ( drawCmds + 0 );

    size_t instanceBufferOffset = 0ull;

    updateVectorBuffer( drawCmds[0], instanceBufferOffset );

    DrawCmdBucket * previousBucket = &drawCmdBuckets[layer][viewportLayer];
    previousBucket->instanceDataStartOffset = 0.0f;
    previousBucket->vectorPerInstance = static_cast< float >( sizeof( DrawCommandInfos::InstanceData ) / sizeof( dkVec4f ) );

    for ( size_t drawCmdIdx = 1; drawCmdIdx < drawCmdCount; drawCmdIdx++ ) {
        const auto& drawCmdKey = drawCmds[drawCmdIdx].key.bitfield;

        if ( layer != drawCmdKey.layer || viewportLayer != drawCmdKey.viewportLayer ) {
            previousBucket->endAddr = ( drawCmds + drawCmdIdx );

            auto & bucket = drawCmdBuckets[drawCmdKey.layer][drawCmdKey.viewportLayer];
            bucket.beginAddr = ( drawCmds + drawCmdIdx );
            bucket.vectorPerInstance = static_cast< float >( sizeof( DrawCommandInfos::InstanceData ) / sizeof( dkVec4f ) );
            bucket.instanceDataStartOffset = static_cast< float >( instanceBufferOffset / sizeof( dkVec4f ) );

            layer = drawCmdKey.layer;
            viewportLayer = drawCmdKey.viewportLayer;

            previousBucket = &drawCmdBuckets[drawCmdKey.layer][drawCmdKey.viewportLayer];
        }

        updateVectorBuffer( drawCmds[drawCmdIdx], instanceBufferOffset );
    }

    previousBucket->endAddr = ( drawCmds + drawCmdCount );
}

void FrameGraphResources::importPersistentImage( const dkStringHash_t resourceHashcode, Image* image )
{
    persistentImagesMap[resourceHashcode] = image;
}

void FrameGraphResources::importPersistentBuffer( const dkStringHash_t resourceHashcode, Buffer * buffer )
{
    persistentBuffersMap[resourceHashcode] = buffer;
}

const FrameGraphResources::DrawCmdBucket& FrameGraphResources::getDrawCmdBucket( const DrawCommandKey::Layer layer, const uint8_t viewportLayer ) const
{
    return drawCmdBuckets[layer][viewportLayer];
}

void* FrameGraphResources::getVectorBufferData() const
{
    return instanceBufferData;
}

const CameraData* FrameGraphResources::getMainCamera() const
{
    return &activeCameraData;
}

const Viewport* FrameGraphResources::getMainViewport() const
{
    return &activeViewport;
}

const ScissorRegion* FrameGraphResources::getMainScissorRegion() const
{
    return &activeScissor;
}

void FrameGraphResources::setScreenSize( const dkVec2u& screenSize )
{
    activeScreenSize = screenSize;
}

const dkVec2u& FrameGraphResources::getScreenSize() const
{
    return activeScreenSize;
}

void FrameGraphResources::updateDeltaTime( const float dt )
{
    deltaTime = dt;
}

f32 FrameGraphResources::getDeltaTime() const
{
    return deltaTime;
}

Buffer* FrameGraphResources::getBuffer( const FGHandle resourceHandle ) const
{
    return inUseBuffers[resourceHandle];
}

Image* FrameGraphResources::getImage( const FGHandle resourceHandle ) const
{
    return inUseImages[resourceHandle];
}

Sampler* FrameGraphResources::getSampler( const FGHandle resourceHandle ) const
{
    return inUseSamplers[resourceHandle];
}

Buffer* FrameGraphResources::getPersistentBuffer( const FGHandle resourceHandle ) const
{
    return persistentBuffers[resourceHandle];
}

Image* FrameGraphResources::getPersitentImage( const FGHandle resourceHandle ) const
{
    return persistentImages[resourceHandle];
}

void FrameGraphResources::allocateBuffer( RenderDevice* renderDevice, const FGHandle resourceHandle, const BufferDesc& description )
{
    Buffer* buffer = nullptr;

    for ( int i = 0; i < allocatedBuffersCount; i++ ) {
        if ( buffersDesc[i] == description && isBufferFree[i] ) {
            buffer = allocatedBuffers[i];
            isBufferFree[i] = false;
            break;
        }
    }

    if ( buffer == nullptr ) {
        buffer = renderDevice->createBuffer( description );
        allocatedBuffers[allocatedBuffersCount] = buffer;
        buffersDesc[allocatedBuffersCount] = description;
        allocatedBuffersCount++;
    }

    inUseBuffers[resourceHandle] = buffer;
}

void FrameGraphResources::allocateImage( RenderDevice* renderDevice, const FGHandle resourceHandle, const ImageDesc& description, const u32 flags )
{
    Image* image = nullptr;

    for ( i32 i = 0; i < allocatedImageCount; i++ ) {
        if ( imagesDesc[i] == description && isImageFree[i] ) {
            image = allocatedImages[i];
            isImageFree[i] = false;
            break;
        }
    }

    if ( image == nullptr ) {
        image = renderDevice->createImage( description );
        allocatedImages[allocatedImageCount] = image;
        imagesDesc[allocatedImageCount] = description;

        if ( flags & FrameGraphBuilder::eImageFlags::REQUEST_PER_MIP_RESOURCE_VIEW ) {
            // Negative or null mip count means that the mip count must be automatically computed.
            if ( description.mipCount <= 0 ) {
                const u32 mipCount = ImageDesc::GetMipCount( description );

                for ( u32 mipIdx = 0u; mipIdx < mipCount; mipIdx++ ) {
                    ImageViewDesc mipView;
                    mipView.MipCount = 1;
                    mipView.StartMipIndex = mipIdx;
                    renderDevice->createImageView( *image, mipView, IMAGE_VIEW_CREATE_SRV );
                }
            } else {
                ImageViewDesc dummyView;
                dummyView.MipCount = 1;
                renderDevice->createImageView( *image, dummyView, IMAGE_VIEW_CREATE_SRV | IMAGE_VIEW_COVER_WHOLE_MIPCHAIN );
            }
        }

        allocatedImageCount++;
    }

    inUseImages[resourceHandle] = image;
}

void FrameGraphResources::allocateSampler( RenderDevice* renderDevice, const FGHandle resourceHandle, const SamplerDesc& description )
{
    Sampler* sampler = nullptr;

    for ( i32 i = 0; i < allocatedSamplerCount; i++ ) {
        if ( samplerDesc[i] == description && isSamplerFree[i] ) {
            sampler = allocatedSamplers[i];
            isSamplerFree[i] = false;
            break;
        }
    }

    if ( sampler == nullptr ) {
        sampler = renderDevice->createSampler( description );
        allocatedSamplers[allocatedSamplerCount] = sampler;
        samplerDesc[allocatedSamplerCount] = description;
        allocatedSamplerCount++;
    }

    inUseSamplers[resourceHandle] = sampler;
}

void FrameGraphResources::bindPersistentBuffers( const FGHandle resourceHandle, const dkStringHash_t hashcode )
{
    auto it = persistentBuffersMap.find( hashcode );
    persistentBuffers[resourceHandle] = ( it != persistentBuffersMap.end() ) ? it->second : nullptr;
}

void FrameGraphResources::bindPersistentImages( const FGHandle resourceHandle, const dkStringHash_t hashcode )
{
    auto it = persistentImagesMap.find( hashcode );
    persistentImages[resourceHandle] = ( it != persistentImagesMap.end() ) ? it->second : nullptr;
}

bool FrameGraphResources::isPersistentImageAvailable( const dkStringHash_t resourceHashcode ) const
{
    return ( persistentImagesMap.find( resourceHashcode ) != persistentImagesMap.end() );
}

bool FrameGraphResources::isPersistentBufferAvailable( const dkStringHash_t resourceHashcode ) const
{
    return ( persistentBuffersMap.find( resourceHashcode ) != persistentBuffersMap.end() );
}

FrameGraph::FrameGraph( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVfs )
    : memoryAllocator( allocator )
    , renderPassCount( 0 )
    , pipelineImageQuality( 1.0f )
    , graphScreenSize( dkVec2u::Zero )
    , msaaSamplerCount( 0 )
    , activeCamera( nullptr )
    , hasViewportChanged( false )
    , lastFrameRenderTarget( nullptr )
    , ssrLastFrameRenderTarget( nullptr )
    , presentRenderTarget( nullptr )
    , graphResources( allocator )
    , graphBuilder()
    , graphScheduler( allocator, activeRenderDevice, activeVfs )
    , graphicsProfiler( nullptr )
{
    memset( renderPasses, 0, sizeof( FrameGraphRenderPass ) * 48 );
    memset( &activeViewport, 0, sizeof( Viewport ) );

    // Setup PerView Buffer (persistent buffer shared between workers)
    perViewData.ViewProjectionMatrix = dkMat4x4f::Identity;
    perViewData.InverseViewProjectionMatrix = dkMat4x4f::Identity;
    perViewData.PreviousViewProjectionMatrix = dkMat4x4f::Identity;
	perViewData.OrthoProjectionMatrix = dkMat4x4f::Identity;
	perViewData.ViewMatrix = dkMat4x4f::Identity;
	perViewData.ProjectionMatrix = dkMat4x4f::Identity;
	perViewData.ProjectionMatrixFiniteFar = dkMat4x4f::Identity;
	perViewData.InverseProjectionMatrix = dkMat4x4f::Identity;
    perViewData.InverseViewMatrix = dkMat4x4f::Identity;
    perViewData.ScreenToWorldMatrix = dkMat4x4f::Identity;
    perViewData.ViewportSize = dkVec2f( 0.0f, 0.0f );
    perViewData.InverseViewportSize = dkVec2f( 0.0f, 0.0f );
    perViewData.WorldPosition = dkVec3f( 0.0f, 0.0f, 0.0f );
    perViewData.FrameIndex = 0;
    perViewData.CameraJitteringOffset = dkVec2f( 0.0f, 0.0f );
	perViewData.ImageQuality = 1.0f;
	perViewData.ViewDirection = dkVec3f( 0.0f, 0.0f, 0.0f );
	perViewData.MouseCoordinates = dkVec2u( 0, 0 );
	perViewData.UpVector = dkVec3f( 0.0f, 1.0f, 0.0f );
    perViewData.FieldOfView = 90.0f;
	perViewData.RightVector = dkVec3f( 0.0f, 0.0f, 1.0f );
	perViewData.AspectRatio = 1.0f;
	perViewData.NearPlane = 0.0f;
	perViewData.FarPlane = 100.0f;
}

FrameGraph::~FrameGraph()
{

}

void FrameGraph::destroy( RenderDevice* renderDevice )
{
    graphResources.releaseResources( renderDevice );

    if ( graphicsProfiler != nullptr ) {
        //graphicsProfiler->destroy( renderDevice );
        //dk::core::free<GraphicsProfiler>( memoryAllocator, graphicsProfiler );
    }

    if ( lastFrameRenderTarget != nullptr ) {
        renderDevice->destroyImage( lastFrameRenderTarget );
    }

    if ( ssrLastFrameRenderTarget != nullptr ) {
        renderDevice->destroyImage( ssrLastFrameRenderTarget );
    }

    if ( presentRenderTarget != nullptr ) {
        renderDevice->destroyImage( presentRenderTarget );
    }
}

void FrameGraph::enableProfiling( RenderDevice* renderDevice )
{
    DUSK_UNUSED_VARIABLE( renderDevice );

    // graphicsProfiler = dk::core::allocate<GraphicsProfiler>( memoryAllocator );
//    graphicsProfiler->create( renderDevice );
}

void FrameGraph::waitPendingFrameCompletion()
{
    DUSK_CPU_PROFILE_FUNCTION;
    
    while ( !graphScheduler.isReady() ) {
        std::this_thread::yield();
    }
}

void FrameGraph::execute( RenderDevice* renderDevice, const f32 deltaTime )
{
    // Check if we need to reallocate persistent resources
    if ( hasViewportChanged ) {
        recreatePersistentResources( renderDevice );
        hasViewportChanged = false;
    }

    // Update per-frame renderer infos
    graphResources.updateDeltaTime( deltaTime );
    graphResources.importPersistentImage( SWAPCHAIN_BUFFER_RESOURCE_HASHCODE, renderDevice->getSwapchainBuffer() );
    graphResources.importPersistentBuffer( PERVIEW_BUFFER_RESOURCE_HASHCODE, graphScheduler.getPerViewPersistentBuffer() );
    graphResources.importPersistentImage( LAST_FRAME_IMAGE_RESOURCE_HASHCODE, lastFrameRenderTarget );
    graphResources.importPersistentImage( SSR_LAST_FRAME_IMAGE_RESOURCE_HASHCODE, ssrLastFrameRenderTarget );
	graphResources.importPersistentImage( PRESENT_IMAGE_RESOURCE_HASHCODE, presentRenderTarget );
#if DUSKED
	graphResources.importPersistentBuffer( MATERIALED_BUFFER_RESOURCE_HASHCODE, graphScheduler.getMaterialEdPersistentBuffer() );
#endif
	graphResources.importPersistentBuffer( VECTORDATA_BUFFER_RESOURCE_HASHCODE, graphScheduler.getVectorDataBuffer() );
    
    // Cull & compile
    //graphBuilder.cullRenderPasses( renderPasses, renderPassCount );
    graphBuilder.compile( renderDevice, graphResources );
    
    // Schedule renderpass execution.
    graphBuilder.scheduleRenderPasses( graphScheduler, renderPasses, renderPassCount );

    // Update PerView Buffer
    if ( activeCamera != nullptr ) {
        perViewData.ViewProjectionMatrix = activeCamera->viewProjectionMatrix;
        perViewData.InverseViewProjectionMatrix = activeCamera->inverseViewProjectionMatrix;
        perViewData.PreviousViewProjectionMatrix = activeCamera->previousViewProjectionMatrix;
        perViewData.OrthoProjectionMatrix = dk::maths::MakeOrtho( 0.0f, activeCamera->viewportSize.x, activeCamera->viewportSize.y, 0.0f, -1.0f, 1.0f );
        perViewData.ViewMatrix = activeCamera->viewMatrix;
		perViewData.ProjectionMatrix = activeCamera->projectionMatrix;
		perViewData.ProjectionMatrixFiniteFar = activeCamera->finiteProjectionMatrix;
        perViewData.InverseProjectionMatrix = activeCamera->inverseFiniteProjectionMatrix;
        perViewData.InverseViewMatrix = activeCamera->inverseViewMatrix;
        perViewData.ViewportSize = activeCamera->viewportSize;
        perViewData.InverseViewportSize = activeCamera->inverseViewportSize;
        perViewData.WorldPosition = activeCamera->worldPosition;
        perViewData.FrameIndex = activeCamera->cameraFrameNumber;
        perViewData.CameraJitteringOffset = activeCamera->jitteringOffset;
		perViewData.ImageQuality = activeCamera->imageQuality;
        perViewData.ViewDirection = activeCamera->viewDirection;
        perViewData.UpVector = activeCamera->upVector;
		perViewData.FieldOfView = activeCamera->fov;
        perViewData.RightVector = activeCamera->rightVector;
        perViewData.AspectRatio = activeCamera->aspectRatio;
        perViewData.NearPlane = activeCamera->nearPlane;
        perViewData.FarPlane = activeCamera->farPlane;
    }
    

    // Make a matrix that converts from screen coordinates to clip coordinates.
    dkMat4x4f viewportMatrix = dk::maths::MakeOrtho(
        static_cast< f32 >( activeViewport.X ),
        static_cast< f32 >( activeViewport.X + activeViewport.Width ),
        static_cast< f32 >( activeViewport.Y ),
        static_cast< f32 >( activeViewport.Y + activeViewport.Height ),
        -1.0f,
        1.0f
    );

    // Setting column 2 (z-axis) to identity makes the matrix ignore the z-axis.
    dkMat4x4f viewProjMatrix = perViewData.ViewProjectionMatrix;
    //viewProjMatrix[0][2] = 0.0f;
    //viewProjMatrix[1][2] = 0.0f;
    //viewProjMatrix[2][2] = 1.0f;
    //viewProjMatrix[3][2] = 0.0f;

    perViewData.ScreenToWorldMatrix = /*viewportMatrix * */ viewProjMatrix.inverse();

#ifdef DUSKED
    graphScheduler.updateMaterialEdBuffer( &localMaterialEdData );
#endif

    // Dispatch render passes to rendering threads
    graphScheduler.dispatch( &perViewData, graphResources.getVectorBufferData() );

    renderPassCount = 0;
}

void FrameGraph::submitAndDispatchDrawCmds( DrawCmd* drawCmds, const size_t drawCmdCount )
{
    graphResources.dispatchToBuckets( drawCmds, drawCmdCount );
}

void FrameGraph::setViewport( const Viewport& viewport, const ScissorRegion& scissorRegion, const CameraData* camera )
{
    hasViewportChanged = ( activeViewport != viewport );
    activeCamera = camera;
    activeViewport = viewport;

    graphBuilder.setPipelineViewport( viewport );
    graphBuilder.setPipelineScissor( scissorRegion );
    
    if ( activeCamera != nullptr ) {
        graphResources.setPipelineViewport( viewport, scissorRegion, activeCamera );
    }
}

void FrameGraph::setMSAAQuality( const uint32_t samplerCount )
{
    graphBuilder.setMSAAQuality( samplerCount );

    msaaSamplerCount = samplerCount;
}

void FrameGraph::setScreenSize( const dkVec2u& screenSize )
{
    graphBuilder.setScreenSize( screenSize );
    graphResources.setScreenSize( screenSize );

    graphScreenSize = screenSize;
}

void FrameGraph::updateMouseCoordinates( const dkVec2u& mouseCoordinates )
{
    perViewData.MouseCoordinates = mouseCoordinates;
}

#ifdef DUSKED
void FrameGraph::acquireCurrentMaterialEdData( const MaterialEdData* matEdData )
{
    if ( matEdData == nullptr ) {
        return;
    }

    memcpy( &localMaterialEdData, matEdData, sizeof( MaterialEdData ) );
}
#endif

void FrameGraph::setImageQuality( const f32 imageQuality )
{
    graphBuilder.setImageQuality( imageQuality );
    graphResources.setImageQuality( imageQuality );

    pipelineImageQuality = imageQuality;
}

f32 FrameGraph::getImageQuality() const
{
    return pipelineImageQuality;
}

u32 FrameGraph::getMSAASamplerCount() const
{
    return msaaSamplerCount;
}

const CameraData* FrameGraph::getActiveCameraData() const
{
    return activeCamera;
}

void FrameGraph::importPersistentImage( const dkStringHash_t resourceHashcode, Image* image )
{
    graphResources.importPersistentImage( resourceHashcode, image );
}

void FrameGraph::importPersistentBuffer( const dkStringHash_t resourceHashcode, Buffer* buffer )
{
    graphResources.importPersistentBuffer( resourceHashcode, buffer );
}

enum class CopyImageTarget  
{
    LastFrameImage,
    PresentImage,
    SSRLastFrame
};

// Copy a FrameGraph image to a persistent image owned by the same FrameGraph (see CopyImageTarget or extend it
// if you need to).
template<CopyImageTarget OuputTarget>
DUSK_INLINE void AddCopyImagePass( FrameGraph& frameGraph, FGHandle inputRenderTarget )
{ 
    constexpr PipelineStateDesc DefaultPipelineStateDesc = PipelineStateDesc( 
        PipelineStateDesc::GRAPHICS,
        ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        DepthStencilStateDesc(),
        RasterizerStateDesc( CULL_MODE_BACK ),
        RenderingHelpers::BS_Disabled,
        FramebufferLayoutDesc( FramebufferLayoutDesc::AttachmentDesc( FramebufferLayoutDesc::WRITE_COLOR, FramebufferLayoutDesc::DONT_CARE, eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT ) ),
        { { RenderingHelpers::S_BilinearClampEdge }, 1 }
    );
        
    struct PassData {
        FGHandle inputImage;
        FGHandle outputImage;
    };

    PassData& downscaleData = frameGraph.addRenderPass<PassData>(
        "FrameGraph::copyAsPresentRenderTarget",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.setUncullablePass();

            passData.inputImage = builder.readReadOnlyImage( inputRenderTarget );

            switch ( OuputTarget ) {
            case CopyImageTarget::LastFrameImage:
                passData.outputImage = builder.retrieveLastFrameImage();
                break;
            case CopyImageTarget::PresentImage:
                passData.outputImage = builder.retrievePresentImage();
                break;
            case CopyImageTarget::SSRLastFrame:
                passData.outputImage = builder.retrieveSSRLastFrameImage();
                break;
            };
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* output = resources->getPersitentImage( passData.outputImage );
            Image* input = resources->getImage( passData.inputImage );

            PipelineState* pso = psoCache->getOrCreatePipelineState( DefaultPipelineStateDesc, BuiltIn::CopyImagePass_ShaderBinding );

            cmdList->pushEventMarker( BuiltIn::CopyImagePass_EventName );
            cmdList->bindPipelineState( pso );

            switch ( OuputTarget ) {
            case CopyImageTarget::LastFrameImage: {
                // Update viewport (using image quality scaling)
                const CameraData* camera = resources->getMainCamera();

                dkVec2f scaledViewportSize = camera->viewportSize * camera->imageQuality;

                Viewport vp;
                vp.X = 0;
                vp.Y = 0;
                vp.Width = static_cast< i32 >( scaledViewportSize.x );
                vp.Height = static_cast< i32 >( scaledViewportSize.y );
                vp.MinDepth = 0.0f;
                vp.MaxDepth = 1.0f;

                cmdList->setViewport( vp );
            } break;
            default:
                cmdList->setViewport( *resources->getMainViewport() );
                break;
            };

            // Bind resources
            cmdList->bindImage( BuiltIn::CopyImagePass_InputRenderTarget_Hashcode, input );

            cmdList->prepareAndBindResourceList();

            FramebufferAttachment fboAttachment( output );
            cmdList->setupFramebuffer( &fboAttachment );

            cmdList->draw( 3, 1 );

            cmdList->popEventMarker();
        }
    );
}

void FrameGraph::savePresentRenderTarget( FGHandle inputRenderTarget )
{
    AddCopyImagePass<CopyImageTarget::PresentImage>( *this, inputRenderTarget );
}

void FrameGraph::saveLastFrameRenderTarget( FGHandle inputRenderTarget )
{
    AddCopyImagePass<CopyImageTarget::LastFrameImage>( *this, inputRenderTarget );
}

void FrameGraph::saveLastFrameSSRRenderTarget( FGHandle inputRenderTarget )
{
    AddCopyImagePass<CopyImageTarget::SSRLastFrame>( *this, inputRenderTarget );
}

#if DUSK_DEVBUILD
const char* FrameGraph::getProfilingSummary() const
{
    return nullptr;
    /*const std::string& proflingString = graphicsProfiler->getProfilingSummaryString();
    return ( !proflingString.empty() ) ? proflingString.c_str() : nullptr;*/
}

u32 FrameGraph::getBufferMemoryUsage() const
{
    std::vector<FGBufferInfosEditor> bufferInfos;
    graphBuilder.fillBufferEditorInfos( bufferInfos );

    u32 totalBufferSize = 0u;
    for ( const FGBufferInfosEditor& infos : bufferInfos ) {
        totalBufferSize += infos.Description.SizeInBytes;
    }

    return totalBufferSize;
}
#endif

Image* FrameGraph::getPresentRenderTarget() const
{
    return presentRenderTarget;
}

void FrameGraph::recreatePersistentResources( RenderDevice* renderDevice )
{
    // Last Frame Render Target
    if ( lastFrameRenderTarget != nullptr ) {
        renderDevice->destroyImage( lastFrameRenderTarget );
    }

    ImageDesc lastFrameDesc = {};
    lastFrameDesc.dimension = ImageDesc::DIMENSION_2D;
    lastFrameDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
    lastFrameDesc.width = static_cast< uint32_t >( activeViewport.Width * pipelineImageQuality );
    lastFrameDesc.height = static_cast< uint32_t >( activeViewport.Height * pipelineImageQuality );
    lastFrameDesc.depth = 1;
    lastFrameDesc.mipCount = 1;
    lastFrameDesc.samplerCount = 1;
    lastFrameDesc.usage = RESOURCE_USAGE_DEFAULT;
    lastFrameDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE;

    lastFrameRenderTarget = renderDevice->createImage( lastFrameDesc );

    // Present Render Target
    if ( presentRenderTarget != nullptr ) {
        renderDevice->destroyImage( presentRenderTarget );
    }

    ImageDesc postPostFxDesc = {};
    postPostFxDesc.dimension = ImageDesc::DIMENSION_2D;
    postPostFxDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
    postPostFxDesc.width = static_cast< uint32_t >( activeViewport.Width );
    postPostFxDesc.height = static_cast< uint32_t >( activeViewport.Height );
    postPostFxDesc.depth = 1;
    postPostFxDesc.mipCount = 1;
    postPostFxDesc.samplerCount = 1;
    postPostFxDesc.usage = RESOURCE_USAGE_DEFAULT;
    postPostFxDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;

    presentRenderTarget = renderDevice->createImage( postPostFxDesc );

    // SSR Last Frame Render Target
    if ( ssrLastFrameRenderTarget != nullptr ) {
        renderDevice->destroyImage( ssrLastFrameRenderTarget );
    }

    ImageDesc ssrDesc = {};
    ssrDesc.dimension = ImageDesc::DIMENSION_2D;
    ssrDesc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_FLOAT;
    ssrDesc.width = static_cast< uint32_t >( activeViewport.Width );
    ssrDesc.height = static_cast< uint32_t >( activeViewport.Height );
    ssrDesc.depth = 1;
    ssrDesc.mipCount = 1;
    ssrDesc.samplerCount = 1;
    ssrDesc.usage = RESOURCE_USAGE_DEFAULT;
    ssrDesc.bindFlags = RESOURCE_BIND_RENDER_TARGET_VIEW | RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;

    ssrLastFrameRenderTarget = renderDevice->createImage( ssrDesc );
}

FrameGraphScheduler::FrameGraphScheduler( BaseAllocator* allocator, RenderDevice* renderDevice, VirtualFileSystem* virtualFileSys )
    : memoryAllocator( allocator )
    , renderDevice( renderDevice )
    , perViewBuffer( nullptr )
#if DUSKED
    , materialEditorBuffer( nullptr )
#endif
    , enqueuedRenderPassCount( 0u )
    , enqueuedAsyncRenderPassCount( 0u )
    , currentState( SCHEDULER_STATE_READY )
{
    BufferDesc perViewBufferDesc;
    perViewBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    perViewBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
    perViewBufferDesc.SizeInBytes = sizeof( PerViewBufferData );

    perViewBuffer = renderDevice->createBuffer( perViewBufferDesc );

#if DUSKED
	BufferDesc matEdBufferDesc;
	matEdBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    matEdBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
    matEdBufferDesc.SizeInBytes = sizeof( MaterialEdData );

    materialEditorBuffer = renderDevice->createBuffer( matEdBufferDesc );
#endif

	BufferDesc vectorDataBufferDesc;
	vectorDataBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE;
	vectorDataBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
	vectorDataBufferDesc.SizeInBytes = VECTOR_BUFFER_SIZE;
	vectorDataBufferDesc.StrideInBytes = MAX_VECTOR_PER_INSTANCE;
	vectorDataBufferDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;

    vectorDataBuffer = renderDevice->createBuffer( vectorDataBufferDesc );

    instanceBufferData = dk::core::allocateArray<u8>( allocator, VECTOR_BUFFER_SIZE );

    workers = dk::core::allocateArray<FrameGraphRenderThread>( memoryAllocator, RENDER_THREAD_COUNT, memoryAllocator, renderDevice, virtualFileSys );
    dispatcherThread = std::thread( &FrameGraphScheduler::jobDispatcherThread, this );

    memset( enqueuedRenderPass, 0, sizeof( RenderPassExecutionInfos ) * MAX_RENDERPASS_PER_FRAME );
    memset( enqueuedAsyncRenderPass, 0, sizeof( RenderPassExecutionInfos ) * MAX_RENDERPASS_PER_FRAME );
}

FrameGraphScheduler::~FrameGraphScheduler()
{
    if ( workers != nullptr ) {
        dk::core::freeArray( memoryAllocator, workers );
        workers = nullptr;
    }

    currentState.store( SCHEDULER_STATE_WAITING_SHUTDOWN );
    if ( dispatcherThread.joinable() ) {
        dispatcherThread.join();
    }

    dk::core::freeArray( memoryAllocator, instanceBufferData );
}

void FrameGraphScheduler::addRenderPass( const FrameGraphRenderPass& renderPass, const FrameGraphRenderPass::Handle_t* dependencies, const u32 dependencyCount )
{
    // Retrieve internal FrameGraphRenderPass state to build state dependencies.
    RenderPassExecutionInfos::State_t* internalDependencies[RenderPassExecutionInfos::MAX_DEP_COUNT] = { nullptr };
    for ( u32 depIdx = 0u; depIdx < dependencyCount; depIdx++ ) {
        const FrameGraphRenderPass::Handle_t dependencyHandle = dependencies[depIdx];

        internalDependencies[depIdx] = &enqueuedRenderPass[dependencyHandle].ExecutionState;
    }

    RenderPassExecutionInfos& execInfos = enqueuedRenderPass[enqueuedRenderPassCount++];
    execInfos = RenderPassExecutionInfos( &renderPass, internalDependencies, dependencyCount );
}

void FrameGraphScheduler::addAsyncComputeRenderPass( const FrameGraphRenderPass& renderPass, const FrameGraphRenderPass::Handle_t* dependencies, const u32 dependencyCount )
{
#ifndef DUSK_ASYNC_COMPUTE_AVAILABLE
    addRenderPass( renderPass, dependencies, dependencyCount );
#else
    // Retrieve internal FrameGraphRenderPass state to build state dependencies.
    RenderPassExecutionInfos::State_t* internalDependencies[RenderPassExecutionInfos::MAX_DEP_COUNT] = { nullptr };
    for ( u32 depIdx = 0u; depIdx < dependencyCount; depIdx++ ) {
        const FrameGraphRenderPass::Handle_t dependencyHandle = dependencies[depIdx];

        internalDependencies[depIdx] = &enqueuedAsyncRenderPass[dependencyHandle].ExecutionState;
    }

    enqueuedAsyncRenderPass[enqueuedAsyncRenderPassCount++] = RenderPassExecutionInfos( &renderPass, internalDependencies, dependencyCount );
#endif
}

#if DUSKED
void FrameGraphScheduler::updateMaterialEdBuffer( const MaterialEdData* matEdData )
{
    // Make a local copy of the data (if available).
    if ( matEdData != nullptr ) {
        memcpy( &materialEdData, matEdData, sizeof( MaterialEdData ) );
    }
}
#endif

void FrameGraphScheduler::dispatch( const PerViewBufferData* perViewData, const void* vectorBufferData )
{
    if ( enqueuedRenderPassCount == 0u && enqueuedAsyncRenderPassCount == 0u ) {
        return;
    }

    // Make a local copy of the data (if available).
    if ( perViewData != nullptr ) {
        memcpy( &perViewBufferData, perViewData, sizeof( PerViewBufferData ) );
    }

    if ( vectorBufferData != nullptr ) {
        memcpy( instanceBufferData, vectorBufferData, VECTOR_BUFFER_SIZE );
    }

    State schedulerState = SCHEDULER_STATE_READY;
    bool flushResult = currentState.compare_exchange_strong( schedulerState, SCHEDULER_STATE_HAS_JOB_TO_DO );
    
    DUSK_DEV_ASSERT( flushResult, "Failed to flush FrameGraphScheduler : the scheduler is busy!" );
}

bool FrameGraphScheduler::isReady()
{
    State schedulerState = SCHEDULER_STATE_READY;
    return currentState.compare_exchange_weak( schedulerState, SCHEDULER_STATE_READY );
}

void FrameGraphScheduler::jobDispatcherThread()
{
    while ( 1 ) {
        State schedulerState = SCHEDULER_STATE_HAS_JOB_TO_DO;
        if ( !currentState.compare_exchange_weak( schedulerState, SCHEDULER_STATE_HAS_JOB_TO_DO ) ) {
            if ( schedulerState == SCHEDULER_STATE_WAITING_SHUTDOWN ) {
                return;
            }

            std::this_thread::yield();
            continue;
        }

        currentState.store( SCHEDULER_STATE_WAITING_JOB_COMPLETION );

        // Schedule renderpasses
        i32 freeWorkerIdx = 0;
        i32 perWorkerJobCount[RENDER_THREAD_COUNT] = { 0 };

        // TODO Have a dedicated worker for compute render pass? (so that we can schedule stuff asynchronously?)

        // Super basic load-balancing, we should get some kind of metric to make this more efficient (instruction count? expected run time?)
//#ifndef DUSK_ASYNC_COMPUTE_AVAILABLE
        i32 renderPassPerWorker = ( enqueuedRenderPassCount / RENDER_THREAD_COUNT ) + 1;

        // TODO For testing purposes, we do a FIFO scheduling.
        for ( u32 i = 0; i < enqueuedRenderPassCount; i++ ) {
            workers[freeWorkerIdx].enqueueRenderPass( &enqueuedRenderPass[i] );

            ++perWorkerJobCount[freeWorkerIdx];
            if ( perWorkerJobCount[freeWorkerIdx] >= renderPassPerWorker ) {
                freeWorkerIdx = ( ++freeWorkerIdx % RENDER_THREAD_COUNT );
            }
        }
        enqueuedRenderPassCount = 0u;
//#else
//#pragma error "Async Compute needs to be implemented!"
//#endif

        // Update shared resources at the beginning of the frame (e.g. PerViewBuffer).
        // Flush the command list as soon as possible.
        // We need to do the buffer upload in the dispatcher thread since we want to guarantee the constness of the view
        // data for the frame being rendered.
        CommandList& bufferUploadCmdList = renderDevice->allocateCopyCommandList();
        bufferUploadCmdList.begin();
        bufferUploadCmdList.updateBuffer( *perViewBuffer, &perViewBufferData, sizeof( PerViewBufferData ) );
#if DUSKED
        bufferUploadCmdList.updateBuffer( *materialEditorBuffer, &materialEdData, sizeof( MaterialEdData ) );
#endif
        bufferUploadCmdList.updateBuffer( *vectorDataBuffer, instanceBufferData, VECTOR_BUFFER_SIZE );
        bufferUploadCmdList.end();
        renderDevice->submitCommandList( bufferUploadCmdList );

        CommandList* allocatedCmdList[RENDER_THREAD_COUNT] = { nullptr };
        for ( i32 j = 0; j < RENDER_THREAD_COUNT; j++ ) {
            if ( workers[j].hasWorkTodo() ) {
                allocatedCmdList[j] = ( j == 1 ) ? &renderDevice->allocateComputeCommandList() : &renderDevice->allocateGraphicsCommandList();
                allocatedCmdList[j]->begin();

                workers[j].flush( allocatedCmdList[j] );
            }
        }

        spinUntilCompletion();

        // Finish cmd list and submit to the Device.
        u32 cmdCount = 0;
        CommandList* cmdListToSubmit[RENDER_THREAD_COUNT] = { nullptr };
        for ( i32 j = 0; j < RENDER_THREAD_COUNT; j++ ) {
            if ( allocatedCmdList[j] != nullptr ) {
                CommandList& workerCmdList = *allocatedCmdList[j];
                workerCmdList.end();

                cmdListToSubmit[cmdCount++] = allocatedCmdList[j];
            }
        }

        renderDevice->submitCommandLists( cmdListToSubmit, cmdCount );

        // Swap buffers
        renderDevice->present();

        currentState.store( SCHEDULER_STATE_READY );
    }
}

void FrameGraphScheduler::spinUntilCompletion()
{
    bool isReady;

    do {
        isReady = true;

        for ( i32 j = 0; j < RENDER_THREAD_COUNT; j++ ) {
            isReady &= workers[j].isReady();
        }
    } while ( !isReady );
}

FrameGraphRenderThread::FrameGraphRenderThread( BaseAllocator* allocator, RenderDevice* renderDevice, VirtualFileSystem* virtualFileSys )
    : memoryAllocator( allocator )
    , threadHandle( &FrameGraphRenderThread::workerThread, this )
    , pipelineStateCache( nullptr )
    , commandList( nullptr )
    , currentState( FGRAPH_THREAD_STATE_READY )
    , rpToExecuteCount( 0u )
{
    pipelineStateCache = dk::core::allocate<PipelineStateCache>( memoryAllocator, memoryAllocator, renderDevice, virtualFileSys );
    memset( renderpassToExecute, 0, sizeof( RenderPassExecutionInfos* ) * MAX_RENDERPASS_COUNT );
}

FrameGraphRenderThread::~FrameGraphRenderThread()
{
    // Notify workerThread for shutdown.
    currentState.store( FGRAPH_THREAD_STATE_WAITING_SHUTDOWN );
    if ( threadHandle.joinable() ) {
        threadHandle.join();
    }

    dk::core::free( memoryAllocator, pipelineStateCache );
    commandList = nullptr;
    rpToExecuteCount.store( 0u );
}

bool FrameGraphRenderThread::isReady()
{
    State ExpectedState = FGRAPH_THREAD_STATE_READY;
    return currentState.compare_exchange_weak( ExpectedState, FGRAPH_THREAD_STATE_READY );
}

bool FrameGraphRenderThread::hasWorkTodo()
{
    u32 workCount = 0u;

    bool hasNothingTodo = rpToExecuteCount.compare_exchange_weak( workCount, 0u );
    return !( hasNothingTodo && workCount == 0u );
}

void FrameGraphRenderThread::flush( CommandList* cmdList )
{
    commandList = cmdList;

    rpInExecutionCount.store( rpToExecuteCount.load() );
    rpToExecuteCount.store( 0u );

    State workerState = FGRAPH_THREAD_STATE_READY;
    bool flushResult = currentState.compare_exchange_strong( workerState, FGRAPH_THREAD_STATE_HAS_JOB_TO_DO );
    DUSK_DEV_ASSERT( flushResult, "Failed to flush RenderJobs : the RenderThread is busy!" );
}

void FrameGraphRenderThread::enqueueRenderPass( RenderPassExecutionInfos* renderpass )
{
    renderpassToExecute[rpToExecuteCount++] = renderpass;
}

void FrameGraphRenderThread::workerThread()
{
    while ( 1 ) {
        State workerState = FGRAPH_THREAD_STATE_HAS_JOB_TO_DO;
        bool hasBeenNotified = currentState.compare_exchange_weak( workerState, FGRAPH_THREAD_STATE_HAS_JOB_TO_DO );
        if ( !hasBeenNotified ) {
            // Received shutdown signal; we can safely return.
            if ( workerState == FGRAPH_THREAD_STATE_WAITING_SHUTDOWN ) {
                return;
            }

            std::this_thread::yield();
            continue;
        }

        DUSK_DEV_ASSERT( commandList, "FrameGraphRenderThread has no CommandList binded!" );

        currentState.store( FGRAPH_THREAD_STATE_BUSY );

        // Execute enqueued RenderPass
        const u32 jobCount = rpInExecutionCount.load();
        rpInExecutionCount.store( 0u );

        for ( u32 jobIdx = 0; jobIdx < jobCount; jobIdx++ ) {
            RenderPassExecutionInfos& renderPass = *renderpassToExecute[jobIdx];

            // Wait for dependencies
            if ( renderPass.DependencyCount != 0u ) {
                RenderPassExecutionInfos::State dependencyState = RenderPassExecutionInfos::RP_EXECUTION_STATE_DONE;
                do {
                    dependencyState = RenderPassExecutionInfos::RP_EXECUTION_STATE_DONE;
                    for ( u32 depIdx = 0; depIdx < renderPass.DependencyCount; depIdx++ ) {
                        if ( !renderPass.Dependencies[depIdx]->compare_exchange_weak( dependencyState, RenderPassExecutionInfos::RP_EXECUTION_STATE_DONE ) ) {
                            break;
                        }
                    }
                } while ( dependencyState != RenderPassExecutionInfos::RP_EXECUTION_STATE_DONE );
            }

            renderPass.ExecutionState.store( RenderPassExecutionInfos::RP_EXECUTION_STATE_IN_PROGRESS );
            renderPass.RenderPass->Execute( commandList, pipelineStateCache );
            renderPass.ExecutionState.store( RenderPassExecutionInfos::RP_EXECUTION_STATE_DONE );
        }

        currentState.store( FGRAPH_THREAD_STATE_READY );
    }
}
