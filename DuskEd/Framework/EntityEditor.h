/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class GraphicsAssetCache;
class VirtualFileSystem;
class World;
struct Entity;
struct CameraData;
class FbxParser;
class RenderWorld;
class RenderDevice;

#include <Maths/Vector.h>

class EntityEditor
{
public:
                        EntityEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs, RenderWorld* rWorld, RenderDevice* activeRenderDevice );
                        ~EntityEditor();

#if DUSK_USE_IMGUI
    // Display EntityEditor panel (as a ImGui window).
    void                displayEditorWindow( CameraData& viewportCamera, const dkVec4f& viewportBounds );
#endif

    // Open the Entity Editor window.
    void                openEditorWindow();

    // Set the entity to edit.
    void                setActiveEntity( Entity* entity );
    
    // Set the world being edited.
    void                setActiveWorld( World* world );

    bool                isManipulatingTransform() const;

private:
    // True if the editor window is opened in the editor workspace.
    bool                isOpened;

    // Proxy to the entity being edited. Please note that this is a PROXY 
    // holding the handle to the active entity being edited (which means
    // the handle shouldn't be deleted/released unless the app is closing).
    Entity*             activeEntity;

    // Instance of the world being edited.
    World*              activeWorld;

    // Memory allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // Pointer to the active instance of the Graphics Assets cache.
    GraphicsAssetCache* graphicsAssetCache;

    // Pointer to the active geometry cache.
    RenderWorld*        renderWorld;

    // Pointer to the active instance of the Virtual Filesystem.
    VirtualFileSystem*  virtualFileSystem;

    // Pointer to the active RenderDevice.
    RenderDevice*       renderDevice;

#if DUSK_USE_FBXSDK
    // Parser to parse fbx geometry file. 
    FbxParser*          fbxParser;
#endif

private:
#if DUSK_USE_IMGUI
    void displayEntityInHiearchy( const dkStringHash_t entityHashcode, const Entity& entity );

    // Display Transform edition section (if the activeEntity has a Transform component and the context is valid).
    void displayTransformSection( const dkVec4f& viewportBounds, CameraData& viewportCamera );

    // Display Static geometry edition section.
    void displayStaticGeometrySection();
#endif
};
