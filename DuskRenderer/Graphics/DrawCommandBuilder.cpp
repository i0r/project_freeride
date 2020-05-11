/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "DrawCommandBuilder.h"
#if 0 
#include <Framework/Cameras/Camera.h>
//#include <Framework/Mesh.h>
//#include <Framework/Material.h>

#include "DirectionalLightHelpers.h"
#include "Light.h"
#include "WorldRenderer.h"
#include "FrameGraph.h"
#include "LightGrid.h"
#include "GraphicsAssetCache.h"

#include <Rendering/RenderDevice.h>

#include "RenderModules/BrunetonSkyModel.h"
#include "RenderModules/AutomaticExposure.h"
//#include "RenderModules/ProbeCaptureModule.h"
//#include "RenderModules/LineRenderingModule.h"
#include "RenderModules/TextRenderingModule.h"
#include "RenderModules/PresentRenderPass.h"
//#include "RenderPasses/LightRenderPass.h"
//#include "RenderPasses/HUDRenderPass.h"
//#include "RenderPasses/CopyRenderPass.h"
#include "RenderModules/FinalPostFxRenderPass.h"
#include "RenderModules/BlurPyramid.h"
#include "RenderModules/MSAAResolvePass.h"
//#include "RenderPasses/CascadedShadowMapCapturePass.h"
//#include "RenderPasses/FastFourierTransform.h"

#include <Maths/Helpers.h>
#include <Maths/AABB.h>
#include <Maths/Matrix.h>

#include "Shared.h"

#include <Core/EnvVarsRegister.h>
#include <Core/Allocators/StackAllocator.h>
#include <Core/Allocators/LinearAllocator.h>

DUSK_ENV_VAR( DisplayDebugIBLProbe, true, bool )

dkMat4x4f GetProbeCaptureViewMatrix( const dkVec3f& probePositionWorldSpace, const eProbeCaptureStep captureStep )
{
    switch ( captureStep ) {
    case eProbeCaptureStep::FACE_X_PLUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( 1.0f, 0.0f, 0.0f ),
            dkVec3f( 0.0f, 1.0f, 0.0f ) );

    case eProbeCaptureStep::FACE_X_MINUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( -1.0f, 0.0f, 0.0f ),
            dkVec3f( 0.0f, 1.0f, 0.0f ) );

    case eProbeCaptureStep::FACE_Y_MINUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( 0.0f, -1.0f, 0.0f ),
            dkVec3f( 0.0f, 0.0f, 1.0f ) );

    case eProbeCaptureStep::FACE_Y_PLUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( 0.0f, 1.0f, 0.0f ),
            dkVec3f( 0.0f, 0.0f, -1.0f ) );

    case eProbeCaptureStep::FACE_Z_PLUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( 0.0f, 0.0f, 1.0f ),
            dkVec3f( 0.0f, 1.0f, 0.0f ) );

    case eProbeCaptureStep::FACE_Z_MINUS:
        return dk::maths::MakeLookAtMat(
            probePositionWorldSpace,
            probePositionWorldSpace + dkVec3f( 0.0f, 0.0f, -1.0f ),
            dkVec3f( 0.0f, 1.0f, 0.0f ) );
    }

    return dkMat4x4f::Identity;
}

float DistanceToPlane( const dkVec4f& vPlane, const dkVec3f& vPoint )
{
    return dkVec4f::dot( dkVec4f( vPoint, 1.0f ), vPlane );
}

unsigned int FloatFlip( unsigned int f )
{
    unsigned mask = -int( f >> 31 ) | 0x80000000;
    return f ^ mask;
}

// Taking highest 10 bits for rough sort of floats.
// 0.01 maps to 752; 0.1 to 759; 1.0 to 766; 10.0 to 772;
// 100.0 to 779 etc. Negative numbers go similarly in 0..511 range.
uint16_t DepthToBits( const float depth )
{
    union { float f; unsigned int i; } f2i;
    f2i.f = depth;
    f2i.i = FloatFlip( f2i.i ); // flip bits to be sortable
    unsigned int b = f2i.i >> 22; // take highest 10 bits
    return static_cast<uint16_t>( b );
}

