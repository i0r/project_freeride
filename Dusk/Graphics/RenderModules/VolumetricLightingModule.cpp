/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "VolumetricLightingModule.h"

#include "Graphics/FrameGraph.h"
#include "Graphics/ShaderCache.h"
#include "Graphics/GraphicsAssetCache.h"
#include "Graphics/ShaderHeaders/Light.h"
#include "Graphics/RenderModules/Generated/VolumetricLighting.generated.h"

#include "Maths/Helpers.h"

DUSK_DEV_VAR( VolumeTileDimension, "Dimension of a volume tile (in pixels). Should be smaller than the size of a light tile.", 8, i32 );
DUSK_DEV_VAR( VolumeDepthResolution, "Resolution of froxels depth for volumetric rendering (in pixels).", 64, i32 );
DUSK_DEV_VAR( VolumeDistribution, "Lambda controling the distribution of samples across the depth.", 0.7f, f32 );

VolumetricLightModule::VolumetricLightModule()
{

}

VolumetricLightModule::~VolumetricLightModule()
{

}

void VolumetricLightModule::addFroxelDataFetchPass( FrameGraph& frameGraph, const u32 width, const u32 height )
{
    struct PassData
    {
        // FroxelsBuffer[0] : Scattering RGB
        // FroxelsBuffer[1] : Extinction RGB
        // FroxelsBuffer[2] : Emission RGB
        // FroxelsBuffer[3] : Phase RGB
        FGHandle    FroxelsBuffer[4];
        FGHandle    PerPassBuffer;
    };

    const f32 invVolumeTileDim = ( 1.0f / VolumeTileDimension );
    i32 textureWidth = static_cast< i32 >( width * invVolumeTileDim );
    i32 textureHeight = static_cast< i32 >( height * invVolumeTileDim );
    i32 textureDepth = static_cast< i32 >( VolumeDepthResolution );

    dkVec4f coordinatesScale = dkVec4f(
        width / static_cast< f32 >( VolumeTileDimension * textureWidth ),
        height / static_cast< f32 >( VolumeTileDimension * textureHeight ),
        1.0f / width,
        1.0f / height
    );

    dkVec3f invTextureSize = 1.0f / dkVec3f(
        static_cast< f32 >( textureWidth ),
        static_cast< f32 >( textureHeight ),
        static_cast< f32 >( textureDepth )
    );

    dkVec3f jitterOffsets = dkVec3f::Zero; // dk::maths::HaltonOffset3D();

    PassData passData = frameGraph.addRenderPass<PassData>(
        VolumetricLighting::StoreProperties_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            ImageDesc froxelsBuffer;
            froxelsBuffer.dimension = ImageDesc::DIMENSION_3D;
            froxelsBuffer.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            froxelsBuffer.format = VIEW_FORMAT_R11G11B10_FLOAT;
            froxelsBuffer.usage = RESOURCE_USAGE_DEFAULT;
            froxelsBuffer.width = textureWidth;
            froxelsBuffer.height = textureHeight;
            froxelsBuffer.depth = textureDepth;

            passData.FroxelsBuffer[0] = builder.allocateImage( froxelsBuffer );
            passData.FroxelsBuffer[1] = builder.allocateImage( froxelsBuffer );
            passData.FroxelsBuffer[2] = builder.allocateImage( froxelsBuffer );
            passData.FroxelsBuffer[3] = builder.allocateImage( froxelsBuffer );

            BufferDesc perPassDesc;
            perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassDesc.SizeInBytes = sizeof( VolumetricLighting::StorePropertiesRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            const CameraData* cameraData = resources->getMainCamera();

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* froxelScattering = resources->getImage( passData.FroxelsBuffer[0] );
            Image* froxelExtinction = resources->getImage( passData.FroxelsBuffer[1] );
            Image* froxelEmission = resources->getImage( passData.FroxelsBuffer[2] );
            Image* froxelPhase = resources->getImage( passData.FroxelsBuffer[3] );

            const f32 depthRange = ( cameraData->nearPlane - cameraData->farPlane );

            f32 depthParamsZ = 4.0f * ( Max( 1.0f - sqrt( VolumeDistribution ) * 0.95f, 1e-2f ) );
            f32 depthParamsX = ( depthRange * pow( 2.0f, 1.0f / depthParamsZ ) ) / depthRange;
            f32 depthParamsY = ( 1.0f - depthParamsX ) / cameraData->farPlane;

            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, VolumetricLighting::StoreProperties_ShaderBinding );
            cmdList->pushEventMarker( VolumetricLighting::StoreProperties_EventName );
            cmdList->bindPipelineState( pipelineState );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            cmdList->bindImage( VolumetricLighting::StoreProperties_FroxelScatteringOut_Hashcode, froxelScattering );
            cmdList->bindImage( VolumetricLighting::StoreProperties_FroxelExtinctionOut_Hashcode, froxelExtinction );
            cmdList->bindImage( VolumetricLighting::StoreProperties_FroxelEmissionOut_Hashcode, froxelEmission );
            cmdList->bindImage( VolumetricLighting::StoreProperties_FroxelPhaseOut_Hashcode, froxelPhase );

            VolumetricLighting::StorePropertiesProperties.VolDepthParams = dkVec3f( depthParamsX, depthParamsY, depthParamsZ );
            VolumetricLighting::StorePropertiesProperties.VolInverseTexSize = invTextureSize;
            VolumetricLighting::StorePropertiesProperties.VolJitter = jitterOffsets;
            VolumetricLighting::StorePropertiesProperties.VolScale = coordinatesScale;
            cmdList->updateBuffer( *perPassBuffer, &VolumetricLighting::StorePropertiesProperties, sizeof( VolumetricLighting::StorePropertiesRuntimeProperties ) );
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = DispatchSize( VolumetricLighting::StoreProperties_DispatchX, textureWidth );
            u32 ThreadGroupY = DispatchSize( VolumetricLighting::StoreProperties_DispatchY, textureHeight );
            u32 ThreadGroupZ = DispatchSize( VolumetricLighting::StoreProperties_DispatchZ, textureDepth );

            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, ThreadGroupZ );

            cmdList->popEventMarker();
        }
    );

    getScatteringFroxelTexHandle() = passData.FroxelsBuffer[0];
    getExtinctionFroxelTexHandle() = passData.FroxelsBuffer[1];
    getEmissionTexHandle() = passData.FroxelsBuffer[2];
    getPhaseTexHandle() = passData.FroxelsBuffer[3];
}

