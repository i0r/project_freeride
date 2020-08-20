/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "DrawCommandBuilder.h"

#include <Framework/Cameras/Camera.h>

#include <Graphics/LightGrid.h>
#include <Graphics/Model.h>
#include <Graphics/Mesh.h>
#include "Graphics/WorldRenderer.h"

#include <Core/Allocators/LinearAllocator.h>
#include <Maths/MatrixTransformations.h>

#include "Graphics/ShaderHeaders/Light.h"

DUSK_DEV_VAR( DisplayBoundingSphere, "Display Geometry Bounding Sphere (as wireframe primitive)", false, bool );

static constexpr size_t MAX_SIMULTANEOUS_VIEWPORT_COUNT = 8;
static constexpr size_t MAX_STATIC_MODEL_COUNT = 4096;
static constexpr size_t MAX_INSTANCE_COUNT_PER_MODEL = 256;

struct ModelInstance 
{
    const Model*    ModelResource;
    dkStringHash_t  EntityIdentifier;
    dkMat4x4f		ModelMatrix;
};

DUSK_INLINE f32 DistanceToPlane( const dkVec4f& vPlane, const dkVec3f& vPoint )
{
    return dkVec4f::dot( dkVec4f( vPoint, 1.0f ), vPlane );
}

DUSK_INLINE u32 FloatFlip( u32 f )
{
    u32 mask = -i32( f >> 31 ) | 0x80000000;
    return f ^ mask;
}

// Taking highest 10 bits for rough sort of floats.
// 0.01 maps to 752; 0.1 to 759; 1.0 to 766; 10.0 to 772;
// 100.0 to 779 etc. Negative numbers go similarly in 0..511 range.
u16 DepthToBits( const f32 depth )
{
    union { f32 f; u32 i; } f2i;
    f2i.f = depth;
    f2i.i = FloatFlip( f2i.i ); // flip bits to be sortable
    u32 b = f2i.i >> 22; // take highest 10 bits
    return static_cast< u16 >( b );
}

// Frustum culling on a sphere. Returns > 0 if visible, <= 0 otherwise
// NOTE Infinite Z version (it implicitly skips the far plane check)
f32 CullSphereInfReversedZ( const Frustum* frustum, const dkVec3f& vCenter, f32 fRadius )
{
    f32 dist01 = Min( DistanceToPlane( frustum->planes[0], vCenter ), DistanceToPlane( frustum->planes[1], vCenter ) );
    f32 dist23 = Min( DistanceToPlane( frustum->planes[2], vCenter ), DistanceToPlane( frustum->planes[3], vCenter ) );
    f32 dist45 = DistanceToPlane( frustum->planes[5], vCenter );

    return Min( Min( dist01, dist23 ), dist45 ) + fRadius;
}

f32 CullSphereInfReversedZ( const Frustum* frustum, const BoundingSphere& sphere )
{
    return CullSphereInfReversedZ( frustum, sphere.center, sphere.radius );
}

struct LODBatch 
{
    const Model::LevelOfDetail*     ModelLOD;
    DrawCommandInfos::InstanceData* Instances;
    u32                             InstanceCount;
    f32                             ClosestDistance;
};