// Frustum cullling on a sphere. Returns > 0 if visible, <= 0 otherwise
// NOTE Infinite Z version (it implicitly skips the far plane check)
float CullSphereInfReversedZ( const Frustum* frustum, const dkVec3f& vCenter, float fRadius )
{
    float dist01 = Min( DistanceToPlane( frustum->planes[0], vCenter ), DistanceToPlane( frustum->planes[1], vCenter ) );
    float dist23 = Min( DistanceToPlane( frustum->planes[2], vCenter ), DistanceToPlane( frustum->planes[3], vCenter ) );
    float dist45 = DistanceToPlane( frustum->planes[5], vCenter );

    return Min( Min( dist01, dist23 ), dist45 ) + fRadius;
}

DrawCommandBuilder::DrawCommandBuilder( BaseAllocator* allocator )
    : memoryAllocator( allocator )
{
    cameras = dk::core::allocate<LinearAllocator>( allocator, 8 * sizeof( CameraData* ), allocator->allocate( 8 * sizeof( CameraData* ) ) );
    meshes = dk::core::allocate<LinearAllocator>( allocator, 4096 * sizeof( MeshInstance ), allocator->allocate( 4096 * sizeof( MeshInstance ) ) );
    spheresToRender = dk::core::allocate<LinearAllocator>( allocator, 4096 * sizeof( PrimitiveInstance ), allocator->allocate( 4096 * sizeof( PrimitiveInstance ) ) );
    primitivesToRender = dk::core::allocate<LinearAllocator>( allocator, 4096 * sizeof( PrimitiveInstance ), allocator->allocate( 4096 * sizeof( PrimitiveInstance ) ) );
    textToRenderAllocator = dk::core::allocate<LinearAllocator>( allocator, 1024 * sizeof( TextDrawCommand ), allocator->allocate( 1024 * sizeof( TextDrawCommand ) ) );

    probeCaptureCmdAllocator = dk::core::allocate<StackAllocator>( allocator, 16 * 6 * sizeof( IBLProbeCaptureCommand ), allocator->allocate( 16 * 6 * sizeof( IBLProbeCaptureCommand ) ) );
    probeConvolutionCmdAllocator = dk::core::allocate<StackAllocator>( allocator, 16 * 8 * 6 * sizeof( IBLProbeConvolutionCommand ), allocator->allocate( 16 * 8 * 6 * sizeof( IBLProbeConvolutionCommand ) ) );
}

DrawCommandBuilder::~DrawCommandBuilder()
{
    dk::core::free( memoryAllocator, cameras );
    dk::core::free( memoryAllocator, meshes );
    dk::core::free( memoryAllocator, spheresToRender );
    dk::core::free( memoryAllocator, primitivesToRender );
    dk::core::free( memoryAllocator, textToRenderAllocator );
    dk::core::free( memoryAllocator, probeCaptureCmdAllocator );
    dk::core::free( memoryAllocator, probeConvolutionCmdAllocator );

#if DUSK_DEVBUILD
    MaterialDebugIBLProbe = nullptr;
#endif
}

#if DUSK_DEVBUILD
void DrawCommandBuilder::loadDebugResources( GraphicsAssetCache* graphicsAssetCache )
{
    MaterialDebugIBLProbe = graphicsAssetCache->getMaterial( DUSK_STRING( "GameData/materials/Debug/IBLProbe.mat" ) );
    MaterialDebugWireframe = graphicsAssetCache->getMaterial( DUSK_STRING( "GameData/materials/Debug/Wireframe.mat" ) );
}
#endif