void VolumetricLightModule::addFroxelScatteringPass( FrameGraph& frameGraph, const u32 width, const u32 height, FGHandle clusterList, FGHandle itemList )
{
    struct PassData
    {
        // FroxelsBuffer[0] : Scattering RGB
        // FroxelsBuffer[1] : Extinction RGB
        // FroxelsBuffer[2] : Emission RGB
        // FroxelsBuffer[3] : Phase RGB
        FGHandle    FroxelsBuffer[4];
        FGHandle    VBuffer[2];
        FGHandle    PerPassBuffer;
        FGHandle    ItemList;
        FGHandle    ClusterList;
    };

    const f32 invVolumeTileDim = ( 1.0f / VolumeTileDimension );
    i32 textureWidth = static_cast< i32 >( width * invVolumeTileDim );
    i32 textureHeight = static_cast< i32 >( height * invVolumeTileDim );
    i32 textureDepth = static_cast< i32 >( VolumeDepthResolution );

    dkVec4f coordinatesScale = dkVec4f(
        width / static_cast< f32 >( VolumeTileDimension * textureWidth ),
        height / static_cast< f32 >( VolumeTileDimension * textureHeight ),
        1.0f / width,
        1.0f / height
    );

    dkVec3f invTextureSize = 1.0f / dkVec3f(
        static_cast< f32 >( textureWidth ),
        static_cast< f32 >( textureHeight ),
        static_cast< f32 >( textureDepth )
    );

    dkVec3f jitterOffsets = dkVec3f::Zero; //  dk::maths::HaltonOffset3D();

    PassData passData = frameGraph.addRenderPass<PassData>(
        VolumetricLighting::Scattering_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.FroxelsBuffer[0] = builder.readReadOnlyImage( getScatteringFroxelTexHandle() );
            passData.FroxelsBuffer[1] = builder.readReadOnlyImage( getExtinctionFroxelTexHandle() );
            passData.FroxelsBuffer[2] = builder.readReadOnlyImage( getEmissionTexHandle() );
            passData.FroxelsBuffer[3] = builder.readReadOnlyImage( getPhaseTexHandle() );

            ImageDesc froxelsBuffer;
            froxelsBuffer.dimension = ImageDesc::DIMENSION_3D;
            froxelsBuffer.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            froxelsBuffer.format = VIEW_FORMAT_R11G11B10_FLOAT;
            froxelsBuffer.usage = RESOURCE_USAGE_DEFAULT;
            froxelsBuffer.width = textureWidth;
            froxelsBuffer.height = textureHeight;
            froxelsBuffer.depth = textureDepth;

            passData.VBuffer[0] = builder.allocateImage( froxelsBuffer );
            passData.VBuffer[1] = builder.allocateImage( froxelsBuffer );

            BufferDesc perPassDesc;
            perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassDesc.SizeInBytes = sizeof( VolumetricLighting::StorePropertiesRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );

            passData.ItemList = builder.readReadOnlyBuffer( itemList );
            passData.ClusterList = builder.readReadOnlyImage( clusterList );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            const CameraData* cameraData = resources->getMainCamera();

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* froxelScattering = resources->getImage( passData.FroxelsBuffer[0] );
            Image* froxelExtinction = resources->getImage( passData.FroxelsBuffer[1] );
            Image* froxelEmission = resources->getImage( passData.FroxelsBuffer[2] );
            Image* froxelPhase = resources->getImage( passData.FroxelsBuffer[3] );

            Image* vbuffer0 = resources->getImage( passData.VBuffer[0] );
            Image* vbuffer1 = resources->getImage( passData.VBuffer[1] );

            Buffer* itemList = resources->getBuffer( passData.ItemList );
            Image* clusterList = resources->getImage( passData.ClusterList );

            const f32 depthRange = ( cameraData->nearPlane - cameraData->farPlane );

            f32 depthParamsZ = 4.0f * ( Max( 1.0f - sqrt( VolumeDistribution ) * 0.95f, 1e-2f ) );
            f32 depthParamsX = ( depthRange * pow( 2.0f, 1.0f / depthParamsZ ) ) / depthRange;
            f32 depthParamsY = ( 1.0f - depthParamsX ) / cameraData->farPlane;

            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, VolumetricLighting::Scattering_ShaderBinding );
            cmdList->pushEventMarker( VolumetricLighting::Scattering_EventName );
            cmdList->bindPipelineState( pipelineState );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            cmdList->bindImage( VolumetricLighting::Scattering_FroxelScattering_Hashcode, froxelScattering );
            cmdList->bindImage( VolumetricLighting::Scattering_FroxelExtinction_Hashcode, froxelExtinction );
            cmdList->bindImage( VolumetricLighting::Scattering_FroxelEmission_Hashcode, froxelEmission );
            cmdList->bindImage( VolumetricLighting::Scattering_FroxelPhase_Hashcode, froxelPhase );
            cmdList->bindBuffer( VolumetricLighting::Scattering_ItemList_Hashcode, itemList );
            cmdList->bindImage( VolumetricLighting::Scattering_Clusters_Hashcode, clusterList );

            cmdList->bindImage( VolumetricLighting::Scattering_VBuffer0_Hashcode, vbuffer0 );
            cmdList->bindImage( VolumetricLighting::Scattering_VBuffer1_Hashcode, vbuffer1 );

            VolumetricLighting::StorePropertiesProperties.VolDepthParams = dkVec3f( depthParamsX, depthParamsY, depthParamsZ );
            VolumetricLighting::StorePropertiesProperties.VolInverseTexSize = invTextureSize;
            VolumetricLighting::StorePropertiesProperties.VolJitter = jitterOffsets;
            VolumetricLighting::StorePropertiesProperties.VolScale = coordinatesScale;
            cmdList->updateBuffer( *perPassBuffer, &VolumetricLighting::StorePropertiesProperties, sizeof( VolumetricLighting::StorePropertiesRuntimeProperties ) );
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = DispatchSize( VolumetricLighting::Scattering_DispatchX, textureWidth );
            u32 ThreadGroupY = DispatchSize( VolumetricLighting::Scattering_DispatchY, textureHeight );
            u32 ThreadGroupZ = DispatchSize( VolumetricLighting::Scattering_DispatchZ, textureDepth );

            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, ThreadGroupZ );

            cmdList->popEventMarker();
        }
    );

    vbuffers[0] = passData.VBuffer[0];
    vbuffers[1] = passData.VBuffer[1];
}

