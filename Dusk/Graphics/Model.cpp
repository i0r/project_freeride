/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Model.h"

Model::Model( BaseAllocator* allocator, const dkChar_t* name )
    : memoryAllocator( allocator )
    , lodCount( 0 )
    , lod{}
    , modelAABB{}
    , modelBoundingSphere{}
    , resourceFilePath( "" )
{
    setName( name );
}

Model::~Model()
{
    memoryAllocator = nullptr;
    modelName.clear();
}

i32 Model::getLevelOfDetailCount() const
{
    return lodCount;
}

const Model::LevelOfDetail& Model::getLevelOfDetail( const f32 distanceToMesh ) const
{
    // Cheapest lod available (chose by default if the model is too far from the camera).
    i32 cheapestValidLodIdx = 0;

    for ( u32 lodIdx = 0; lodIdx < lodCount; lodIdx++ ) {
        if ( lod[lodIdx].EndDistance >= distanceToMesh ) {
            return lod[lodIdx];
        }

        cheapestValidLodIdx = lodIdx;
    }

    return lod[cheapestValidLodIdx];
}

const Model::LevelOfDetail& Model::getLevelOfDetailByIndex( const u32 lodIdx ) const
{
    DUSK_DEV_ASSERT( lodIdx < lodCount, "lodIdx is out of bounds!" );

    return lod[lodIdx];
}

const bool Model::hasLevelOfDetail( const u32 lodIdx ) const
{
    return ( lodIdx < lodCount
             && lodIdx > 0
             && lodIdx < MAX_LOD_COUNT );
}

#if DUSK_DEVBUILD
Model::LevelOfDetail& Model::getLevelOfDetailForEditor( const u32 lodIdx )
{
    if ( lodIdx <= lodCount ) {
        lodCount = ( lodIdx + 1 );
    }

    return lod[lodIdx];
}

void Model::setResourcePath( const std::string& path )
{
    resourceFilePath = path;
}

const std::string& Model::getResourcedPath() const
{
    return resourceFilePath;
}
#endif

Model::LevelOfDetail& Model::addLevelOfDetail( const f32 lodEndDistance )
{
    i32 lodIdx = lodCount;
    if ( lodIdx >= MAX_LOD_COUNT ) {
        DUSK_LOG_ERROR( "Too many LOD for this mesh! Please raise MAX_LOD_COUNT if you wanna add more LOD." );
        return lod[MAX_LOD_COUNT - 1];
    }

    Model::LevelOfDetail& allocatedLevel = lod[lodIdx];
    allocatedLevel.EndDistance = lodEndDistance;
    allocatedLevel.LodIndex = lodIdx;

    lodCount++;

    rebuildLodHashcodes();

    return allocatedLevel;
}

void Model::setName( const dkString_t& newName )
{
    modelName = newName;
    modelHashcode = dk::core::CRC32( modelName );

    rebuildLodHashcodes();
}

const dkChar_t* Model::getName() const
{
    return modelName.empty() ? DUSK_STRING( "Model" ) : modelName.c_str();
}

void Model::computeBounds()
{
    dk::maths::CreateAABBFromMinMaxPoints( modelAABB, dkVec3f::Max, -dkVec3f::Max );

    for ( u32 lodIdx = 0; lodIdx < lodCount; lodIdx++ ) {
        dk::maths::ExpandAABB( modelAABB, lod[lodIdx].GroupAABB );
    }

    dkVec3f sphereCenterWorldSpace = dk::maths::GetAABBCentroid( modelAABB );
    f32 sphereRadius = dk::maths::GetBiggestScalar( dk::maths::GetAABBHalfExtents( modelAABB ) );

    dk::maths::CreateSphere( modelBoundingSphere, sphereCenterWorldSpace, sphereRadius );
}

const BoundingSphere& Model::getBoundingSphere() const
{
    return modelBoundingSphere;
}

dkStringHash_t Model::getHashcode() const
{
    return modelHashcode;
}

void Model::rebuildLodHashcodes()
{
	for ( u32 lodIdx = 0; lodIdx < lodCount; lodIdx++ ) {
		Model::LevelOfDetail& levelOfDetail = lod[lodIdx];

		dkString_t lodName = getName();
		lodName.append( DUSK_TO_STRING( lodIdx ) );
		levelOfDetail.Hashcode = dk::core::CRC32( lodName );
	}
}