void DrawCommandBuilder::addGeometryToRender( const Mesh* meshResource, const dkMat4x4f* modelMatrix, const uint32_t flagset )
{
    auto* mesh = dk::core::allocate<MeshInstance>( meshes );
    mesh->mesh = meshResource;
    mesh->modelMatrix = modelMatrix;
    mesh->flags = flagset;
}

void DrawCommandBuilder::addSphereToRender( const dkVec3f& sphereCenter, const float sphereRadius, Material* material )
{
    auto sphereMatrix = dk::core::allocate<PrimitiveInstance>( spheresToRender );
    sphereMatrix->modelMatrix = dk::maths::MakeTranslationMat( sphereCenter ) *  dk::maths::MakeScaleMat( sphereRadius );
    sphereMatrix->material = material;
}

void DrawCommandBuilder::addAABBToRender( const AABB& aabb, Material* material )
{
    auto sphereMatrix = dk::core::allocate<PrimitiveInstance>( spheresToRender );
    sphereMatrix->modelMatrix = dk::maths::MakeTranslationMat( dk::maths::GetAABBCentroid( aabb ) ) *  dk::maths::MakeScaleMat( dk::maths::GetAABBHalfExtents( aabb ) );
    sphereMatrix->material = material;
}

void DrawCommandBuilder::addCamera( CameraData* cameraData )
{
    auto camera = dk::core::allocate<CameraData*>( cameras );
    *camera = cameraData;
}
//
//void DrawCommandBuilder::addIBLProbeToCapture( const IBLProbeData* probeData )
//{
//    for ( uint32_t i = 0; i < 6; i++ ) {
//        auto* probeCaptureCmd = dk::core::allocate<IBLProbeCaptureCommand>( probeCaptureCmdAllocator );
//        probeCaptureCmd->CommandInfos.EnvProbeArrayIndex = probeData->ProbeIndex;
//        probeCaptureCmd->CommandInfos.Step = static_cast<eProbeCaptureStep>( i );
//        probeCaptureCmd->Probe = probeData;
//
//        probeCaptureCmds.push( probeCaptureCmd );
//
//        for ( uint16_t mipIndex = 0; mipIndex < 8; mipIndex++ ) {
//            auto* probeConvolutionCmd = dk::core::allocate<IBLProbeConvolutionCommand>( probeConvolutionCmdAllocator );
//            probeConvolutionCmd->CommandInfos.EnvProbeArrayIndex = probeData->ProbeIndex;
//            probeConvolutionCmd->CommandInfos.Step = static_cast<eProbeCaptureStep>( i );
//            probeConvolutionCmd->CommandInfos.MipIndex = mipIndex;
//            probeConvolutionCmd->Probe = probeData;
//
//            probeConvolutionCmds.push( probeConvolutionCmd );
//        }
//    }
//}

void DrawCommandBuilder::addHUDRectangle( const dkVec2f& positionScreenSpace, const dkVec2f& dimensionScreenSpace, const float rotationInRadians, Material* material )
{
    dkMat4x4f mat1 = dk::maths::MakeTranslationMat( dkVec3f( positionScreenSpace, 0.0f ) );

    dkMat4x4f mat2 = dk::maths::MakeTranslationMat( dkVec3f( 0.5f * dimensionScreenSpace.x, 0.5f * dimensionScreenSpace.y, 0.0f ), mat1 );
    dkMat4x4f mat3 = dk::maths::MakeRotationMatrix( rotationInRadians, dkVec3f( 0.0f, 0.0f, 1.0f ), mat2 );
    dkMat4x4f mat4 = dk::maths::MakeTranslationMat( dkVec3f( -0.5f * dimensionScreenSpace.x, -0.5f * dimensionScreenSpace.y, 0.0f ), mat3 );

    dkMat4x4f mat5 = dk::maths::MakeScaleMat( dkVec3f( dimensionScreenSpace, 1.0f ), mat4 );

    auto primInstance = dk::core::allocate<PrimitiveInstance>( primitivesToRender );
    primInstance->modelMatrix = mat5;
    primInstance->material = material;
}

