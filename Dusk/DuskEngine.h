/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class LinearAllocator;
class VirtualFileSystem;
class FileSystem;
class InputMapper;
class InputReader;
class RenderDocHelper;
class DisplaySurface;
class RenderDevice;
class ShaderCache;
class GraphicsAssetCache;
class WorldRenderer;
class HUDRenderer;
class RenderWorld;
class DrawCommandBuilder;
class World;
class DynamicsWorld;

#include "Core/Listener.h"

// Monolithic class which implements most of the feature available in the Dusk Engine.
// This class must be used to implement Dusk in an application.
class DuskEngine
{
public:
    DUSK_INLINE InputMapper* getInputMapper() { return inputMapper; }
    DUSK_INLINE InputReader* getInputReader() { return inputReader; }
    DUSK_INLINE LinearAllocator* getGlobalAllocator() { return globalAllocator; }
    DUSK_INLINE VirtualFileSystem* getVirtualFileSystem() { return virtualFileSystem; }
    DUSK_INLINE RenderDevice* getRenderDevice() { return renderDevice; }
    DUSK_INLINE GraphicsAssetCache* getGraphicsAssetCache() { return graphicsAssetCache; }
    DUSK_INLINE DisplaySurface* getMainDisplaySurface() { return mainDisplaySurface; }
    DUSK_INLINE RenderWorld* getRenderWorld() { return renderWorld; }
    DUSK_INLINE World* getLogicWorld() { return world; }

public:
    // Listener for main display resize events. Disabled if the application is headless.
    Listener<void( f32, f32 ), 32> OnResize;

public:
                DuskEngine();
                DuskEngine( DuskEngine& ) = delete;
                ~DuskEngine();
            
    // Create an instance of the Dusk Game Engine. 'cmdLineArgs' is an optional character array holding
    // the command line arguments provided by the user.
    void        create( const char* cmdLineArgs = nullptr );

    // Shutdown this engine instance (destroy and deallocate everything).
    void        shutdown();

    // Set the name of the application owning this engine instance.
    void        setApplicationName( const dkChar_t* name = DUSK_STRING( "Dusk GameEngine" ) );

    // Execute the main loop. This function will only return once the engine receives a shutdown/close signal from its environment
    // (e.g. window close; user close request; etc...).
    void        mainLoop();

    // Return the delta time for the running frame. Return 0 if the engine state is invalid
    // (e.g. if this function is called before the first frame).
    f32         getDeltaTime() const;

private:
    // The name of the application which created the instance of the engine.
    // Should be set using either setApplicationName or Parameters::ApplicationName.
    dkString_t          applicationName;

    // Running frame delta time.
    f32                 deltaTime;

    // Memory chunk preallocated at initialization time. It'll probably require some changes in the future.
    void*               allocatedTable;

    // Global allocator used for subsystem allocation (this is used to split the allocated memory table for each subsystem).
    LinearAllocator*    globalAllocator;

    // VirtualFileSystem instance (abstracts logical/physical FileSystem).
    VirtualFileSystem*  virtualFileSystem;

    // FileSystem used to store/load game data (baked assets or runtime cached binaries like pso cache).
    FileSystem*         dataFileSystem;

    // FileSystem used to load game data (READ ONLY).
    FileSystem*         gameFileSystem;

    // FileSystem used to load/store configuration/save data.
    FileSystem*         saveFileSystem;

#if DUSK_DEVBUILD
    // FileSystem used to load editor assets (e.g. geometry; uncompressed textures; plain text scripts; etc.).
    FileSystem*         edAssetsFileSystem;

    // FileSystem used to load shared content between host and GPU (e.g. shared shader headers).
    FileSystem*         rendererFileSystem;
#endif

    // Forward and map read input to an abstracted interface.
    InputMapper*        inputMapper;

    // Read input from system specific API (e.g. DirectInput, WinAPI, etc.).
    InputReader*        inputReader;

#if DUSK_USE_RENDERDOC
    // Helper to use RenderDoc API in an application. Note that you still need RenderDoc library on your system.
    RenderDocHelper*    renderDocHelper;
#endif

    // The main application display surface. Note that this pointer might be null! (e.g. headless application)
    DisplaySurface*     mainDisplaySurface;

    RenderDevice*       renderDevice;

    ShaderCache*        shaderCache;

    GraphicsAssetCache* graphicsAssetCache;

    WorldRenderer*      worldRenderer;

    HUDRenderer*        hudRenderer;

    RenderWorld*        renderWorld;

    DrawCommandBuilder* drawCommandBuilder;

    World*              world;

    DynamicsWorld*      dynamicsWorld;

private:
    // Initialize IO subsytems.
    void        initializeIoSubsystems();

    // Initialize input subsytems.
    void        initializeInputSubsystems();

    // Initialize render subsytems (rendering and graphics).
    void        initializeRenderSubsystems();

    // Initialize logic subsytems.
    void        initializeLogicSubsystems();
};

// Global pointer to the Dusk GameEngine instance. This pointer is automatically set once an instance
// of the class DuskEngine is created (otherwise expect this pointer to be nullptr!).
extern DuskEngine* g_DuskEngine;
