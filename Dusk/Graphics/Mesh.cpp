/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Mesh.h"

Mesh::Mesh()
    : RenderMaterial( nullptr )
    , IndexBuffer( nullptr )
    , IndiceBufferOffset( 0 )
    , VertexAttributeBufferOffset( 0 )
    , IndiceCount( 0 )
    , VertexCount( 0 )
    , PositionVertices( nullptr )
    , MemoryAllocator( nullptr )
    , Indices( nullptr )
    , ShadowGPUBatchEntryIndex( 0u )
{
    memset( AttributeBuffers, 0, sizeof( Buffer* ) * ToUnderlyingType( eMeshAttribute::Count ) );

#if DUSK_DEVBUILD
    Name = "Mesh_No_Name";
#endif
}

Mesh::~Mesh()
{
#if DUSK_DEVBUILD
    Name.clear();
#endif

    if ( PositionVertices != nullptr ) {
        dk::core::freeArray( MemoryAllocator, PositionVertices );
    }

	if ( Indices != nullptr ) {
		dk::core::freeArray( MemoryAllocator, Indices );
    }
}
