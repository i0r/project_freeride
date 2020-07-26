/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class Model;
class DrawCommandBuilder;
class RenderDevice;
class PoolAllocator;
class GraphicsAssetCache;
class CommandList;

#include <Parsing/GeometryParser.h>

#include <Maths/Matrix.h>

// The RenderWorld is responsible for render entity management (allocation/streaming/update/etc.).
class RenderWorld 
{
public:
    static constexpr i32 MAX_MODEL_COUNT = 1024;

public:
    DUSK_INLINE MeshConstants*       getGpuShadowBatchesData() const { return gpuShadowBatches; }
    DUSK_INLINE u32                  getGpuShadowBatchCount() const { return gpuShadowMeshCount; }
    DUSK_INLINE Buffer*              getShadowVertexBuffer() const { return shadowCasterVertexBuffer; }
    DUSK_INLINE Buffer*              getShadowIndiceBuffer() const { return shadowCasterIndexBuffer; }
    DUSK_INLINE Buffer*             getMeshConstantsBuffer() const { return gpuMeshInfos; }
    // TEMPORARY CRAP; SHOULD BE REWORKED TO AVOID THE AWFUL MEMORY ALLOCATION
    DUSK_INLINE const std::vector<MeshCluster>*        getMeshClusters() const { return Clusters.data(); }

public:
                    RenderWorld( BaseAllocator* allocator );
                    RenderWorld( RenderWorld& ) = default;
                    ~RenderWorld();

    void            create( RenderDevice& renderDevice );
    
    void            destroy( RenderDevice& renderDevice );
    
    // Add a Model to the RenderWorld and immediately upload its data to the GPU.
    // This should be used in an editor context only.
    Model*          addAndCommitParsedDynamicModel( RenderDevice* renderDevice, ParsedModel& parsedModel, GraphicsAssetCache* graphicsAssetCache );

    void            update( RenderDevice* renderDevice );
    
private:
    // The memory allocator owning this instance.
    BaseAllocator*  memoryAllocator;

    // Allocator for WorldModel
    PoolAllocator*  modelAllocator;

    // List of active model in the world (allocated by modelAllocator).
    // Note that this is a resource list; not a list of instance in the world!
    Model*          modelList[MAX_MODEL_COUNT];

    // Allocated model count (resource; not instance).
    i32             modelCount;
    
    // Vertex buffer used to batch GPU-driven shadow draw calls.
    Buffer*         shadowCasterVertexBuffer;
    
    // Index buffer used to batch GPU-driven shadow draw calls.
    Buffer*         shadowCasterIndexBuffer;
    
    // Copy of the GPU shadow vertex buffer on the CPU (used for quick updating).
    f32*            vertexBufferData;
    
    // Copy of the GPU shadow indice buffer on the CPU (used for quick updating).
    u32*            indiceBufferData;
    
    // Start offset at which vertex data (in vertexBufferData) has been modified.
    u32             vertexBufferDirtyOffset;
    
    // Length of the modification made to the vertex data.
    u32             vertexBufferDirtyLength;
    
    // Start offset at which indice data (in indiceBufferData) has been modified.
    u32             indiceBufferDirtyOffset;
    
    // Length of the modification made to the indice data.
    u32             indiceBufferDirtyLength;
    
    // Current usage of the vertex buffer.
    u32             vertexBufferUsageOfffset;
    
    // Current usage of the indice buffer.
    u32             indiceBufferUsageOfffset;
    
    // Number of GPU-driven mesh allocated (in gpuShadowBatches).
    u32             gpuShadowMeshCount;
    
    // Number of free entry available in gpuShadowBatches. Free entries start at the end of the used entry list.
    u32             gpuShadowFreeListLength;
    
    // CPU copy of entries describing the data stored in the gpu shadow vertex buffer.
    MeshConstants* gpuShadowBatches;

    std::vector<std::vector<MeshCluster>> Clusters;

    // GPU instance of gpuShadowBatches.
    Buffer*         gpuMeshInfos;
    
    bool            isGpuMeshInfosDirty;

private:
	// Allocate or reuse a GPUShadowBatchInfos from gpuShadowBatches (allocation is done if there is no freenode/suitable node).
    i32                 allocateGpuMeshInfos( MeshConstants& allocatedBatch, const u32 vertexCount, const u32 faceCount, const u32 indiceCount );

    void                createMeshClusters( const i32 meshConstantsIdx, const u32 indexCount, const f32* vertices, const u32* indices );
};