DrawCommandBuilder::DrawCommandBuilder( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , cameraToRenderAllocator( dk::core::allocate<LinearAllocator>( allocator, MAX_SIMULTANEOUS_VIEWPORT_COUNT * sizeof( CameraData ), allocator->allocate( MAX_SIMULTANEOUS_VIEWPORT_COUNT * sizeof( CameraData ) ) ) )
    , staticModelsToRender( dk::core::allocate<LinearAllocator>( allocator, MAX_STATIC_MODEL_COUNT * sizeof( ModelInstance ), allocator->allocate( MAX_STATIC_MODEL_COUNT * sizeof( ModelInstance ) ) ) )
    , instanceDataAllocator( dk::core::allocate<LinearAllocator>( allocator, MAX_STATIC_MODEL_COUNT * MAX_INSTANCE_COUNT_PER_MODEL * sizeof( DrawCommandInfos::InstanceData ), allocator->allocate( MAX_STATIC_MODEL_COUNT * MAX_INSTANCE_COUNT_PER_MODEL * sizeof( DrawCommandInfos::InstanceData ) ) ) )
	, gpuInstanceDataAllocator( dk::core::allocate<LinearAllocator>( allocator, MAX_STATIC_MODEL_COUNT* MAX_INSTANCE_COUNT_PER_MODEL * sizeof( GPUBatchData ), allocator->allocate( MAX_STATIC_MODEL_COUNT* MAX_INSTANCE_COUNT_PER_MODEL * sizeof( GPUBatchData ) ) ) )
{

}

DrawCommandBuilder::~DrawCommandBuilder()
{
	dk::core::free( memoryAllocator, cameraToRenderAllocator );
	dk::core::free( memoryAllocator, staticModelsToRender );
	dk::core::free( memoryAllocator, instanceDataAllocator );
	dk::core::free( memoryAllocator, gpuInstanceDataAllocator );
}

void DrawCommandBuilder::addWorldCameraToRender( CameraData* cameraData )
{
    if ( cameraToRenderAllocator->getAllocationCount() >= MAX_SIMULTANEOUS_VIEWPORT_COUNT ) {
        DUSK_LOG_WARN( "Failed to register camera: too many camera(>=MAX_SIMULTANEOUS_VIEWPORT_COUNT)!\n" );
        return;
    }

    CameraData* camera = dk::core::allocate<CameraData>( cameraToRenderAllocator );
    *camera = *cameraData;
}

void DrawCommandBuilder::addStaticModelInstance( const Model* model, const dkMat4x4f& modelMatrix, const u32 entityIndex )
{
    ModelInstance* modelInstance = dk::core::allocate<ModelInstance>( staticModelsToRender );
    modelInstance->ModelResource = model;
    modelInstance->ModelMatrix = modelMatrix;
    modelInstance->EntityIdentifier = entityIndex;
}

void DrawCommandBuilder::prepareAndDispatchCommands( WorldRenderer* worldRenderer )
{
    u32 cameraIdx = 0;
    CameraData* cameraArray = static_cast< CameraData* >( cameraToRenderAllocator->getBaseAddress() );
    const size_t cameraCount = cameraToRenderAllocator->getAllocationCount();

	for ( ; cameraIdx < cameraCount; cameraIdx++ ) {
		const CameraData& camera = cameraArray[cameraIdx];

		buildGeometryDrawCmds( worldRenderer, &camera, cameraIdx );

		// TODO Conditionally enable this (find a way to check if we should render 
		// CSM, or simply check if a camera wants to render CSM with SDSDM).
		buildShadowGPUDrivenCullCmds( worldRenderer, &camera );
    }

	resetAllocators();
}

void DrawCommandBuilder::buildShadowGPUDrivenCullCmds( WorldRenderer* worldRenderer, const CameraData* camera )
{
	struct LODGPUBatch
	{
		const Model::LevelOfDetail*     ModelLOD;
		GPUBatchData*                   Instances;
		u32                             InstanceCount;
	};

	std::unordered_map<dkStringHash_t, LODGPUBatch> lodBatches;

	ModelInstance* modelsArray = static_cast< ModelInstance* >( staticModelsToRender->getBaseAddress() );
	const size_t modelCount = staticModelsToRender->getAllocationCount();
	for ( u32 modelIdx = 0; modelIdx < modelCount; modelIdx++ ) {
		const ModelInstance& modelInstance = modelsArray[modelIdx];

		const dkMat4x4f& modelMatrix = modelInstance.ModelMatrix;
		const Model* model = modelInstance.ModelResource;
		const u32 entityIdx = modelInstance.EntityIdentifier;

		// Retrieve instance location and scale.
		const dkVec3f instancePosition = dk::maths::ExtractTranslation( modelMatrix );
		const f32 distanceToCamera = dkVec3f::distanceSquared( camera->worldPosition, instancePosition );

		// Ignore far away geometry (should fallback to distant shadows).
		if ( distanceToCamera > ( CSM_MAX_DEPTH * CSM_MAX_DEPTH ) ) {
			continue;
		}

		const f32 instanceScale = dk::maths::GetBiggestScalar( dk::maths::ExtractScale( modelMatrix ) );

		BoundingSphere instanceBoundingSphere = model->getBoundingSphere();
		instanceBoundingSphere.center += instancePosition;
		instanceBoundingSphere.radius *= instanceScale;

		// Retrieve LOD based on instance to camera distance
		const Model::LevelOfDetail& activeLOD = model->getLevelOfDetail( distanceToCamera );

		// Check if a batch already exists for the given LOD.
		auto batchIt = lodBatches.find( activeLOD.Hashcode );
		if ( batchIt == lodBatches.end() ) {
			LODGPUBatch batch;
			batch.ModelLOD = &activeLOD;
			batch.Instances = dk::core::allocateArray<GPUBatchData>( gpuInstanceDataAllocator, MAX_INSTANCE_COUNT_PER_MODEL );
			batch.Instances[0].ModelMatrix = modelMatrix;
			batch.Instances[0].BoundingSphereCenter = instanceBoundingSphere.center;
			batch.Instances[0].BoundingSphereRadius = instanceBoundingSphere.radius;

			batch.InstanceCount = 1;

			lodBatches.insert( std::make_pair( activeLOD.Hashcode, batch ) );
		} else {
			LODGPUBatch& batch = batchIt->second;

			u32 instanceIdx = batch.InstanceCount;
			batch.Instances[instanceIdx].ModelMatrix = modelMatrix;
			batch.Instances[instanceIdx].BoundingSphereCenter = instanceBoundingSphere.center;
			batch.Instances[instanceIdx].BoundingSphereRadius = instanceBoundingSphere.radius;
			batch.InstanceCount++;
		}
	}

	// Merge all the shadow casters into a single vertex buffer.
	for ( auto& lodBatch : lodBatches ) {
		const LODGPUBatch& batch = lodBatch.second;

		const Model::LevelOfDetail* lod = batch.ModelLOD;
		for ( i32 meshIdx = 0; meshIdx < lod->MeshCount; meshIdx++ ) {
			const Mesh& mesh = lod->MeshArray[meshIdx];

			GPUShadowDrawCmd& shadowCullCmd = worldRenderer->allocateGPUShadowCullDrawCmd();
			shadowCullCmd.InstancesData = batch.Instances;
            shadowCullCmd.ShadowMeshBatchIndex = mesh.RenderWorldIndex;
            shadowCullCmd.InstanceCount = batch.InstanceCount;
		}
	}
}

void DrawCommandBuilder::resetAllocators()
{
    cameraToRenderAllocator->clear();
    staticModelsToRender->clear();
    instanceDataAllocator->clear();
	gpuInstanceDataAllocator->clear();
}

template<DrawCommandKey::Layer layer, u8 viewportLayer>
void AddCommand( WorldRenderer* worldRenderer, const LODBatch& batch, const u8 cameraIdx, const Material* material, const Mesh& mesh )
{
    // Allocate a depth render command
    DrawCmd& drawCmd = worldRenderer->allocateDrawCmd();

    // TODO Add back sort key / depth sort / sort Order infos.
    auto& key = drawCmd.key.bitfield;
    key.materialSortKey = 0; // subMesh.material->getSortKey();
    key.depth = DepthToBits( batch.ClosestDistance );
    key.sortOrder = DrawCommandKey::SORT_FRONT_TO_BACK; // ( subMesh.material->isOpaque() ) ? DrawCommandKey::SORT_FRONT_TO_BACK : DrawCommandKey::SORT_BACK_TO_FRONT;
    key.layer = layer;
    key.viewportLayer = viewportLayer;
    key.viewportId = cameraIdx;

    DrawCommandInfos& infos = drawCmd.infos;
    infos.material = material;
    infos.vertexBuffers = mesh.AttributeBuffers;
    infos.indiceBuffer = &mesh.IndexBuffer;
    infos.indiceBufferOffset = mesh.IndiceBufferOffset;
    infos.indiceBufferCount = mesh.IndiceCount;
    infos.alphaDitheringValue = 1.0f;
    infos.instanceCount = batch.InstanceCount;
    infos.modelMatrix = batch.Instances;
}

void DrawCommandBuilder::buildGeometryDrawCmds( WorldRenderer* worldRenderer, const CameraData* camera, const u8 cameraIdx )
{
    std::unordered_map<dkStringHash_t, LODBatch> lodBatches;
    
    DrawCommandInfos::InstanceData* boundingSphereInstanceData = ( DisplayBoundingSphere ) ? dk::core::allocateArray<DrawCommandInfos::InstanceData>( instanceDataAllocator, MAX_INSTANCE_COUNT_PER_MODEL ) : nullptr;
    i32 boundingSphereCount = 0;

    // Do a first pass to perform a basic frustum culling and batch static geometry.
	ModelInstance* modelsArray = static_cast< ModelInstance* >( staticModelsToRender->getBaseAddress() );
	const size_t modelCount = staticModelsToRender->getAllocationCount();
    for ( u32 modelIdx = 0; modelIdx < modelCount; modelIdx++ ) {
        const ModelInstance& modelInstance = modelsArray[modelIdx];

        const dkMat4x4f& modelMatrix = modelInstance.ModelMatrix;
        const Model* model = modelInstance.ModelResource;
        const u32 entityIdx = modelInstance.EntityIdentifier;

        // Retrieve instance location and scale.
		const dkVec3f instancePosition = dk::maths::ExtractTranslation( modelMatrix );
		const f32 instanceScale = dk::maths::GetBiggestScalar( dk::maths::ExtractScale( modelMatrix ) );

        BoundingSphere instanceBoundingSphere = model->getBoundingSphere();
        instanceBoundingSphere.center += instancePosition;
        instanceBoundingSphere.radius *= instanceScale;

		if ( CullSphereInfReversedZ( &camera->frustum, instanceBoundingSphere ) > 0.0f ) {
			const f32 distanceToCamera = dkVec3f::distanceSquared( camera->worldPosition, instancePosition );

			// Retrieve LOD based on instance to camera distance
			const Model::LevelOfDetail& activeLOD = model->getLevelOfDetail( distanceToCamera );

            // Check if a batch already exists for the given LOD.
            auto batchIt = lodBatches.find( activeLOD.Hashcode );
            if ( batchIt == lodBatches.end() ) {
                LODBatch batch;
                batch.ModelLOD = &activeLOD;
				batch.Instances = dk::core::allocateArray<DrawCommandInfos::InstanceData>( instanceDataAllocator, MAX_INSTANCE_COUNT_PER_MODEL );
                batch.ClosestDistance = distanceToCamera;
                batch.Instances[0].ModelMatrix = modelMatrix;
                batch.Instances[0].EntityIdentifier = entityIdx;
				batch.InstanceCount = 1;

                lodBatches.insert( std::make_pair( activeLOD.Hashcode, batch ) );
            } else {
                LODBatch& batch = batchIt->second;

				u32 instanceIdx = batch.InstanceCount;
				batch.Instances[instanceIdx].ModelMatrix = modelMatrix;
				batch.Instances[instanceIdx].EntityIdentifier = entityIdx;
                batch.InstanceCount++;

                batch.ClosestDistance = Min( batch.ClosestDistance, distanceToCamera );
            }

            // Draw debug bounding sphere.
            if ( DisplayBoundingSphere ) {
                dkMat4x4f translationMat = dk::maths::MakeTranslationMat( instanceBoundingSphere.center );
                dkMat4x4f scaleMat = dk::maths::MakeScaleMat( dkVec3f( instanceBoundingSphere.radius ) );

                const u32 bbIndex = boundingSphereCount;
                boundingSphereInstanceData[bbIndex].ModelMatrix = translationMat * scaleMat;
                boundingSphereInstanceData[bbIndex].EntityIdentifier = 0;
                boundingSphereCount++;
            }
        }
    }

    // Build draw commands from the batches.
    for ( auto& lodBatch : lodBatches ) {
        const LODBatch& batch = lodBatch.second;

        const Model::LevelOfDetail* lod = batch.ModelLOD;
        for ( i32 meshIdx = 0; meshIdx < lod->MeshCount; meshIdx++ ) {
            const Mesh& mesh = lod->MeshArray[meshIdx];
            const Material* material = mesh.RenderMaterial;

            AddCommand<DrawCommandKey::LAYER_WORLD, DrawCommandKey::WORLD_VIEWPORT_LAYER_DEFAULT>( worldRenderer, batch, cameraIdx, material, mesh );
            AddCommand<DrawCommandKey::LAYER_DEPTH, DrawCommandKey::DEPTH_VIEWPORT_LAYER_DEFAULT>( worldRenderer, batch, cameraIdx, material, mesh );
        }
    }
}
