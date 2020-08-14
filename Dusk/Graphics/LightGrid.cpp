/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "LightGrid.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/GraphicsAssetCache.h>
#include <Graphics/FrameGraph.h>

#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>
#include <Core/ViewFormat.h>

#include <Maths/Helpers.h>

static constexpr i32 CLUSTER_X = 16;
static constexpr i32 CLUSTER_Y = 8;
static constexpr i32 CLUSTER_Z = 24;

LightGrid::LightGrid( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , perSceneBufferData{ 0 }
{
    memset( &perSceneBufferData, 0, sizeof( PerSceneBufferData ) );
    setSceneBounds( dkVec3f( 2048.0f, 2048.0f, 2048.0f ), -dkVec3f( 2048.0f, 2048.0f, 2048.0f ) );

    // Set Default directional light values.
    perSceneBufferData.SunLight.SphericalCoordinates = dkVec2f( 0.5f, 0.5f );
    perSceneBufferData.SunLight.NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( 0.5f, 0.5f );

    constexpr f32 kSunAngularRadius = 0.00935f / 2.0f;
    perSceneBufferData.SunLight.AngularRadius = kSunAngularRadius;

    // Cone solid angle formula = 2PI(1-cos(0.5*AD*(PI/180)))
    const f32 solidAngle = ( 2.0f * dk::maths::PI<f32>() ) * ( 1.0f - cos( perSceneBufferData.SunLight.AngularRadius ) );

    perSceneBufferData.SunLight.IlluminanceInLux = 100000.0f * solidAngle;
    perSceneBufferData.SunLight.ColorLinearSpace = dkVec3f( 1.0f, 1.0f, 1.0f );
}

LightGrid::~LightGrid()
{

}

LightGrid::Data LightGrid::updateClusters( FrameGraph& frameGraph )
{
    constexpr PipelineStateDesc DefaultPipelineState( PipelineStateDesc::COMPUTE );

    struct PassData {
        FGHandle PerSceneBuffer;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        "PerSceneBufferData Update",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            BufferDesc sceneClustersBufferDesc;
            sceneClustersBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            sceneClustersBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            sceneClustersBufferDesc.SizeInBytes = sizeof( PerSceneBufferData );
            passData.PerSceneBuffer = builder.allocateBuffer( sceneClustersBufferDesc, SHADER_STAGE_COMPUTE );

            /*     ImageDesc lightClusterDesc;
            lightClusterDesc.dimension = ImageDesc::DIMENSION_3D;
            lightClusterDesc.format = eViewFormat::VIEW_FORMAT_R32G32_UINT;
            lightClusterDesc.usage = RESOURCE_USAGE_DEFAULT;
            lightClusterDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            lightClusterDesc.width = CLUSTER_X;
            lightClusterDesc.height = CLUSTER_Y;
            lightClusterDesc.depth = CLUSTER_Z;
            lightClusterDesc.mipCount = 1u;
            passData.LightClusters = builder.allocateImage( lightClusterDesc );

            BufferDesc itemListDesc;
            itemListDesc.Usage = RESOURCE_USAGE_DEFAULT;
            itemListDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            itemListDesc.SizeInBytes = sizeof( u32 ) * CLUSTER_X * CLUSTER_Y * CLUSTER_Z * ( MAX_POINT_LIGHT_COUNT + MAX_LOCAL_IBL_PROBE_COUNT );
            itemListDesc.StrideInBytes = static_cast<u32>( itemListDesc.SizeInBytes / sizeof( u32 ) );
            itemListDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32_UINT;

            passData.ItemList = builder.allocateBuffer( itemListDesc, SHADER_STAGE_COMPUTE );*/
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
  /*          Image* lightsClusters = resources->getImage( passData.LightClusters );
            Buffer* itemList = resources->getBuffer( passData.ItemList );*/
            Buffer* perSceneBuffer = resources->getBuffer( passData.PerSceneBuffer );

            //PipelineState* pipelineState = psoCache->getOrCreatePipelineState( DefaultPipelineState, Scene::LightCulling_ShaderBinding );

           /*
            cmdList->bindPipelineState( pipelineState );

            cmdList->bindImage( Scene::LightCulling_LightClusters_Hashcode, lightsClusters );
            cmdList->bindBuffer( Scene::LightCulling_ItemList_Hashcode, itemList, VIEW_FORMAT_R32_UINT );
            cmdList->bindConstantBuffer( PerWorldBufferHashcode, perSceneBuffer );*/
            cmdList->pushEventMarker( DUSK_STRING( "LightCulling Buffer Update" ) );
            cmdList->updateBuffer( *perSceneBuffer, &perSceneBufferData, sizeof( PerSceneBufferData ) );

            //cmdList->prepareAndBindResourceList();

            //cmdList->dispatchCompute( CLUSTER_X, CLUSTER_Y, CLUSTER_Z );

            cmdList->popEventMarker();
        }
    );

    LightGrid::Data returnData;
    returnData.PerSceneBuffer = passData.PerSceneBuffer;

    return returnData;
}

