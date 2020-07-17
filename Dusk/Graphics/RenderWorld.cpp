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
    , vertexBufferDirtyOffset( 0u )
    , vertexBufferDirtyLength( 0u )
    , indiceBufferDirtyOffset( 0u )
    , indiceBufferDirtyLength( 0u )
    , vertexBufferUsageOfffset( 0u )
    , indiceBufferUsageOfffset( 0u )
    , gpuShadowMeshCount( 0u )
    , gpuShadowFreeListLength( 0u )
    , gpuShadowBatches( dk::core::allocateArray<GPUShadowBatchInfos>( allocator, MAX_MODEL_COUNT ) )
{
    memset( modelList, 0, sizeof( Model* ) * MAX_MODEL_COUNT );
    memset( gpuShadowBatches, 0, sizeof( GPUShadowBatchInfos ) * MAX_MODEL_COUNT );
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
    shadowVbDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    shadowVbDesc.SizeInBytes = sizeof( dkVec3f ) * 10000 * MAX_MODEL_COUNT;
    shadowVbDesc.StrideInBytes = sizeof( dkVec3f );

    shadowCasterVertexBuffer = renderDevice.createBuffer( shadowVbDesc );
    
    BufferDesc shadowIbDesc;
    shadowIbDesc.Usage = RESOURCE_USAGE_DYNAMIC;
    shadowIbDesc.BindFlags = RESOURCE_BIND_INDICE_BUFFER;
    shadowIbDesc.SizeInBytes = sizeof( u32 ) * 10000 * MAX_MODEL_COUNT;
    shadowIbDesc.StrideInBytes = sizeof( u32 );

    shadowCasterIndexBuffer = renderDevice.createBuffer( shadowIbDesc );

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
            
            const bool meshCanCastShadows = ( mesh.PositionVertices != nullptr && mesh.Indices != nullptr );
			if ( mesh.RenderMaterial->castShadow() && meshCanCastShadows ) {
				// Allocate data for GPU-driven shadow rendering.
                GPUShadowBatchInfos meshBatchInfos;
                mesh.ShadowGPUBatchEntryIndex = allocateGpuMeshInfos( meshBatchInfos, mesh.VertexCount, mesh.IndiceCount );

			    memcpy( &vertexBufferData[meshBatchInfos.VertexBufferOffset], mesh.PositionVertices, sizeof( dkVec3f ) * mesh.VertexCount );
				memcpy( &indiceBufferData[meshBatchInfos.IndiceBufferOffset], mesh.Indices, sizeof( u32 ) * mesh.IndiceCount );

				vertexBufferDirtyOffset = Min( vertexBufferDirtyOffset, meshBatchInfos.VertexBufferOffset );
				vertexBufferDirtyLength = Max( vertexBufferDirtyLength, ( meshBatchInfos.VertexBufferOffset + meshBatchInfos.VertexBufferCount ) );

				indiceBufferDirtyOffset = Min( indiceBufferDirtyOffset, meshBatchInfos.IndiceBufferOffset );
				indiceBufferDirtyLength = Max( indiceBufferDirtyLength, ( meshBatchInfos.IndiceBufferOffset + meshBatchInfos.IndiceBufferCount ) );
            }
        }
    }

    modelList[modelCount++] = model;
    
    return model;
}

void RenderWorld::update( RenderDevice* renderDevice )
{
    const bool isVertexBufferDirty = ( vertexBufferDirtyLength > 0 );
    const bool isIndiceBufferDirty = ( indiceBufferDirtyLength > 0 );
    
    CommandList& cmdList = renderDevice->allocateCopyCommandList();
    cmdList.begin();

    if ( isVertexBufferDirty ) {
        void* vertexBufferDataPtr = cmdList.mapBuffer( *shadowCasterVertexBuffer, vertexBufferDirtyOffset, vertexBufferDirtyLength );
        
        if ( vertexBufferDataPtr != nullptr ) {
            memcpy( vertexBufferDataPtr, &vertexBufferData[vertexBufferDirtyOffset], sizeof( f32 ) * vertexBufferDirtyLength );
            cmdList.unmapBuffer( *shadowCasterVertexBuffer );
            
            vertexBufferDirtyLength = 0;
            vertexBufferDirtyOffset = 0;
        }
    }
    
    if ( isIndiceBufferDirty ) {
        void* indiceBufferDataPtr = cmdList.mapBuffer( *shadowCasterIndexBuffer, indiceBufferDirtyOffset, indiceBufferDirtyLength );
        
        if ( indiceBufferDataPtr != nullptr ) {
            memcpy( indiceBufferDataPtr, &indiceBufferData[indiceBufferDirtyOffset], sizeof( u32 ) * indiceBufferDirtyLength );
            cmdList.unmapBuffer( *shadowCasterIndexBuffer );
            
            indiceBufferDirtyLength = 0;
            indiceBufferDirtyOffset = 0;
        }
    }

    cmdList.end();
    renderDevice->submitCommandList( cmdList );
}

i32 RenderWorld::allocateGpuMeshInfos( GPUShadowBatchInfos& allocatedBatch, const u32 vertexCount, const u32 indiceCount )
{
    // Check if one of the freelist entry is suitable for our allocation.
    // Free batch infos are stored at the end of the array.
    i32 freeListEnd = ( gpuShadowMeshCount + gpuShadowFreeListLength );
    for ( i32 i = gpuShadowMeshCount; i < freeListEnd; i++ ) {
        if ( gpuShadowBatches[i].VertexBufferCount >= ( vertexCount * 3 )
          && gpuShadowBatches[i].IndiceBufferCount >= indiceCount ) {
            GPUShadowBatchInfos& batch = gpuShadowBatches[i];
            
            // We don't need to swap anything if the free node is the first node of the list
            const bool skipSwap = ( gpuShadowFreeListLength == i || gpuShadowFreeListLength == 1 );
            
            if ( !skipSwap ) {
                // Move the free node at the end of the data array (to keep things continous).
                GPUShadowBatchInfos batchDst = gpuShadowBatches[gpuShadowMeshCount];         
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
        GPUShadowBatchInfos batchDst = gpuShadowBatches[gpuShadowMeshCount];         
        gpuShadowBatches[gpuShadowMeshCount] = gpuShadowBatches[freeListEnd];
        gpuShadowBatches[freeListEnd] = batchDst;      
    }
    
    // Allocate a new batch info.
    const i32 allocatedBatchIndex = gpuShadowMeshCount;
    GPUShadowBatchInfos& meshBatchInfos = gpuShadowBatches[gpuShadowMeshCount++];
    meshBatchInfos.VertexBufferOffset = vertexBufferUsageOfffset;
    meshBatchInfos.VertexBufferCount = vertexCount * 3;
    meshBatchInfos.IndiceBufferOffset = indiceBufferUsageOfffset;
    meshBatchInfos.IndiceBufferCount = indiceCount;

	allocatedBatch = meshBatchInfos;

    vertexBufferUsageOfffset += meshBatchInfos.VertexBufferCount;
    indiceBufferUsageOfffset += meshBatchInfos.IndiceBufferCount;
    
    return allocatedBatchIndex;
}