void VolumetricLightModule::addIntegrationPass( FrameGraph& frameGraph, const u32 width, const u32 height )
{
    struct PassData
    {
        FGHandle    VBuffer[2];
        FGHandle    IntegratedVBuffer[2];
        FGHandle    PerPassBuffer;
    };

    const f32 invVolumeTileDim = ( 1.0f / VolumeTileDimension );
    i32 textureWidth = static_cast< i32 >( width * invVolumeTileDim );
    i32 textureHeight = static_cast< i32 >( height * invVolumeTileDim );
    i32 textureDepth = static_cast< i32 >( VolumeDepthResolution );

    dkVec4f coordinatesScale = dkVec4f(
        width / static_cast< f32 >( VolumeTileDimension * textureWidth ),
        height / static_cast< f32 >( VolumeTileDimension * textureHeight ),
        1.0f / width,
        1.0f / height
    );

    dkVec3f textureSize = dkVec3f(
        static_cast< f32 >( textureWidth ),
        static_cast< f32 >( textureHeight ),
        static_cast< f32 >( textureDepth )
    );

    dkVec3f invTextureSize = 1.0f / textureSize;

    dkVec3f jitterOffsets = dkVec3f::Zero; // dk::maths::HaltonOffset3D();

    PassData passData = frameGraph.addRenderPass<PassData>(
        VolumetricLighting::Integration_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.VBuffer[0] = builder.readReadOnlyImage( vbuffers[0] );
            passData.VBuffer[1] = builder.readReadOnlyImage( vbuffers[1] );

            ImageDesc froxelsBuffer;
            froxelsBuffer.dimension = ImageDesc::DIMENSION_3D;
            froxelsBuffer.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            froxelsBuffer.format = VIEW_FORMAT_R11G11B10_FLOAT;
            froxelsBuffer.usage = RESOURCE_USAGE_DEFAULT;
            froxelsBuffer.width = textureWidth;
            froxelsBuffer.height = textureHeight;
            froxelsBuffer.depth = textureDepth;

            passData.IntegratedVBuffer[0] = builder.allocateImage( froxelsBuffer );
            passData.IntegratedVBuffer[1] = builder.allocateImage( froxelsBuffer );

            BufferDesc perPassDesc;
            perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassDesc.SizeInBytes = sizeof( VolumetricLighting::IntegrationRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            const CameraData* cameraData = resources->getMainCamera();

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* integratedVbuffer0 = resources->getImage( passData.IntegratedVBuffer[0] );
            Image* integratedVbuffer1 = resources->getImage( passData.IntegratedVBuffer[1] );

            Image* vbuffer0 = resources->getImage( passData.VBuffer[0] );
            Image* vbuffer1 = resources->getImage( passData.VBuffer[1] );

            const f32 depthRange = ( cameraData->nearPlane - cameraData->farPlane );

            f32 depthParamsZ = 4.0f * ( Max( 1.0f - sqrt( VolumeDistribution ) * 0.95f, 1e-2f ) );
            f32 depthParamsX = ( depthRange * pow( 2.0f, 1.0f / depthParamsZ ) ) / depthRange;
            f32 depthParamsY = ( 1.0f - depthParamsX ) / cameraData->farPlane;

            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
            psoDesc.addStaticSampler( RenderingHelpers::S_BilinearClampEdge );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, VolumetricLighting::Integration_ShaderBinding );
            cmdList->pushEventMarker( VolumetricLighting::Integration_EventName );
            cmdList->bindPipelineState( pipelineState );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            cmdList->bindImage( VolumetricLighting::Integration_FroxelScattering_Hashcode, vbuffer0 );
            cmdList->bindImage( VolumetricLighting::Integration_FroxelExtinction_Hashcode, vbuffer1 );
            cmdList->bindImage( VolumetricLighting::Integration_VBuffer0_Hashcode, integratedVbuffer0 );
            cmdList->bindImage( VolumetricLighting::Integration_VBuffer1_Hashcode, integratedVbuffer1 );

            VolumetricLighting::StorePropertiesProperties.VolDepthParams = dkVec3f( depthParamsX, depthParamsY, depthParamsZ );
            VolumetricLighting::StorePropertiesProperties.VolInverseTexSize = invTextureSize;
            VolumetricLighting::StorePropertiesProperties.VolJitter = jitterOffsets;
            VolumetricLighting::StorePropertiesProperties.VolScale = coordinatesScale;
            VolumetricLighting::StorePropertiesProperties.VolSize = textureSize;
            cmdList->updateBuffer( *perPassBuffer, &VolumetricLighting::StorePropertiesProperties, sizeof( VolumetricLighting::StorePropertiesRuntimeProperties ) );
            cmdList->prepareAndBindResourceList();

            u32 ThreadGroupX = DispatchSize( VolumetricLighting::Integration_DispatchX, textureWidth );
            u32 ThreadGroupY = DispatchSize( VolumetricLighting::Integration_DispatchY, textureHeight );
            u32 ThreadGroupZ = DispatchSize( VolumetricLighting::Integration_DispatchZ, textureDepth );

            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, ThreadGroupZ );

            cmdList->popEventMarker();
        }
    );

    integratedVbuffers[0] = passData.IntegratedVBuffer[0];
    integratedVbuffers[1] = passData.IntegratedVBuffer[1];
}