void LightGrid::setSceneBounds( const dkVec3f& sceneAABBMax, const dkVec3f& sceneAABBMin )
{
    perSceneBufferData.SceneAABBMax = sceneAABBMax;

    perSceneBufferData.SceneAABBMinX = sceneAABBMin[0];
    perSceneBufferData.SceneAABBMinY = sceneAABBMin[1];
    perSceneBufferData.SceneAABBMinZ = sceneAABBMin[2];

    updateClustersInfos();
}

DirectionalLightGPU* LightGrid::getDirectionalLightData()
{
    return &perSceneBufferData.SunLight;
}

//
//uint32_t LightGrid::allocatePointLightData( const PointLightData&& lightData )
//{
//    uint32_t i = 0u;
//    for ( ; i < MAX_POINT_LIGHT_COUNT; i++ ) {
//        if ( !pointLightsUsed[i] ) {
//            break;
//        }
//    }
//
//    if ( i == MAX_POINT_LIGHT_COUNT ) {
//        DUSK_DEV_ASSERT( false, "Too many Point Lights! (max is set to %i)", MAX_POINT_LIGHT_COUNT );
//        return std::numeric_limits<uint32_t>::max();
//    }
//
//    pointLightsUsed[i] = true;
//
//    PointLightData& light = cpuBuffer.PointLights[i];
//    light = std::move( lightData );
//
//    needBufferRebuild = true;
//
//    return i;
//}
//
//uint32_t LightGrid::allocateLocalIBLProbeData( const IBLProbeData&& probeData )
//{
//    // NOTE Offset probe array index (first probe should be the global IBL probe)
//    uint32_t i = 1u;
//    for ( ; i < MAX_POINT_LIGHT_COUNT; i++ ) {
//        if ( !iblProbesUsed[i] ) {
//            break;
//        }
//    }
//
//    if ( i == MAX_LOCAL_IBL_PROBE_COUNT ) {
//        DUSK_DEV_ASSERT( false, "Too many Local IBL Probes! (max is set to %i)", MAX_LOCAL_IBL_PROBE_COUNT );
//        return std::numeric_limits<uint32_t>::max();
//    }
//
//    iblProbesUsed[i] = true;
//
//    IBLProbeData& light = cpuBuffer.IBLProbes[i];
//    light = std::move( probeData );
//    light.ProbeIndex = i;
//
//    needBufferRebuild = true;
//
//    return i;
//}
//
//void LightGrid::releasePointLight( const uint32_t pointLightIndex )
//{
//    cpuBuffer.PointLights[pointLightIndex] = { 0 };
//    pointLightsUsed[pointLightIndex] = false;
//}
//
//void LightGrid::releaseIBLProbe( const uint32_t iblProbeIndex )
//{
//    cpuBuffer.IBLProbes[iblProbeIndex] = { 0 };
//    iblProbesUsed[iblProbeIndex] = false;
//}
//
//DirectionalLightData* LightGrid::updateDirectionalLightData( const DirectionalLightData&& lightData )
//{
//    cpuBuffer.DirectionalLight = std::move( lightData );
//    return &cpuBuffer.DirectionalLight;
//}
//
//IBLProbeData* LightGrid::updateGlobalIBLProbeData( const IBLProbeData&& probeData )
//{
//    cpuBuffer.IBLProbes[0] = std::move( probeData );
//    cpuBuffer.IBLProbes[0].ProbeIndex = 0u;
//
//    return &cpuBuffer.IBLProbes[0];
//}
//
//const DirectionalLightData* LightGrid::getDirectionalLightData() const
//{
//    return &cpuBuffer.DirectionalLight;
//}
//
//const IBLProbeData* LightGrid::getGlobalIBLProbeData() const
//{
//    return &cpuBuffer.IBLProbes[0];
//}
//
//PointLightData* LightGrid::getPointLightData( const uint32_t pointLightIndex )
//{
//    DUSK_DEV_ASSERT( pointLightIndex != std::numeric_limits<uint32_t>::max(), "Invalid point light index provided!" );
//
//    return &cpuBuffer.PointLights[pointLightIndex];
//}
//
//IBLProbeData* LightGrid::getIBLProbeData( const uint32_t iblProbeIndex )
//{
//    DUSK_DEV_ASSERT( iblProbeIndex != std::numeric_limits<uint32_t>::max(), "Invalid IBL probe index provided!" );
//
//    return &cpuBuffer.IBLProbes[iblProbeIndex];
//}
//
//#if DUSK_DEVBUILD
//DirectionalLightData& LightGrid::getDirectionalLightDataRW() 
//{
//    return cpuBuffer.DirectionalLight;
//}
//#endif

void LightGrid::updateClustersInfos()
{
    dkVec3f sceneAABBMin = dkVec3f( perSceneBufferData.SceneAABBMinX, perSceneBufferData.SceneAABBMinY, perSceneBufferData.SceneAABBMinZ );

    perSceneBufferData.ClustersScale = dkVec3f( f32( CLUSTER_X ), f32( CLUSTER_Y ), f32( CLUSTER_Z ) ) / ( perSceneBufferData.SceneAABBMax - sceneAABBMin );
    perSceneBufferData.ClustersInverseScale = 1.0f / perSceneBufferData.ClustersScale;
    perSceneBufferData.ClustersBias = -perSceneBufferData.ClustersScale * sceneAABBMin;
}