void DrawCommandBuilder::addHUDText( const dkVec2f& positionScreenSpace, const float size, const dkVec4f& colorAndAlpha, const std::string& value )
{
    auto textCmd = dk::core::allocate<TextDrawCommand>( textToRenderAllocator );
    textCmd->stringToPrint = value;
    textCmd->color = colorAndAlpha;
    textCmd->scale = size;
    textCmd->positionScreenSpace = positionScreenSpace;
}

void DrawCommandBuilder::buildRenderQueues( WorldRenderer* worldRenderer, LightGrid* lightGrid )
{
    //uint32_t cameraIdx = 0;
    //CameraData** cameraArray = static_cast<CameraData**>( cameras->getBaseAddress() );
    //const size_t cameraCount = cameras->getAllocationCount();

    //for ( ; cameraIdx < cameraCount; cameraIdx++ ) {
    //    CameraData* camera = cameraArray[cameraIdx];

    //    // Register viewport into the world renderer
    //    FrameGraph& renderPipeline = worldRenderer->allocateFrameGraph( { 0, 0, static_cast<int32_t>( camera->viewportSize.x ), static_cast<int32_t>( camera->viewportSize.y ), 0.0f, 1.0f }, camera );
    //    renderPipeline.setMSAAQuality( camera->msaaSamplerCount );
    //    renderPipeline.setImageQuality( camera->imageQuality );

    //    //worldRenderer->probeCaptureModule->importResourcesToPipeline( &renderPipeline );

    //        auto lightClustersData = lightGrid->updateClusters( &renderPipeline );

    //        auto sunShadowMap = AddCSMCapturePass( &renderPipeline );

    //        auto lightRenderTarget = AddLightRenderPass( &renderPipeline, lightClustersData, sunShadowMap );
    //        auto skyRenderTarget = worldRenderer->SkyRenderModule->renderSky( &renderPipeline, lightRenderTarget.lightRenderTarget, lightRenderTarget.depthRenderTarget );

    //        auto resolvedTarget = AddMSAAResolveRenderPass( &renderPipeline, skyRenderTarget, lightRenderTarget.velocityRenderTarget, lightRenderTarget.depthRenderTarget, camera->msaaSamplerCount, camera->flags.enableTAA );
    //        AddCurrentFrameSaveRenderPass( &renderPipeline, resolvedTarget );

    //        if ( camera->imageQuality != 1.0f ) {
    //            resolvedTarget = AddCopyAndDownsampleRenderPass( &renderPipeline, resolvedTarget, static_cast< uint32_t >( camera->viewportSize.x ), static_cast< uint32_t >( camera->viewportSize.y ) );
    //        }

    //        worldRenderer->automaticExposureModule->computeExposure( &renderPipeline, resolvedTarget, camera->viewportSize );

    //        auto fftInput = AddCopyAndDownsampleRenderPass( &renderPipeline, resolvedTarget, 512, 256 );
    //        auto fftOuput = AddFFTComputePass( &renderPipeline, fftInput );
    //        auto ifftOutput = AddInverseFFTComputePass( &renderPipeline, fftOuput.RealPartBuffer, fftOuput.ImaginaryPartBuffer );

    //        auto blurPyramid = AddBlurPyramidRenderPass( &renderPipeline, resolvedTarget, static_cast<uint32_t>( camera->viewportSize.x ), static_cast<uint32_t>( camera->viewportSize.y ) );

    //        // NOTE UI Rendering should be done in linear space! (once async compute is implemented, UI rendering will be parallelized with PostFx)
    //        auto hudRenderTarget = AddHUDRenderPass( &renderPipeline, resolvedTarget );
    //        hudRenderTarget = worldRenderer->TextRenderModule->renderText( &renderPipeline, hudRenderTarget );
    //        hudRenderTarget = worldRenderer->LineRenderModule->addLineRenderPass( &renderPipeline, hudRenderTarget );
    //        
    //        auto postFxRenderTarget = AddFinalPostFxRenderPass( &renderPipeline, resolvedTarget, blurPyramid );
    //        AddPresentRenderPass( &renderPipeline, postFxRenderTarget );
    //    
    //    // CSM Capture
    //    // TODO Check if we can skip CSM capture depending on sun orientation?
    //    const DirectionalLightData* sunLight = lightGrid->getDirectionalLightData();

    //    camera->globalShadowMatrix = dk::framework::CSMCreateGlobalShadowMatrix( sunLight->direction, camera->depthViewProjectionMatrix );

    //    for ( int sliceIdx = 0; sliceIdx < CSM_SLICE_COUNT; sliceIdx++ ) {
    //        dk::framework::CSMComputeSliceData( sunLight, sliceIdx, camera );
    //    }

    //    camera->globalShadowMatrix = camera->globalShadowMatrix.transpose();

    //    // Create temporary frustum to cull geometry
    //    Frustum csmCameraFrustum;
    //    for ( int sliceIdx = 0; sliceIdx < CSM_SLICE_COUNT; sliceIdx++ ) {
    //        dk::maths::UpdateFrustumPlanes( camera->shadowViewMatrix[sliceIdx], csmCameraFrustum );

    //        // Cull static mesh instances (depth viewport)
    //        buildMeshDrawCmds( worldRenderer, camera, static_cast< uint8_t >( cameraIdx ), DrawCommandKey::LAYER_DEPTH, static_cast<DrawCommandKey::WorldViewportLayer>( DrawCommandKey::DEPTH_VIEWPORT_LAYER_CSM0 + sliceIdx ) );
    //    }

    //    // Cull static mesh instances (world viewport)
    //    buildMeshDrawCmds( worldRenderer, camera, static_cast< uint8_t >( cameraIdx ), DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT );
    //    buildHUDDrawCmds( worldRenderer, camera, static_cast< uint8_t >( cameraIdx ) );
    //}

    //if ( !probeCaptureCmds.empty() ) {
    //    IBLProbeCaptureCommand* cmd = probeCaptureCmds.top();

    //    // Tweak probe field of view to avoid visible seams
    //    const float ENV_PROBE_FOV = 2.0f * atanf( IBL_PROBE_DIMENSION / ( IBL_PROBE_DIMENSION - 0.5f ) );
    //    constexpr float ENV_PROBE_ASPECT_RATIO = ( IBL_PROBE_DIMENSION / static_cast<float>( IBL_PROBE_DIMENSION ) );

    //    CameraData probeCamera = {};
    //    probeCamera.worldPosition = cmd->Probe->worldPosition;

    //    probeCamera.projectionMatrix = dk::maths::MakeInfReversedZProj( ENV_PROBE_FOV, ENV_PROBE_ASPECT_RATIO, 0.01f );
    //    probeCamera.inverseProjectionMatrix = probeCamera.projectionMatrix.inverse();
    //    probeCamera.depthProjectionMatrix = dk::maths::MakeFovProj( ENV_PROBE_FOV, 1.0f, 1.0f, 125.0f );

    //    probeCamera.viewMatrix = GetProbeCaptureViewMatrix( cmd->Probe->worldPosition, cmd->CommandInfos.Step );
    //    probeCamera.depthViewProjectionMatrix = probeCamera.depthProjectionMatrix * probeCamera.viewMatrix;

    //    probeCamera.inverseViewMatrix = probeCamera.viewMatrix.inverse();

    //    probeCamera.viewProjectionMatrix = probeCamera.projectionMatrix * probeCamera.viewMatrix;
    //    probeCamera.inverseViewProjectionMatrix = probeCamera.viewProjectionMatrix.inverse();

    //    probeCamera.viewportSize = { IBL_PROBE_DIMENSION, IBL_PROBE_DIMENSION };
    //    probeCamera.imageQuality = 1.0f;
    //    probeCamera.msaaSamplerCount = 1;

    //    FrameGraph& renderPipeline = worldRenderer->allocateFrameGraph( { 0, 0, IBL_PROBE_DIMENSION, IBL_PROBE_DIMENSION, 0.0f, 1.0f }, &probeCamera );
    //    worldRenderer->probeCaptureModule->importResourcesToPipeline( &renderPipeline );

    //    if ( !cmd->Probe->isFallbackProbe ) {
    //        dk::maths::UpdateFrustumPlanes( probeCamera.depthViewProjectionMatrix, probeCamera.frustum );

    //        // CSM Capture
    //        // TODO Check if we can skip CSM capture depending on sun orientation?
    //        const DirectionalLightData* sunLight = lightGrid->getDirectionalLightData();

    //        probeCamera.globalShadowMatrix = dk::framework::CSMCreateGlobalShadowMatrix( sunLight->direction, probeCamera.depthViewProjectionMatrix );

    //        for ( int sliceIdx = 0; sliceIdx < CSM_SLICE_COUNT; sliceIdx++ ) {
    //            dk::framework::CSMComputeSliceData( sunLight, sliceIdx, &probeCamera );
    //        }

    //        probeCamera.globalShadowMatrix = probeCamera.globalShadowMatrix.transpose();

    //        // Create temporary frustum to cull geometry
    //        Frustum csmCameraFrustum;
    //        for ( int sliceIdx = 0; sliceIdx < CSM_SLICE_COUNT; sliceIdx++ ) {
    //            dk::maths::UpdateFrustumPlanes( probeCamera.shadowViewMatrix[sliceIdx], csmCameraFrustum );

    //            // Cull static mesh instances (depth viewport)
    //            buildMeshDrawCmds( worldRenderer, &probeCamera, static_cast< uint8_t >( cameraIdx ), DrawCommandKey::LAYER_DEPTH, static_cast<DrawCommandKey::WorldViewportLayer>( DrawCommandKey::DEPTH_VIEWPORT_LAYER_CSM0 + sliceIdx ) );
    //        }

    //        buildMeshDrawCmds( worldRenderer, &probeCamera, static_cast< uint8_t >( cameraIdx ), DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT );
    //   
    //        auto lightClustersData = lightGrid->updateClusters( &renderPipeline );
    //        auto sunShadowMap = AddCSMCapturePass( &renderPipeline );
    //        auto lightRenderTarget = AddLightRenderPass( &renderPipeline, lightClustersData, sunShadowMap, -1, true );
    //        auto faceRenderTarget = worldRenderer->SkyRenderModule->renderSky( &renderPipeline, lightRenderTarget.lightRenderTarget, lightRenderTarget.depthRenderTarget, false, false );

    //        worldRenderer->probeCaptureModule->saveCapturedProbeFace( &renderPipeline, faceRenderTarget, cmd->CommandInfos.EnvProbeArrayIndex, cmd->CommandInfos.Step );
    //    } else {
    //        auto faceRenderTarget = worldRenderer->SkyRenderModule->renderSky( &renderPipeline, -1, -1, false, false );

    //        worldRenderer->probeCaptureModule->saveCapturedProbeFace( &renderPipeline, faceRenderTarget, cmd->CommandInfos.EnvProbeArrayIndex, cmd->CommandInfos.Step );
    //    }
    //    
    //    cameraIdx++;

    //    probeCaptureCmdAllocator->free( cmd );
    //    probeCaptureCmds.pop();
    //} else if ( !probeConvolutionCmds.empty() ) {
    //    IBLProbeConvolutionCommand* cmd = probeConvolutionCmds.top();

    //    FrameGraph& renderPipeline = worldRenderer->allocateFrameGraph( { 0, 0, IBL_PROBE_DIMENSION, IBL_PROBE_DIMENSION, 0.0f, 1.0f }, nullptr );
    //    worldRenderer->probeCaptureModule->importResourcesToPipeline( &renderPipeline );

    //        worldRenderer->probeCaptureModule->convoluteProbeFace( &renderPipeline, cmd->CommandInfos.EnvProbeArrayIndex, cmd->CommandInfos.Step, cmd->CommandInfos.MipIndex );
    //    
    //    cameraIdx++;

    //    probeConvolutionCmdAllocator->free( cmd );
    //    probeConvolutionCmds.pop();
    //}

    resetEntityCounters();
}