FGHandle VolumetricLightModule::addResolvePass( FrameGraph& frameGraph, FGHandle colorBuffer, FGHandle depthBuffer, const u32 width, const u32 height )
{
    struct PassData
    {
        FGHandle    IntegratedVBuffer[2];
        FGHandle    ColorBuffer;
        FGHandle    DepthBuffer;

        FGHandle    ResolvedColor;
        FGHandle    PerPassBuffer;
    };

    const f32 invVolumeTileDim = ( 1.0f / VolumeTileDimension );
    i32 textureWidth = static_cast< i32 >( width * invVolumeTileDim );
    i32 textureHeight = static_cast< i32 >( height * invVolumeTileDim );
    i32 textureDepth = static_cast< i32 >( VolumeDepthResolution );

    dkVec4f coordinatesScale = dkVec4f(
        width / static_cast< f32 >( VolumeTileDimension * textureWidth ),
        height / static_cast< f32 >( VolumeTileDimension * textureHeight ),
        1.0f / width,
        1.0f / height
    );

    dkVec3f textureSize = dkVec3f(
        static_cast< f32 >( textureWidth ),
        static_cast< f32 >( textureHeight ),
        static_cast< f32 >( textureDepth )
    );

    dkVec3f invTextureSize = 1.0f / textureSize;

    dkVec3f jitterOffsets = dkVec3f::Zero; // dk::maths::HaltonOffset3D();

    PassData passData = frameGraph.addRenderPass<PassData>(
        VolumetricLighting::Resolve_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();
            builder.setUncullablePass();

            passData.IntegratedVBuffer[0] = builder.readReadOnlyImage( integratedVbuffers[0] );
            passData.IntegratedVBuffer[1] = builder.readReadOnlyImage( integratedVbuffers[1] );

            passData.ColorBuffer = builder.readReadOnlyImage( colorBuffer );
            passData.DepthBuffer = builder.readReadOnlyImage( depthBuffer );

            ImageDesc resolvedBuffer;
            resolvedBuffer.dimension = ImageDesc::DIMENSION_2D;
            resolvedBuffer.bindFlags = RESOURCE_BIND_UNORDERED_ACCESS_VIEW | RESOURCE_BIND_SHADER_RESOURCE;
            resolvedBuffer.format = VIEW_FORMAT_R16G16B16A16_FLOAT;
            resolvedBuffer.usage = RESOURCE_USAGE_DEFAULT;

            passData.ResolvedColor = builder.allocateImage( resolvedBuffer, FrameGraphBuilder::eImageFlags::USE_PIPELINE_DIMENSIONS_ONE | FrameGraphBuilder::eImageFlags::NO_MULTISAMPLE );

            BufferDesc perPassDesc;
            perPassDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            perPassDesc.SizeInBytes = sizeof( VolumetricLighting::IntegrationRuntimeProperties );
            perPassDesc.Usage = RESOURCE_USAGE_DYNAMIC;

            passData.PerPassBuffer = builder.allocateBuffer( perPassDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            const CameraData* cameraData = resources->getMainCamera();

            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            Image* integratedVbuffer0 = resources->getImage( passData.IntegratedVBuffer[0] );
            Image* integratedVbuffer1 = resources->getImage( passData.IntegratedVBuffer[1] );

            Image* colorBuffer = resources->getImage( passData.ColorBuffer );
            Image* depthBuffer = resources->getImage( passData.DepthBuffer );

            Image* resolvedColorBuffer = resources->getImage( passData.ResolvedColor );

            const f32 depthRange = ( cameraData->nearPlane - cameraData->farPlane );

            f32 depthParamsZ = 4.0f * ( Max( 1.0f - sqrt( VolumeDistribution ) * 0.95f, 1e-2f ) );
            f32 depthParamsX = ( depthRange * pow( 2.0f, 1.0f / depthParamsZ ) ) / depthRange;
            f32 depthParamsY = ( 1.0f - depthParamsX ) / cameraData->farPlane;

            PipelineStateDesc psoDesc( PipelineStateDesc::COMPUTE );
            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( psoDesc, VolumetricLighting::Resolve_ShaderBinding );
            cmdList->pushEventMarker( VolumetricLighting::Resolve_EventName );
            cmdList->bindPipelineState( pipelineState );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            cmdList->bindImage( VolumetricLighting::Resolve_FroxelScattering_Hashcode, integratedVbuffer0 );
            cmdList->bindImage( VolumetricLighting::Resolve_FroxelEmission_Hashcode, integratedVbuffer1 );
            cmdList->bindImage( VolumetricLighting::Resolve_ColorBuffer_Hashcode, colorBuffer );
            cmdList->bindImage( VolumetricLighting::Resolve_ResolvedDepthBuffer_Hashcode, depthBuffer );
            cmdList->bindImage( VolumetricLighting::Resolve_ResolvedColorBuffer_Hashcode, resolvedColorBuffer );

            VolumetricLighting::StorePropertiesProperties.VolDepthParams = dkVec3f( depthParamsX, depthParamsY, depthParamsZ );
            VolumetricLighting::StorePropertiesProperties.VolInverseTexSize = invTextureSize;
            VolumetricLighting::StorePropertiesProperties.VolJitter = jitterOffsets;
            VolumetricLighting::StorePropertiesProperties.VolScale = coordinatesScale;
            VolumetricLighting::StorePropertiesProperties.VolSize = textureSize;
            cmdList->updateBuffer( *perPassBuffer, &VolumetricLighting::StorePropertiesProperties, sizeof( VolumetricLighting::StorePropertiesRuntimeProperties ) );
            cmdList->prepareAndBindResourceList();

            const Viewport* vp = resources->getMainViewport();

            u32 ThreadGroupX = DispatchSize( VolumetricLighting::Resolve_DispatchX, vp->Width );
            u32 ThreadGroupY = DispatchSize( VolumetricLighting::Resolve_DispatchY, vp->Height );
            u32 ThreadGroupZ = VolumetricLighting::Resolve_DispatchZ;

            cmdList->dispatchCompute( ThreadGroupX, ThreadGroupY, ThreadGroupZ );

            cmdList->popEventMarker();
        }
    );

    return passData.ResolvedColor;
}

void VolumetricLightModule::destroy( RenderDevice& renderDevice )
{

}

void VolumetricLightModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{

}
