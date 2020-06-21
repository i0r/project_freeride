/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "PrimitiveCache.h"

#include "Maths/Primitives.h"

#include "Rendering/RenderDevice.h"

PrimitiveCache::PrimitiveCache()
    : indiceBuffer( nullptr )
{
    memset( vertexAttributesBuffer, 0, sizeof( Buffer* ) * 3 );
    memset( &sphereEntry, 0, sizeof( Entry ) );
}

PrimitiveCache::~PrimitiveCache()
{

}

void PrimitiveCache::destroy( RenderDevice* renderDevice )
{
    for ( i32 i = 0; i < 3; i++ ) {
        renderDevice->destroyBuffer( vertexAttributesBuffer[i] );
    }

    renderDevice->destroyBuffer( indiceBuffer );
}

void PrimitiveCache::createCachedGeometry( RenderDevice* renderDevice )
{
    u32 vertexCount = 0;
    u32 indiceCount = 0;

    // Sphere Primitive
    PrimitiveData spherePrimitive = dk::maths::CreateSpherePrimitive();

    sphereEntry.vertexBufferOffset = vertexCount;
    vertexCount += static_cast<u32>( spherePrimitive.position.size() / 3 );

    sphereEntry.indiceBufferOffset = indiceCount;
    sphereEntry.indiceCount = static_cast< u32 >( spherePrimitive.indices.size() );
    indiceCount += static_cast< u32 >( spherePrimitive.indices.size() );

    // Build cached data buffers   
    std::vector<f32> positionBuffer( vertexCount * 3 );
    std::vector<f32> normalBuffer( vertexCount * 3 );
    std::vector<f32> uvBuffer( vertexCount * 3 );
    std::vector<u16> indiceBufferData( indiceCount );

    memcpy( &positionBuffer[sphereEntry.vertexBufferOffset], spherePrimitive.position.data(), spherePrimitive.position.size() * sizeof( f32 ) );
    memcpy( &normalBuffer[sphereEntry.vertexBufferOffset], spherePrimitive.normal.data(), spherePrimitive.normal.size() * sizeof( f32 ) );
    memcpy( &uvBuffer[sphereEntry.vertexBufferOffset], spherePrimitive.uv.data(), spherePrimitive.uv.size() * sizeof( f32 ) );
    memcpy( &indiceBufferData[sphereEntry.indiceBufferOffset], spherePrimitive.indices.data(), spherePrimitive.indices.size() * sizeof( u16 ) );

    // Allocate buffers on GPU
    BufferDesc vboDesc;
    vboDesc.BindFlags = eResourceBind::RESOURCE_BIND_VERTEX_BUFFER;
    vboDesc.Usage = RESOURCE_USAGE_STATIC;

    vboDesc.SizeInBytes = static_cast< u32 >( vertexCount * 3 * sizeof( f32 ) );
    vboDesc.StrideInBytes = 3 * sizeof( f32 );
    vertexAttributesBuffer[0] = renderDevice->createBuffer( vboDesc, positionBuffer.data() );

    vboDesc.SizeInBytes = static_cast< u32 >( vertexCount * 2 * sizeof( f32 ) );
    vboDesc.StrideInBytes = 2 * sizeof( f32 );
    vertexAttributesBuffer[1] = renderDevice->createBuffer( vboDesc, uvBuffer.data() );

    vboDesc.SizeInBytes = static_cast< u32 >( vertexCount * 3 * sizeof( f32 ) );
    vboDesc.StrideInBytes = 3 * sizeof( f32 );
    vertexAttributesBuffer[2] = renderDevice->createBuffer( vboDesc, normalBuffer.data() );

    BufferDesc iboDesc;
    iboDesc.SizeInBytes = static_cast< u32 >( indiceBufferData.size() * sizeof( u16 ) );
    iboDesc.BindFlags = eResourceBind::RESOURCE_BIND_INDICE_BUFFER;
    iboDesc.Usage = RESOURCE_USAGE_STATIC;

    indiceBuffer = renderDevice->createBuffer( iboDesc, indiceBufferData.data() );

    // Assign buffers pointers once the buffers have been created
    for ( i32 i = 0; i < 3; i++ ) {
        sphereEntry.vertexBuffers[i] = vertexAttributesBuffer[i];
    }

    sphereEntry.indiceBuffer = indiceBuffer;
}
