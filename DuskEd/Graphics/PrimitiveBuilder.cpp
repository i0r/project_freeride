/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "PrimitiveBuilder.h"

#include <Rendering/RenderDevice.h>

#include <vector>

static DUSK_INLINE u32 HashVertex( const dkVec3f& vertex )
{
    f32 vertexScalars[3];
    for ( i32 i = 0; i < 3; i++ ) {
        vertexScalars[i] = vertex[i];
    }

    return dk::core::CRC32( reinterpret_cast<u8*>( vertexScalars ), sizeof( f32 ) * 3 );
}

BuiltPrimitive dk::graphics::BuildPrimitive( RenderDevice* renderDevice, const ParsedMesh& parsedMesh )
{
    // TODO Remove crappy memory allocation scheme.
    std::vector<f32> position;
    std::vector<f32> texCoords;
    std::vector<f32> normals;
    
    std::vector<u32> indices;

    // Store unique vertices to remove duplicates (required for certain geometry format like FBX).
    std::unordered_map<u32, i32> uniqueVertexArray;

    for ( i32 j = 0; j < parsedMesh.VertexCount; j++ ) {
        position.push_back( parsedMesh.PositionVertices[j][0] );
        position.push_back( parsedMesh.PositionVertices[j][1] );
        position.push_back( parsedMesh.PositionVertices[j][2] );

        normals.push_back( parsedMesh.NormalVertices[j][0] );
        normals.push_back( parsedMesh.NormalVertices[j][1] );
        normals.push_back( parsedMesh.NormalVertices[j][2] );

        texCoords.push_back( parsedMesh.TexCoordsVertices[j][0] );
        texCoords.push_back( parsedMesh.TexCoordsVertices[j][1] );
    }

    BufferDesc posBufferDesc;
    posBufferDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    posBufferDesc.Usage = RESOURCE_USAGE_STATIC;
    posBufferDesc.SizeInBytes = static_cast<u32>( position.size() * sizeof( f32 ) );
    posBufferDesc.StrideInBytes = static_cast< u32 >( 3 * sizeof( f32 ) );

    Buffer* pos = renderDevice->createBuffer( posBufferDesc, position.data() );

    BufferDesc uvBufferDesc;
    uvBufferDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    uvBufferDesc.Usage = RESOURCE_USAGE_STATIC;
    uvBufferDesc.SizeInBytes = static_cast< u32 >( texCoords.size() * sizeof( f32 ) );
    uvBufferDesc.StrideInBytes = static_cast< u32 >( 2 * sizeof( f32 ) );

    Buffer* uv = renderDevice->createBuffer( uvBufferDesc, texCoords.data() );

    BufferDesc normalBufferDesc;
    normalBufferDesc.BindFlags = RESOURCE_BIND_VERTEX_BUFFER;
    normalBufferDesc.Usage = RESOURCE_USAGE_STATIC;
    normalBufferDesc.SizeInBytes = static_cast< u32 >( normals.size() * sizeof( f32 ) );
    normalBufferDesc.StrideInBytes = static_cast< u32 >( 3 * sizeof( f32 ) );

    Buffer* normal = renderDevice->createBuffer( normalBufferDesc, normals.data() );

    BufferDesc indiceBufferDesc;
    indiceBufferDesc.BindFlags = RESOURCE_BIND_INDICE_BUFFER;
    indiceBufferDesc.Usage = RESOURCE_USAGE_STATIC;
    indiceBufferDesc.SizeInBytes = static_cast< u32 >( parsedMesh.IndexCount * sizeof( i32 ) );
    indiceBufferDesc.StrideInBytes = sizeof( i32 );

    Buffer* indice = renderDevice->createBuffer( indiceBufferDesc, parsedMesh.IndexList );

    BuiltPrimitive builtPrimitive;
    builtPrimitive.VertexAttributeBuffer[0] = pos;
    builtPrimitive.VertexAttributeBuffer[1] = normal;
    builtPrimitive.VertexAttributeBuffer[2] = uv;

    builtPrimitive.VertexCount = static_cast< u32 >( parsedMesh.VertexCount );
    builtPrimitive.IndiceCount = static_cast<u32>( parsedMesh.IndexCount );
    builtPrimitive.IndiceBuffer = indice;

    return builtPrimitive;
}
