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

#include "Graphics/RenderModules/Generated/Culling.generated.h"

LightGrid::LightGrid( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , perSceneBufferData{ 0 }
{
    memset( &perSceneBufferData, 0, sizeof( PerSceneBufferData ) );
    setSceneBounds( dkVec3f( 64.0f, 64.0f, 64.0f ), -dkVec3f( 64.0f, 64.0f, 64.0f ) );

    // Set Default directional light values.
    perSceneBufferData.SunLight.SphericalCoordinates = dkVec2f( 0.5f, 0.5f );
    perSceneBufferData.SunLight.NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( 0.5f, 0.5f );

    constexpr f32 kSunAngularRadius = 0.00935f / 2.0f;
    perSceneBufferData.SunLight.AngularRadius = kSunAngularRadius;

    // Cone solid angle formula = 2PI(1-cos(0.5*AD*(PI/180)))
    const f32 solidAngle = ( 2.0f * dk::maths::PI<f32>() ) * ( 1.0f - cos( perSceneBufferData.SunLight.AngularRadius ) );

    perSceneBufferData.SunLight.IlluminanceInLux = 100000.0f * solidAngle;
    perSceneBufferData.SunLight.ColorLinearSpace = dkVec3f( 1.0f, 1.0f, 1.0f );
    
    perSceneBufferData.PointLightCount = 0;
}

LightGrid::~LightGrid()
{

}

LightGrid::Data LightGrid::updateClusters( FrameGraph& frameGraph )
{
    constexpr PipelineStateDesc DefaultPipelineState( PipelineStateDesc::COMPUTE );

    struct PassData {
        FGHandle PerSceneBuffer;
        FGHandle LightClusters;
        FGHandle ItemList;
    };

    PassData& passData = frameGraph.addRenderPass<PassData>(
        "PerSceneBufferData Update",
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            BufferDesc sceneClustersBufferDesc;
            sceneClustersBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            sceneClustersBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            sceneClustersBufferDesc.SizeInBytes = sizeof( PerSceneBufferData );
            passData.PerSceneBuffer = builder.allocateBuffer( sceneClustersBufferDesc, SHADER_STAGE_COMPUTE );

            ImageDesc lightClusterDesc;
            lightClusterDesc.dimension = ImageDesc::DIMENSION_3D;
            lightClusterDesc.format = eViewFormat::VIEW_FORMAT_R32G32_UINT;
            lightClusterDesc.usage = RESOURCE_USAGE_DEFAULT;
            lightClusterDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            lightClusterDesc.width = CLUSTER_X;
            lightClusterDesc.height = CLUSTER_Y;
            lightClusterDesc.depth = CLUSTER_Z;
            lightClusterDesc.mipCount = 1u;
            passData.LightClusters = builder.allocateImage( lightClusterDesc );

            u32 itemListElementCount = CLUSTER_X * CLUSTER_Y * CLUSTER_Z * ( MAX_POINT_LIGHT_COUNT + MAX_LOCAL_IBL_PROBE_COUNT );

            BufferDesc itemListDesc;
            itemListDesc.Usage = RESOURCE_USAGE_DEFAULT;
            itemListDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
            itemListDesc.SizeInBytes = sizeof( u32 ) * itemListElementCount;
            itemListDesc.StrideInBytes = itemListElementCount;
            itemListDesc.DefaultView.ViewFormat = eViewFormat::VIEW_FORMAT_R32_UINT;
            itemListDesc.DefaultView.NumElements = itemListElementCount;
            
            passData.ItemList = builder.allocateBuffer( itemListDesc, SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Image* lightsClusters = resources->getImage( passData.LightClusters );
            Buffer* itemList = resources->getBuffer( passData.ItemList );
            Buffer* perSceneBuffer = resources->getBuffer( passData.PerSceneBuffer );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( DefaultPipelineState, Culling::CullLights_ShaderBinding );

            cmdList->pushEventMarker( Culling::CullLights_EventName );
            cmdList->bindPipelineState( pipelineState );

            cmdList->updateBuffer( *perSceneBuffer, &perSceneBufferData, sizeof( PerSceneBufferData ) );

            cmdList->bindImage( Culling::CullLights_g_LightClusters_Hashcode, lightsClusters );
            cmdList->bindBuffer( Culling::CullLights_g_ItemList_Hashcode, itemList, VIEW_FORMAT_R32_UINT );
            cmdList->bindConstantBuffer( PerWorldBufferHashcode, perSceneBuffer );
            cmdList->prepareAndBindResourceList();

            cmdList->dispatchCompute( CLUSTER_X, CLUSTER_Y, CLUSTER_Z );

            cmdList->popEventMarker();

            // Reset the light count for the next frame.
            // TODO Right now we discard and reupload all the streamed entities each frame.
            // I guess we could adopt a more clever way to stream/update content...
            perSceneBufferData.PointLightCount = 0;
        }
    );


    LightGrid::Data returnData;
    returnData.PerSceneBuffer = passData.PerSceneBuffer;
    returnData.LightClusters = passData.LightClusters;
    returnData.ItemList = passData.ItemList;

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

u32 LightGrid::addPointLightData( const PointLightGPU&& lightData )
{
    u32 entityIndex = perSceneBufferData.PointLightCount++;
    perSceneBufferData.PointLights[entityIndex] = std::move( lightData );
    return entityIndex;
}

void LightGrid::updateClustersInfos()
{
    dkVec3f sceneAABBMin = dkVec3f( perSceneBufferData.SceneAABBMinX, perSceneBufferData.SceneAABBMinY, perSceneBufferData.SceneAABBMinZ );

    perSceneBufferData.ClustersScale = dkVec3f( f32( CLUSTER_X ), f32( CLUSTER_Y ), f32( CLUSTER_Z ) ) / ( perSceneBufferData.SceneAABBMax - sceneAABBMin );
    perSceneBufferData.ClustersInverseScale = 1.0f / perSceneBufferData.ClustersScale;
    perSceneBufferData.ClustersBias = -perSceneBufferData.ClustersScale * sceneAABBMin;
}