void DrawCommandBuilder::resetEntityCounters()
{
    cameras->clear();
    meshes->clear();
    spheresToRender->clear();
    primitivesToRender->clear();
    textToRenderAllocator->clear();
}

void DrawCommandBuilder::buildMeshDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx, const uint8_t layer, const uint8_t viewportLayer )
{
    //MeshInstance* meshesArray = static_cast<MeshInstance*>( meshes->getBaseAddress() );
    //const size_t meshCount = meshes->getAllocationCount();
    //for ( uint32_t meshIdx = 0; meshIdx < meshCount; meshIdx++ ) {
    //    const MeshInstance& meshInstance = meshesArray[meshIdx];

    //    // TODO Avoid this crappy test per mesh instance (store per-layer list inside the commandBuilder?)
    //    if ( layer == DrawCommandKey::LAYER_DEPTH && meshInstance.renderDepth == 0 ) {
    //        continue;
    //    }

    //    const dkVec3f instancePosition = dk::maths::ExtractTranslation( *meshInstance.modelMatrix );
    //    const float instanceScale = dk::maths::GetBiggestScalar( dk::maths::ExtractScale( *meshInstance.modelMatrix ) );

    //    const float distanceToCamera = dkVec3f::distanceSquared( camera->worldPosition, instancePosition );

    //    // Retrieve LOD based on instance to camera distance
    //    const auto& activeLOD = meshInstance.mesh->getLevelOfDetail( distanceToCamera );

    //    const Buffer* vertexBuffer = meshInstance.mesh->getVertexBuffer();
    //    const Buffer* indiceBuffer = meshInstance.mesh->getIndiceBuffer();

    //    for ( const SubMesh& subMesh : activeLOD.subMeshes ) {
    //        // Transform sphere origin by instance model matrix
    //        dkVec3f position = instancePosition + subMesh.boundingSphere.center;
    //        float scaledRadius = instanceScale * subMesh.boundingSphere.radius;

    //        if ( CullSphereInfReversedZ( &camera->frustum, position, scaledRadius ) > 0.0f ) {
    //            // Build drawcmd is the submesh is visible
    //            DrawCmd& drawCmd = worldRenderer->allocateDrawCmd();

    //            auto& key = drawCmd.key.bitfield;
    //            key.materialSortKey = subMesh.material->getSortKey();
    //            key.depth = DepthToBits( distanceToCamera );
    //            key.sortOrder = ( subMesh.material->isOpaque() ) ? DrawCommandKey::SORT_FRONT_TO_BACK : DrawCommandKey::SORT_BACK_TO_FRONT;
    //            key.layer = static_cast<DrawCommandKey::Layer>( layer );
    //            key.viewportLayer = viewportLayer;
    //            key.viewportId = cameraIdx;

    //            DrawCommandInfos& infos = drawCmd.infos;
    //            infos.material = subMesh.material;
    //            infos.vertexBuffer = vertexBuffer;
    //            infos.indiceBuffer = indiceBuffer;
    //            infos.indiceBufferOffset = subMesh.indiceBufferOffset;
    //            infos.indiceBufferCount = subMesh.indiceCount;
    //            infos.alphaDitheringValue = 1.0f;
    //            infos.instanceCount = 1;
    //            infos.modelMatrix = meshInstance.modelMatrix;
    //        }
    //    }
    //}

    //PrimitiveInstance* sphereToRenderArray = static_cast<PrimitiveInstance*>( spheresToRender->getBaseAddress() );
    //const size_t sphereCount = spheresToRender->getAllocationCount();

    //for ( uint32_t sphereIdx = 0; sphereIdx < sphereCount; sphereIdx++ ) {
    //    sphereToRender[sphereIdx] = sphereToRenderArray[sphereIdx].modelMatrix.transpose();

    //    const dkVec3f instancePosition = dk::maths::ExtractTranslation( sphereToRender[sphereIdx] );
    //    const float distanceToCamera = dkVec3f::distanceSquared( camera->worldPosition, instancePosition );

    //    DrawCmd& drawCmd = worldRenderer->allocateSpherePrimitiveDrawCmd();
    //    drawCmd.infos.material = sphereToRenderArray[sphereIdx].material;
    //    drawCmd.infos.instanceCount = static_cast<uint32_t>( 1u );
    //    drawCmd.infos.modelMatrix = &sphereToRender[sphereIdx];

    //    auto& key = drawCmd.key.bitfield;
    //    key.materialSortKey = sphereToRenderArray[sphereIdx].material->getSortKey();
    //    key.depth = DepthToBits( distanceToCamera );
    //    key.sortOrder = DrawCommandKey::SORT_FRONT_TO_BACK;
    //    key.layer = static_cast< DrawCommandKey::Layer >( layer );
    //    key.viewportLayer = viewportLayer;
    //    key.viewportId = cameraIdx;
    //}
}

