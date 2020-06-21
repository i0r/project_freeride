/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Matrix.h"
#include "Vector.h"
#include "Quaternion.h"

class FileSystemObject;

class Transform
{
#if DUSK_DEVBUILD
public:
    // Transformation Manipulation state for DuskEd Gizmo
    // TODO Move me please!
    bool                IsManipulating = false;
#endif

public:
                        Transform( const dkVec3f& worldTranslation = dkVec3f::Zero, const dkVec3f& worldScale = dkVec3f( 1.0f, 1.0f, 1.0f ), const dkQuatf& worldRotation = dkQuatf::Identity );
                        Transform( Transform& transform ) = default;
                        Transform& operator = ( Transform& transform ) = default;
                        ~Transform() = default;

    bool                rebuildModelMatrix();

    void                setLocalTranslation( const dkVec3f& newTranslation );
    void                setLocalRotation( const dkQuatf& newRotation );
    void                setLocalScale( const dkVec3f& newScale );

    void                setLocalModelMatrix( const dkMat4x4f& modelMat );

    void                setWorldTranslation( const dkVec3f& newTranslation );
    void                setWorldRotation( const dkQuatf& newRotation );
    void                setWorldScale( const dkVec3f& newScale );

    void                setWorldModelMatrix( const dkMat4x4f& modelMat );

    void                translate( const dkVec3f& translation );

    void                propagateParentModelMatrix( const dkMat4x4f& parentModelMatrix );

    dkMat4x4f*          getWorldModelMatrix();
    const dkMat4x4f&    getWorldModelMatrix() const;

    const dkVec3f&      getWorldScale() const;
    const dkVec3f&      getWorldTranslation() const;
    const dkQuatf&      getWorldRotation() const;
    f32                 getWorldBiggestScale() const;

    dkMat4x4f*          getLocalModelMatrix();
    const dkMat4x4f&    getLocalModelMatrix() const;

    const dkVec3f&      getLocalScale() const;
    const dkVec3f&      getLocalTranslation() const;
    const dkQuatf&      getLocalRotation() const;
    f32                 getLocalBiggestScale() const;

    void                serialize( FileSystemObject* stream );
    void                deserialize( FileSystemObject* stream );

    bool                needRebuild() const;

private:
    bool                isDirty;

    dkVec3f             worldTranslation;
    dkVec3f             worldScale;
    dkQuatf             worldRotation;

    dkVec3f             localTranslation;
    dkVec3f             localScale;
    dkQuatf             localRotation;

    dkMat4x4f           localModelMatrix;
    dkMat4x4f           worldModelMatrix;
};
