/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "Transform.h"

#include <FileSystem/FileSystemObject.h>

#include "MatrixTransformations.h"

using namespace dk::maths;

Transform::Transform( const dkVec3f& worldTranslation, const dkVec3f& worldScale, const dkQuatf& worldRotation )
    : isDirty( true )
    , worldTranslation( worldTranslation )
    , worldScale( worldScale )
    , worldRotation( worldRotation )
    , localTranslation( worldTranslation )
    , localScale( worldScale )
    , localRotation( worldRotation )
    , localModelMatrix( dkMat4x4f::Identity )
    , worldModelMatrix( dkMat4x4f::Identity )
{
    rebuildModelMatrix();
}

bool Transform::rebuildModelMatrix()
{
    bool hasChanged = isDirty;

    // Rebuild model matrix if anything has changed
    if ( isDirty ) {
        dkMat4x4f translationMatrix = MakeTranslationMat( localTranslation );
        dkMat4x4f rotationMatrix = localRotation.toMat4x4();
        dkMat4x4f scaleMatrix = MakeScaleMat( localScale );

        localModelMatrix = translationMatrix * rotationMatrix * scaleMatrix;

        propagateParentModelMatrix( dkMat4x4f::Identity );

        isDirty = false;
    }

    return hasChanged;
}

void Transform::serialize( FileSystemObject* stream )
{
    stream->write( (uint8_t*)&localModelMatrix[0][0], sizeof( dkMat4x4f ) );
}

void Transform::deserialize( FileSystemObject* stream )
{
    stream->read( ( uint8_t* )&localModelMatrix[0][0], sizeof( dkMat4x4f ) );

    localTranslation = ExtractTranslation( localModelMatrix );
    localScale = ExtractScale( localModelMatrix );
    localRotation = dkQuatf( ExtractRotation( localModelMatrix, localScale ) );

    isDirty = true;

    rebuildModelMatrix();
}

void Transform::setLocalTranslation( const dkVec3f& newTranslation )
{
    localTranslation = newTranslation;
    isDirty = true;
}

void Transform::setLocalRotation( const dkQuatf& newRotation )
{
    localRotation = newRotation;
    isDirty = true;
}

void Transform::setLocalScale( const dkVec3f& newScale )
{
    localScale = newScale;
    isDirty = true;
}

void Transform::setLocalModelMatrix( const dkMat4x4f& modelMat )
{
    localModelMatrix = modelMat;

    localTranslation = ExtractTranslation( localModelMatrix );
    localScale = ExtractScale( localModelMatrix );
    localRotation = dkQuatf( ExtractRotation( localModelMatrix, localScale ) );
}

void Transform::setWorldTranslation( const dkVec3f& newTranslation )
{
    worldTranslation = newTranslation;
    isDirty = true;
}

void Transform::setWorldRotation( const dkQuatf& newRotation )
{
    worldRotation = newRotation;
    isDirty = true;
}

void Transform::setWorldScale( const dkVec3f& newScale )
{
    worldScale = newScale;
    isDirty = true;
}

void Transform::setWorldModelMatrix( const dkMat4x4f& modelMat )
{
    worldModelMatrix = modelMat;
    
    worldTranslation = ExtractTranslation( worldModelMatrix );
    worldScale = ExtractScale( worldModelMatrix );
    worldRotation = dkQuatf( ExtractRotation( worldModelMatrix, worldScale ) );
}

void Transform::translate( const dkVec3f& translation )
{
    localTranslation += translation;
    isDirty = true;
}

void Transform::propagateParentModelMatrix( const dkMat4x4f& parentModelMatrix )
{
    worldModelMatrix = ( localModelMatrix * parentModelMatrix ).transpose();

    worldTranslation = ExtractTranslation( worldModelMatrix );
    worldScale = ExtractScale( worldModelMatrix );
    worldRotation = dkQuatf( ExtractRotation( worldModelMatrix, worldScale ) );
}

dkMat4x4f* Transform::getWorldModelMatrix()
{
    return &worldModelMatrix;
}

const dkMat4x4f& Transform::getWorldModelMatrix() const
{
    return worldModelMatrix;
}

const dkVec3f& Transform::getWorldScale() const
{
    return worldScale;
}

const dkVec3f& Transform::getWorldTranslation() const
{
    return worldTranslation;
}

const dkQuatf& Transform::getWorldRotation() const
{
    return worldRotation;
}

f32 Transform::getWorldBiggestScale() const
{
    f32 biggestScale = worldScale.x;

    if ( biggestScale < worldScale.y ) {
        biggestScale = worldScale.y;
    }

    if ( biggestScale < worldScale.z ) {
        biggestScale = worldScale.z;
    }

    return biggestScale;
}

dkMat4x4f* Transform::getLocalModelMatrix()
{
    return &localModelMatrix;
}

const dkMat4x4f& Transform::getLocalModelMatrix() const
{
    return localModelMatrix;
}

const dkVec3f& Transform::getLocalScale() const
{
    return localScale;
}

const dkVec3f& Transform::getLocalTranslation() const
{
    return localTranslation;
}

const dkQuatf& Transform::getLocalRotation() const
{
    return localRotation;
}

f32 Transform::getLocalBiggestScale() const
{
    f32 biggestScale = localScale.x;

    if ( biggestScale < localScale.y ) {
        biggestScale = localScale.y;
    }

    if ( biggestScale < localScale.z ) {
        biggestScale = localScale.z;
    }

    return biggestScale;
}

bool Transform::needRebuild() const
{
    return isDirty;
}
