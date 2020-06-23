/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
struct Mesh;

#include <Maths/BoundingSphere.h>
#include <Maths/AABB.h>

class Model 
{
public:
    struct LevelOfDetail 
    {
        // Maximum distance (in world units) at which this LOD is visible (should be equal LOD[N+1].StartDistance).
        f32         EndDistance;

        // Level Of Detail index.
        u32         LodIndex;

        // Array of mesh used to draw this lod.
        Mesh*       MeshArray;

        // Length of MeshArray.
        i32         MeshCount;

#if DUSK_DEVBUILD
        // Name of this LOD (editor context only).
        std::string GroupName;
#endif

        // The hashcode of this LOD.
        dkStringHash_t  Hashcode;

        // The Bounding Box of this LOD.
        AABB        GroupAABB;

        LevelOfDetail()
            : EndDistance( std::numeric_limits<f32>::max() )
            , LodIndex( 0 )
            , MeshArray( nullptr )
            , MeshCount( -1 )
#if DUSK_DEVBUILD
            , GroupName( "LOD_Group" ) 
#endif
            , Hashcode( 0 )
            , GroupAABB{}
        {

        }
    };

public:
    // Maximum level of detail per model. Might need to be raised in the future.
    static constexpr i32    MAX_LOD_COUNT = 4;

    // LOD Max distance lookup table.
    // TODO Might need to be assigned per model type (e.g. you don't necessary need the same detail granularity for all
    // model type).
    static constexpr f32    LOD_DISTANCE_LUT[MAX_LOD_COUNT] =
    {
        500.0f,
        2500.0f,
        5000.0f,
        10000.0f
    };

public:
                            Model( BaseAllocator* allocator, const dkChar_t* name = DUSK_STRING( "DefaultModel" ) );
                            ~Model();

    // Return the lod count available for this model. If the model has no lod, it will return one (the original geometry
    // is treated as LOD0).
    i32                     getLevelOfDetailCount() const;

    // Return the lod to display accordingly to the distance between the camera and the model.
    const LevelOfDetail&    getLevelOfDetail( const f32 distanceToMesh ) const;

    // Return the lod by its index. The index must be between 0 and ( getLevelOfDetailCount() - 1 ).
    const LevelOfDetail&    getLevelOfDetailByIndex( const u32 lodIdx ) const;

    // Return true if a lod at the specified lodIdx exist. False otherwise.
    const bool              hasLevelOfDetail( const u32 lodIdx ) const;

#if DUSK_DEVBUILD
    // Return a non-const reference to the lod at the specified lodIdx (if already existing).
    // This function should only be used in an editor context.
    LevelOfDetail&          getLevelOfDetailForEditor( const u32 lodIdx );

    // Set the path to the file storing this model data (editor only).
    void                    setResourcePath( const std::string& path );

    // Return the path to the file storing this model data (editor only).
    const std::string&      getResourcedPath() const;
#endif

    // Add a lod to this model (with an explicit max. visibility distance). Return a reference to the lod added, or a 
    // reference to the cheapest lod level if the model has reached the maximum lod count.
    LevelOfDetail&          addLevelOfDetail( const f32 lodEndDistance );

    // Set the name of this model.
    void                    setName( const dkString_t& newName );

    // Return the name of this model.
    const dkChar_t*         getName() const;

    // Compute this model bounding primitive bounds (using each LOD mesh bounds).
    void                    computeBounds();

    // Return the bounding sphere of this model. Might return invalid results if 
    // the model has not been properly created (e.g. if you forgot to call 
    // computeBounds).
    const BoundingSphere&   getBoundingSphere() const;

    // Return the hashcode of this model (precomputed hashcode based on this model name).
    dkStringHash_t          getHashcode() const;

private:
    // Allocator owning this object.
    BaseAllocator*          memoryAllocator;

    // Name of the model (either set from the asset data or by hand).
    dkString_t              modelName;

    // Hashcode of this model.
    dkStringHash_t          modelHashcode;

    // Number of lod available for this model.
    u32                     lodCount;

    // Lods for this model.
    LevelOfDetail           lod[MAX_LOD_COUNT];

    // Bounding Box for this model.
    AABB                    modelAABB;

    // Bounding Sphere for this model.
    BoundingSphere          modelBoundingSphere;

    // Absolute path to the file storing this model.
    std::string             resourceFilePath;

private:
    void rebuildLodHashcodes();
};