void DrawCommandBuilder::buildHUDDrawCmds( WorldRenderer* worldRenderer, CameraData* camera, const uint8_t cameraIdx )
{
    //PrimitiveInstance* primitivesToRenderArray = static_cast<PrimitiveInstance*>( primitivesToRender->getBaseAddress() );
    //const size_t primitiveCount = primitivesToRender->getAllocationCount();

    //for ( uint32_t primIdx = 0; primIdx < primitiveCount; primIdx++ ) {
    //    primitivesModelMatricess[primIdx] = primitivesToRenderArray[primIdx].modelMatrix.transpose();

    //    DrawCmd& drawCmd = worldRenderer->allocateRectanglePrimitiveDrawCmd();
    //    drawCmd.infos.material = primitivesToRenderArray[primIdx].material;
    //    drawCmd.infos.instanceCount = static_cast< uint32_t >( 1u );
    //    drawCmd.infos.modelMatrix = &primitivesModelMatricess[primIdx];

    //    auto& key = drawCmd.key.bitfield;
    //    key.materialSortKey = primitivesToRenderArray[primIdx].material->getSortKey();
    //    key.depth = DepthToBits( 0.0f );
    //    key.sortOrder = DrawCommandKey::SORT_BACK_TO_FRONT;
    //    key.layer = DrawCommandKey::Layer::LAYER_HUD;
    //    key.viewportLayer = DrawCommandKey::HUDViewportLayer::HUD_VIEWPORT_LAYER_DEFAULT;
    //    key.viewportId = cameraIdx;
    //}

    //TextDrawCommand* textDrawCmdArray = static_cast<TextDrawCommand*>( textToRenderAllocator->getBaseAddress() );
    //const size_t textToDrawCount = textToRenderAllocator->getAllocationCount();

    //for ( uint32_t textIdx = 0; textIdx < textToDrawCount; textIdx++ ) {
    //    const TextDrawCommand& drawCmd = textDrawCmdArray[textIdx];
    //    worldRenderer->TextRenderModule->addOutlinedText( drawCmd.stringToPrint.c_str(), drawCmd.scale, drawCmd.positionScreenSpace.x, drawCmd.positionScreenSpace.y, drawCmd.color );
    //}
}
#endif