/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/Polygon.h>
#include <Maths/AABB.h>

// TODO It should be a class (we shouldnt expose the memory allocators nor the flags...).

// Hold data parsed from a regular file format (.fbx, .obj, etc.).
struct ParsedMesh
{
    // Allocator owning this mesh (own memory for the attribute array too).
    BaseAllocator*      MemoryAllocator;

    // Mesh name (assigned in Maya).
    std::string         Name;

    // Index count for this mesh.
    // If zero, the mesh is not indexed.
    i32                 IndexCount;

    // Vertex count for this mesh.
    i32                 VertexCount;

    // Triangle count for this mesh.
    i32                 TriangleCount;

    struct {
        // If true, PositionVertices is guarantee to be non null.
        u8              HasPositionAttribute : 1;

        // If true, NormalVertices is guarantee to be non null.
        u8              HasNormals : 1;

        // If true, TexCoordsVertices[0] is guarantee to be non null.
        u8              HasUvmap0 : 1;

        // If true, VertexColors is guarantee to be non null;
        u8              HasVertexColor0 : 1;

        // If true, IndexList is guarantee to be non null.
        u8              IsIndexed : 1;
    } Flags;

    // Array holding this mesh vertices position attributes.
    dkVec3f*            PositionVertices;

    // Array holding this mesh vertices normal attributes.
    dkVec3f*            NormalVertices;
    
    // Array holding this mesh vertices colors attributes.
    dkVec3f*            ColorVertices;

    // Array holding this mesh texture coordinates for uv map0.
    dkVec2f*            TexCoordsVertices;

    // Array holding this mesh indexes.
    i32*                IndexList;

    // Index of the lod of this mesh (0 if the geometry being parsed has no LOD).
    u32                 LodIndex;

    // Name of the lod group of this mesh (empty if the mesh does not belong to a lod group).
    std::string         LodGroup;

    // Mesh Bounding box. Only include vertices for this mesh.
    AABB                MeshAABB;

    ParsedMesh()
        : MemoryAllocator( nullptr )
        , Name( "" )
        , IndexCount( 0 )
        , VertexCount( 0 )
        , TriangleCount( 0 )
        , Flags{ 0, 0, 0, 0, 0 }
        , PositionVertices( nullptr )
        , NormalVertices( nullptr )
        , ColorVertices( nullptr )
        , TexCoordsVertices( nullptr )
        , IndexList( nullptr )
        , LodIndex( 0 )
        , LodGroup( "LOD0" )
        , MeshAABB{}
    {

    }
};

struct ParsedModel 
{
    // Meshes of the parsed mesh.
    // TODO In the future, it could use a linear allocator.
    // Right now; a crappy vector relying on the standard lib is not that great...
    std::vector<ParsedMesh> ModelMeshes;

    // Number of LOD level. If ModelMeshes is not empty, a model should at least have one LOD.
    u32 LodCount;

    // Name of the model (usually the filename of the asset).
    dkString_t ModelName;

    ParsedModel()
        : LodCount( 1 )
        , ModelName( DUSK_STRING( "ParsedModel" ) )
    {
    }
};
