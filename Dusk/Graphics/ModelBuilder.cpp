/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "ModelBuilder.h"

#include <Rendering/RenderDevice.h>

#include "Graphics/Model.h"
#include <Graphics/Mesh.h>

#include <vector>

// LOD Max distance lookup table.
// TODO Might need to be assigned per model type (e.g. you don't necessary need the same detail granularity for all
// model type).
static constexpr f32    LOD_DISTANCE_LUT[Model::MAX_LOD_COUNT] =
{
    500.0f,
    2500.0f,
    5000.0f,
    10000.0f
};

template<typename T = f32, eResourceBind bindType = RESOURCE_BIND_VERTEX_BUFFER>
static Buffer* CreateIfNonEmpty( RenderDevice* renderDevice, const std::vector<T>& dataArray, const u32 strideInBytes )
{
    if ( dataArray.empty() ) {
        return nullptr;
    }

    constexpr u32 attributeSize = static_cast< u32 >( sizeof( T ) );

    BufferDesc posBufferDesc;
    posBufferDesc.BindFlags = bindType;
    posBufferDesc.Usage = RESOURCE_USAGE_STATIC;
    posBufferDesc.SizeInBytes = static_cast< u32 >( dataArray.size() * attributeSize );
    posBufferDesc.StrideInBytes = static_cast< u32 >( strideInBytes * attributeSize );

    return renderDevice->createBuffer( posBufferDesc, dataArray.data() );
}

