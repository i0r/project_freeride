/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct Buffer;
class Material;
class BaseAllocator;

// An enum containing possible attribute for the vertex of a given mesh.
enum eMeshAttribute : i32 
{
    Position = 0,

    UvMap_0,
    UvMap_1,
    UvMap_2,

    Normal,

    Color_0,
    Color_1,

    Bone_Weight,
    Bone_Indices,

    Count,
};

struct BufferBinding 
{
    // The buffer holding the data for this binding.
    const Buffer*   BufferObject;
    
    // Offset in bytes at which the data starts in BufferObject.
    u32             OffsetInBytes;
   
    // Custom stride to reinterpret the given BufferObject content.
    // If 0, this parameter is ignored.
    u32             StrideOverrideInBytes;

    BufferBinding( const Buffer* buffer = nullptr, const u32 offset = 0u, const u32 stride = 0u )
        : BufferObject( buffer )
        , OffsetInBytes( offset )
        , StrideOverrideInBytes( stride )
    {

    }
};

// A container holding informations to render geometry.
struct Mesh
{
    // Array holding buffers for each eMeshAttribute. The Mesh instance should not have
    // the ownership of those buffers.
    BufferBinding     AttributeBuffers[eMeshAttribute::Count];

    // Buffer holding indexes for this mesh (if the mesh is indexed).
    BufferBinding     IndexBuffer;

    // The material used to draw this mesh. The mesh instance should not have the ownership
    // of the material instance.
    const Material*         RenderMaterial;

    // Offset at which this lod indexes are stored.
    u32                     IndiceBufferOffset;

    // Offset at which this lod vertex attributes are stored. Usually unused unless the lod is stored in a shared buffer.
    u32                     VertexAttributeBufferOffset;

    // Number of indexes to draw (0 if the mesh is not indexed).
    u32                     IndiceCount;

    // Number of vertices to draw.
    u32                     VertexCount;

    // Number of face for this mesh. Required for geometry processing (e.g. mesh cluster calculation).
    u32                     FaceCount;

#if DUSK_DEVBUILD
    // Name of this mesh (for debug purpose; devbuild only).
    std::string             Name;
#endif

    // Vertex position ONLY. Used for shadow buffer update/rebuild (to avoid reparsing).
    f32*                    PositionVertices;

    // The memory allocator owning PositionVertices.
    BaseAllocator*          MemoryAllocator;

    // CPU Mesh Indices. Used for shadow buffer update/rebuild (to avoid reparsing).
    u32*                    Indices;

    // GPU Batch Entry index allocated by the RenderWorld (required during GPU-driven
    // culling).
    u32                     RenderWorldIndex;

                            Mesh();
                            ~Mesh();
};
