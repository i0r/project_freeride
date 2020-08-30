/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "RenderWorld.h"

#include <Core/Allocators/PoolAllocator.h>

#include <Graphics/DrawCommandBuilder.h>
#include <Graphics/ModelBuilder.h>
#include <Graphics/Model.h>
#include <Graphics/Mesh.h>
#include <Graphics/Material.h>
#include <Graphics/GraphicsAssetCache.h>

#include "Graphics/ShaderHeaders/Light.h"

#include "Rendering/CommandList.h"

#include <array>
#include <vector>

static constexpr i32 MAX_VERTEX_COUNT = 10000 * RenderWorld::MAX_MODEL_COUNT;

RenderWorld::RenderWorld( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , modelAllocator( dk::core::allocate<PoolAllocator>( 
        allocator, 
        sizeof( Model ), 
        static_cast<u8>( alignof( Model ) ), 
        sizeof( Model ) * RenderWorld::MAX_MODEL_COUNT, 
        allocator->allocate( sizeof( Model )* RenderWorld::MAX_MODEL_COUNT ) 
    ) )
    , modelCount( 0 )
    , shadowCasterVertexBuffer( nullptr )
    , shadowCasterIndexBuffer( nullptr )
    , vertexBufferData( dk::core::allocateArray<f32>( allocator, 3 * MAX_VERTEX_COUNT ) )
    , indiceBufferData( dk::core::allocateArray<u32>( allocator, 3 * MAX_VERTEX_COUNT ) )
    , vertexBufferDirtyOffset( ~0u )
    , vertexBufferDirtyLength( 0u )
    , indiceBufferDirtyOffset( ~0u )
    , indiceBufferDirtyLength( 0u )
    , vertexBufferUsageOfffset( 0u )
    , indiceBufferUsageOfffset( 0u )
    , gpuShadowMeshCount( 0u )
    , gpuShadowFreeListLength( 0u )
    , indexOffset( 0u )
    , gpuShadowBatches( dk::core::allocateArray<MeshConstants>( allocator, MAX_MODEL_COUNT ) )
    , gpuMeshInfos( nullptr )
    , isGpuMeshInfosDirty( false )
{
    memset( modelList, 0, sizeof( Model* ) * MAX_MODEL_COUNT );
    memset( gpuShadowBatches, 0, sizeof( MeshConstants ) * MAX_MODEL_COUNT );
}

RenderWorld::~RenderWorld()
{
    dk::core::freeArray( memoryAllocator, vertexBufferData );
    dk::core::freeArray( memoryAllocator, indiceBufferData );
    dk::core::freeArray( memoryAllocator, gpuShadowBatches );
    dk::core::free( memoryAllocator, modelAllocator );
}

void RenderWorld::create( RenderDevice& renderDevice )
{
    BufferDesc shadowVbDesc;
    shadowVbDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    shadowVbDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER | RESOURCE_BIND_RAW | RESOURCE_BIND_SHADER_RESOURCE;
    shadowVbDesc.SizeInBytes = sizeof( dkVec3f ) * 10000 * MAX_MODEL_COUNT;
    shadowVbDesc.StrideInBytes = sizeof( dkVec3f );
    shadowVbDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

    shadowCasterVertexBuffer = renderDevice.createBuffer( shadowVbDesc );
    
    BufferDesc shadowIbDesc;
    shadowIbDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    shadowIbDesc.BindFlags = RESOURCE_BIND_INDICE_BUFFER | RESOURCE_BIND_SHADER_RESOURCE;
    shadowIbDesc.SizeInBytes = sizeof( u32 ) * 10000 * MAX_MODEL_COUNT;
    shadowIbDesc.StrideInBytes = sizeof( u32 );
    shadowIbDesc.DefaultView.ViewFormat = VIEW_FORMAT_R32_UINT;

    shadowCasterIndexBuffer = renderDevice.createBuffer( shadowIbDesc );

    BufferDesc meshInfosBufferDesc;
    meshInfosBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    meshInfosBufferDesc.BindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_STRUCTURED_BUFFER;
    meshInfosBufferDesc.SizeInBytes = sizeof( MeshConstants ) * MAX_MODEL_COUNT;
    meshInfosBufferDesc.StrideInBytes = sizeof( MeshConstants );
    
    gpuMeshInfos = renderDevice.createBuffer( meshInfosBufferDesc );
    
#if DUSK_DEVBUILD
	renderDevice.setDebugMarker( *shadowCasterVertexBuffer, DUSK_STRING( "Shadow Casters VertexBuffer" ) );
	renderDevice.setDebugMarker( *shadowCasterIndexBuffer, DUSK_STRING( "Shadow Casters IndexBuffer" ) );
#endif
}

void RenderWorld::destroy( RenderDevice& renderDevice )
{
    if ( shadowCasterVertexBuffer != nullptr ) {
        renderDevice.destroyBuffer( shadowCasterVertexBuffer );
        shadowCasterVertexBuffer = nullptr;
    }
    
    if ( shadowCasterIndexBuffer != nullptr ) {
        renderDevice.destroyBuffer( shadowCasterIndexBuffer );
        shadowCasterIndexBuffer = nullptr;
    }
    
    if ( gpuMeshInfos != nullptr ) {
        renderDevice.destroyBuffer( gpuMeshInfos );
        gpuMeshInfos = nullptr;
    }
}

Model* RenderWorld::addAndCommitParsedDynamicModel( RenderDevice* renderDevice, ParsedModel& parsedModel, GraphicsAssetCache* graphicsAssetCache )
{
    Model* model = dk::core::allocate<Model>( modelAllocator, memoryAllocator, DUSK_STRING( "RenderWorldModel" ) );

    dk::graphics::BuildParsedModel( model, memoryAllocator, renderDevice, parsedModel );

    // TODO Until FBX materials are supported we stupidly assign the default material to each mesh.
    Material* defaultMaterial = graphicsAssetCache->getDefaultMaterial();
    for ( i32 i = 0; i < model->getLevelOfDetailCount(); i++ ) {
        Model::LevelOfDetail& lod = model->getLevelOfDetailForEditor( i );

        for ( i32 j = 0; j < lod.MeshCount; j++ ) {
            Mesh& mesh = lod.MeshArray[j];
            mesh.RenderMaterial = defaultMaterial;
            
            const bool isValidMesh = ( mesh.PositionVertices != nullptr && mesh.Indices != nullptr );
			if ( isValidMesh ) {
				// Allocate data for GPU-driven shadow rendering.
                MeshConstants meshInfos;
                mesh.RenderWorldIndex = allocateGpuMeshInfos( meshInfos, mesh.VertexCount * 3, mesh.FaceCount, mesh.IndiceCount );

                mesh.AttributeBuffers[eMeshAttribute::Position] = BufferBinding( shadowCasterVertexBuffer, meshInfos.VertexOffset );

                // Update CPU Buffers and mark dirty ranges for the next GPU buffers update.
                u32 vertexOffsetInElements = meshInfos.VertexOffset / sizeof( f32 );
                u32 indexOffsetInElements = meshInfos.IndexOffset / sizeof( i32 );

                memcpy( &vertexBufferData[vertexOffsetInElements], mesh.PositionVertices, sizeof( f32 ) * 3 * mesh.VertexCount );
				memcpy( &indiceBufferData[indexOffsetInElements], mesh.Indices, sizeof( u32 ) * mesh.IndiceCount );

				vertexBufferDirtyOffset = Min( vertexBufferDirtyOffset, vertexOffsetInElements );
				vertexBufferDirtyLength = Max( vertexBufferDirtyLength, vertexBufferDirtyOffset + meshInfos.VertexCount );

				indiceBufferDirtyOffset = Min( indiceBufferDirtyOffset, indexOffsetInElements );
				indiceBufferDirtyLength = Max( indiceBufferDirtyLength, indiceBufferDirtyOffset + mesh.IndiceCount );
            
                // Create clusters for this mesh.
                createMeshClusters( mesh.RenderWorldIndex, mesh.IndiceCount, mesh.PositionVertices, mesh.Indices );
            }
        }
    }

    modelList[modelCount++] = model;
    
    return model;
}

void RenderWorld::update( RenderDevice* renderDevice )
{
    const bool isVertexBufferDirty = ( vertexBufferDirtyOffset != ~0 );
    const bool isIndiceBufferDirty = ( indiceBufferDirtyOffset != ~0 );

    // TODO Do explicit range update to avoid remapping the whole buffer...
    CommandList& cmdList = renderDevice->allocateCopyCommandList();
    cmdList.begin();

    if ( isVertexBufferDirty ) {
        void* vertexBufferDataPtr = cmdList.mapBuffer( *shadowCasterVertexBuffer, vertexBufferDirtyOffset, vertexBufferDirtyLength );
        
        if ( vertexBufferDataPtr != nullptr ) {
            memcpy( vertexBufferDataPtr, vertexBufferData, sizeof( f32 ) * 3 * MAX_VERTEX_COUNT );
            cmdList.unmapBuffer( *shadowCasterVertexBuffer );
            
            vertexBufferDirtyLength = 0;
            vertexBufferDirtyOffset = ~0;
        }
    }
    
    if ( isIndiceBufferDirty ) {
        void* indiceBufferDataPtr = cmdList.mapBuffer( *shadowCasterIndexBuffer, indiceBufferDirtyOffset, indiceBufferDirtyLength );
        
        if ( indiceBufferDataPtr != nullptr ) {
            memcpy( indiceBufferDataPtr, indiceBufferData, sizeof( u32 ) * MAX_VERTEX_COUNT );
            cmdList.unmapBuffer( *shadowCasterIndexBuffer );
            
            indiceBufferDirtyLength = 0;
            indiceBufferDirtyOffset = ~0;
        }
    }

    if ( isGpuMeshInfosDirty ) {
        // TODO Use range update instead?
        cmdList.updateBuffer( *gpuMeshInfos, gpuShadowBatches, sizeof( MeshConstants ) * MAX_MODEL_COUNT );
        isGpuMeshInfosDirty = false;
    }

    cmdList.end();
    renderDevice->submitCommandList( cmdList );
}

i32 RenderWorld::allocateGpuMeshInfos( MeshConstants& allocatedBatch, const u32 vertexCount, const u32 faceCount, const u32 indiceCount )
{
    isGpuMeshInfosDirty = true;

    // Check if one of the freelist entry is suitable for our allocation.
    // Free batch infos are stored at the end of the array.
    i32 freeListEnd = ( gpuShadowMeshCount + gpuShadowFreeListLength );
    for ( i32 i = gpuShadowMeshCount; i < freeListEnd; i++ ) {
        if ( gpuShadowBatches[i].VertexCount >= vertexCount
          && gpuShadowBatches[i].FaceCount >= faceCount ) {
            MeshConstants& batch = gpuShadowBatches[i];
            
            // We don't need to swap anything if the free node is the first node of the list
            const bool skipSwap = ( gpuShadowFreeListLength == i || gpuShadowFreeListLength == 1 );
            
            if ( !skipSwap ) {
                // Move the free node at the end of the data array (to keep things continous).
                MeshConstants batchDst = gpuShadowBatches[gpuShadowMeshCount];         
                gpuShadowBatches[gpuShadowMeshCount] = gpuShadowBatches[i];
                gpuShadowBatches[i] = batchDst;      

                batch = gpuShadowBatches[gpuShadowMeshCount];
            }
            
            gpuShadowFreeListLength--;
            gpuShadowMeshCount++;
            
            allocatedBatch = batch;

            return i;
        }   
    }
    
    // Swap nodes to keep the first node of the freelist.
    if ( gpuShadowFreeListLength > 0 ) {
        MeshConstants batchDst = gpuShadowBatches[gpuShadowMeshCount];         
        gpuShadowBatches[gpuShadowMeshCount] = gpuShadowBatches[freeListEnd];
        gpuShadowBatches[freeListEnd] = batchDst;      
    }
    
    // Allocate a new batch info.
    const i32 allocatedBatchIndex = gpuShadowMeshCount;

    MeshConstants& meshBatchInfos = gpuShadowBatches[gpuShadowMeshCount++];
    meshBatchInfos.VertexOffset = vertexBufferUsageOfffset * sizeof( f32 );
    meshBatchInfos.VertexCount = vertexCount;
    meshBatchInfos.IndexOffset = indiceBufferUsageOfffset * sizeof( i32 );
    meshBatchInfos.FaceCount = faceCount;

	allocatedBatch = meshBatchInfos;

    vertexBufferUsageOfffset += vertexCount;
    indiceBufferUsageOfffset += indiceCount;
    
    return allocatedBatchIndex;
}

void RenderWorld::createMeshClusters( const i32 meshConstantsIdx, const u32 indexCount, const f32* vertices, const u32* indices )
{
    struct BasicTriangle {
        dkVec3f Vertex[3];
    };
    
    std::array<BasicTriangle, CascadedShadowRenderModule::BATCH_SIZE * 3> triangleCache;

    const i32 triangleCount = indexCount / 3;
    const i32 clusterCount = (triangleCount + CascadedShadowRenderModule::BATCH_SIZE - 1) / CascadedShadowRenderModule::BATCH_SIZE;
    
    if ( Clusters.size() <= meshConstantsIdx ) {
        Clusters.resize( meshConstantsIdx + 1 );
    }

    std::vector<MeshCluster>& cluster = Clusters.at( meshConstantsIdx );
    cluster.resize( clusterCount );
    for (i32 i = 0; i < clusterCount; ++i) {
        const i32 clusterStart = i * CascadedShadowRenderModule::BATCH_SIZE;
        const i32 clusterEnd = Min( clusterStart + CascadedShadowRenderModule::BATCH_SIZE, triangleCount);

        const i32 clusterTriangleCount = clusterEnd - clusterStart;

        // Load all triangles into our local cache
        for (i32 triangleIndex = clusterStart; triangleIndex < clusterEnd; ++triangleIndex) {
            triangleCache[triangleIndex - clusterStart].Vertex[0] = dkVec3f(
                vertices[indices[triangleIndex * 3 + 0] * 3 + 0],
                vertices[indices[triangleIndex * 3 + 0] * 3 + 1],
                vertices[indices[triangleIndex * 3 + 0] * 3 + 2]
            );

            triangleCache[triangleIndex - clusterStart].Vertex[1] = dkVec3f(
                vertices[indices[triangleIndex * 3 + 1] * 3 + 0],
                vertices[indices[triangleIndex * 3 + 1] * 3 + 1],
                vertices[indices[triangleIndex * 3 + 1] * 3 + 2]
            );

            triangleCache[triangleIndex - clusterStart].Vertex[2] = dkVec3f(
                vertices[indices[triangleIndex * 3 + 2] * 3 + 0],
                vertices[indices[triangleIndex * 3 + 2] * 3 + 1],
                vertices[indices[triangleIndex * 3 + 2] * 3 + 2]
            );
        }

        dkVec3f aabbMin = dkVec3f::Max;
        dkVec3f aabbMax = -dkVec3f::Max;
        
        dkVec3f coneAxis = dkVec3f::Zero;
        
        for (i32 triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex) {
            const BasicTriangle& triangle = triangleCache[triangleIndex];
            for (i32 j = 0; j < 3; ++j) {
                aabbMin = dkVec3f::min(aabbMin, triangle.Vertex[j]);
                aabbMax = dkVec3f::max(aabbMax, triangle.Vertex[j]);
            }
            
            const dkVec3f triangleNormal = dkVec3f::cross( ( triangle.Vertex[1] - triangle.Vertex[0] ), ( triangle.Vertex[2] - triangle.Vertex[0] ) ).normalize();

            coneAxis += -triangleNormal;
        }

        // This is the cosine of the cone opening angle - 1 means it's 0°,
        // we're minimizing this value (at 0, it would mean the cone is 90°
        // open)
        f32 coneOpening = 1.0f;

        dkVec3f center = ( aabbMin + aabbMax ) * 0.5f;
        coneAxis = coneAxis.normalize();
        
        f32 t = -std::numeric_limits<f32>::infinity ();

        // We nee a second pass to find the intersection of the line
        // center + t * coneAxis with the plane defined by each
        // triangle
        for (i32 triangleIndex = 0; triangleIndex < clusterTriangleCount; ++triangleIndex) {
            const BasicTriangle& triangle = triangleCache[triangleIndex];
            
            // Compute the triangle plane from the three vertices
            const dkVec3f triangleNormal = dkVec3f::cross( ( triangle.Vertex[1] - triangle.Vertex[0] ), ( triangle.Vertex[2] - triangle.Vertex[0] ) ).normalize();
            const f32 directionalPart = dkVec3f::dot( coneAxis, -triangleNormal );
            
            if ( directionalPart < 0.0f ) {
                // No solution for this cluster - at least two triangles
                // are facing each other
                break;
            }

            // We need to intersect the plane with our cone ray which is
            // center + t * coneAxis, and find the max
            // t along the cone ray (which points into the empty
            // space)
            // See: https://en.wikipedia.org/wiki/Line%E2%80%93plane_intersection
            
            const f32 td = dkVec3f::dot( ( center - triangle.Vertex[0] ), triangleNormal ) / -directionalPart;
            t = Max(t, td);

            coneOpening = Min(coneOpening, directionalPart);
        }

        cluster[i].AABBMax = aabbMax;
        cluster[i].AABBMin = aabbMin;

        // cos (PI/2 - acos (coneOpening))
        cluster[i].ConeAngleCosine = sqrtf(1.0f - coneOpening * coneOpening);
        cluster[i].ConeCenter = center + coneAxis * t;
        cluster[i].ConeAxis = coneAxis;
    }
}
