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

class EntityEditor
{
public:
                        EntityEditor( BaseAllocator* allocator, GraphicsAssetCache* gfxCache, VirtualFileSystem* vfs );
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

private:
    // True if the editor window is opened in the editor workspace.
    bool                isOpened;

    // Pointer to the entity being edited.
    Entity*             activeEntity;

    // Instance of the world being edited.
    World*              activeWorld;

    // Memory allocator owning this instance.
    BaseAllocator*      memoryAllocator;

    // Pointer to the active instance of the Graphics Assets cache.
    GraphicsAssetCache* graphicsAssetCache;

    // Pointer to the active instance of the Virtual Filesystem.
    VirtualFileSystem*  virtualFileSystem;

private:
#if DUSK_USE_IMGUI
    // Display Transform edition section (if the activeEntity has a Transform component and the context is valid).
    void displayTransformSection( const dkVec4f& viewportBounds, CameraData& viewportCamera );
#endif
};