void dk::graphics::BuildParsedModel( Model* builtModel, BaseAllocator* memoryAllocator, RenderDevice* renderDevice, ParsedModel& parsedModel )
{
    u32 lodCount = 0;

    std::string lodGroupName[Model::MAX_LOD_COUNT];
    std::vector<ParsedMesh*> meshPerLod[Model::MAX_LOD_COUNT];

    // Do a first pass to retrieve the number of lod and mesh per lod level.
    for ( ParsedMesh& mesh : parsedModel.ModelMeshes ) {
        lodCount = Max( ( mesh.LodIndex + 1 ), lodCount );

        meshPerLod[mesh.LodIndex].push_back( &mesh );
        lodGroupName[mesh.LodIndex] = mesh.LodGroup;
    }

    // TODO Remove crappy memory allocation scheme.
    std::vector<f32> position;
    std::vector<f32> texCoords;
    std::vector<f32> normals;
    std::vector<f32> colors;
    std::vector<i32> indices;

    std::vector<Mesh*> builtMeshes;

    // Build model LODs.
    for ( u32 lodIdx = 0; lodIdx < lodCount; lodIdx++ ) {
        Model::LevelOfDetail& lod = builtModel->getLevelOfDetailForEditor( lodIdx );
        lod.EndDistance = LOD_DISTANCE_LUT[lodIdx];
        lod.LodIndex = lodIdx;
        lod.GroupName = lodGroupName[lodIdx];
        lod.MeshCount = static_cast<i32>( meshPerLod[lodIdx].size() );
        lod.MeshArray = dk::core::allocateArray<Mesh>( memoryAllocator, lod.MeshCount );
        
        dk::maths::CreateAABBFromMinMaxPoints( lod.GroupAABB, dkVec3f::Max, -dkVec3f::Max );
        
        for ( i32 i = 0; i < lod.MeshCount; i++ ) {
            ParsedMesh& mesh = *( meshPerLod[lodIdx][i] );

            // Expand LOD group AABB.
            dk::maths::ExpandAABB( lod.GroupAABB, mesh.MeshAABB );

            // Keep track of all the meshes being built (to linearly assign the vertex buffer later).
            builtMeshes.push_back( &lod.MeshArray[i] );

            // Build Mesh instance / copy vertices to the shared array.
            Mesh& builtMesh = lod.MeshArray[i];
            builtMesh.Name = mesh.Name;
            builtMesh.RenderMaterial = nullptr;
			builtMesh.MemoryAllocator = memoryAllocator;
			builtMesh.VertexCount = mesh.VertexCount;
            builtMesh.FaceCount = static_cast<u32>( mesh.TriangleCount );

            if ( mesh.Flags.HasPositionAttribute ) {
                builtMesh.VertexAttributeBufferOffset = static_cast< i32 >( position.size() );

                for ( i32 j = 0; j < mesh.VertexCount; j++ ) {
                    position.push_back( mesh.PositionVertices[j][0] );
                    position.push_back( mesh.PositionVertices[j][1] );
                    position.push_back( mesh.PositionVertices[j][2] );
                }

                builtMesh.PositionVertices = dk::core::allocateArray<f32>( memoryAllocator, mesh.VertexCount * 3 );
                memcpy( builtMesh.PositionVertices, mesh.PositionVertices, sizeof( f32 ) * mesh.VertexCount * 3 );
            }

            if ( mesh.Flags.HasNormals ) {
                for ( i32 j = 0; j < mesh.VertexCount; j++ ) {
                    normals.push_back( mesh.NormalVertices[j][0] );
                    normals.push_back( mesh.NormalVertices[j][1] );
                    normals.push_back( mesh.NormalVertices[j][2] );
                }
            }

            if ( mesh.Flags.HasUvmap0 ) {
                for ( i32 j = 0; j < mesh.VertexCount; j++ ) {
                    texCoords.push_back( mesh.TexCoordsVertices[j][0] );
                    texCoords.push_back( mesh.TexCoordsVertices[j][1] );
                }
            }

            if ( mesh.Flags.HasVertexColor0 ) {
                for ( i32 j = 0; j < mesh.VertexCount; j++ ) {
                    position.push_back( mesh.ColorVertices[j][0] );
                    position.push_back( mesh.ColorVertices[j][1] );
                    position.push_back( mesh.ColorVertices[j][2] );
                }
            }

            if ( mesh.Flags.IsIndexed ) {
                builtMesh.IndiceBufferOffset = static_cast< i32 >( indices.size() );
                builtMesh.IndiceCount = mesh.IndexCount;

                for ( i32 j = 0; j < mesh.IndexCount; j++ ) {
                    indices.push_back( mesh.IndexList[j] );
                }

				builtMesh.Indices = dk::core::allocateArray<u32>( memoryAllocator, mesh.IndexCount );
				memcpy( builtMesh.Indices, mesh.IndexList, sizeof( u32 ) * mesh.IndexCount );
            }
        }
    }
    
    // Create buffers.
    Buffer* pos = CreateIfNonEmpty( renderDevice, position, 3 );
    Buffer* uv0 = CreateIfNonEmpty( renderDevice, texCoords, 2 );
    Buffer* normal = CreateIfNonEmpty( renderDevice, normals, 3 );
    Buffer* colors0 = CreateIfNonEmpty( renderDevice, colors, 3 );
    Buffer* indice = CreateIfNonEmpty<i32, eResourceBind::RESOURCE_BIND_INDICE_BUFFER>( renderDevice, indices, 1 );

    for ( Mesh* builtMesh : builtMeshes ) {
        builtMesh->AttributeBuffers[eMeshAttribute::Position] = BufferBinding();
        builtMesh->AttributeBuffers[eMeshAttribute::UvMap_0] = BufferBinding( uv0 );
        builtMesh->AttributeBuffers[eMeshAttribute::Normal] = BufferBinding( normal );
        builtMesh->AttributeBuffers[eMeshAttribute::Color_0] = BufferBinding( colors0 );

        builtMesh->IndexBuffer = BufferBinding( indice );
    }

    // Recompute AABB/Bounding Sphere.
	builtModel->computeBounds();
	builtModel->setName( parsedModel.ModelName.c_str() );
	builtModel->setResourcePath( parsedModel.ModelPath );
}
