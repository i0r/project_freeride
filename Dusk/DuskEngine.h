/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class LinearAllocator;
class VirtualFileSystem;

// Monolithic class which implements most of the feature available in the Dusk Engine.
// This class must be used to implement Dusk in an application.
class DuskEngine
{
public:
    // Descriptor used to create and initialize a DuskEngine instance.
    struct Parameters
    {
        // The name of the application creating this engine instance.
        const dkChar_t*     ApplicationName;

        // If true rendering/display subsystems won't be initialized (unless requested explicitly).
        bool                IsHeadless;

        // Size of the memory allocated for the global memory table (in bytes).
        size_t              GlobalMemoryTableSize;
    };

public:
                DuskEngine();
                DuskEngine( DuskEngine& ) = delete;
                ~DuskEngine();
            
    // Create an instance of the Dusk Game Engine. 'creationParameters' defines the parameters of the engine
    // (name of the application, services enabled, etc.). 'cmdLineArgs' is an optional character array holding
    // the command line arguments provided by the user.
    void        create( Parameters creationParameters, const char* cmdLineArgs = nullptr );

    // Shutdown this engine instance (destroy and deallocate everything).
    void        shutdown();

    // Set the name of the application owning this engine instance.
    void        setApplicationName( const dkChar_t* name = DUSK_STRING( "Dusk GameEngine" ) );

    // Execute the main loop. This function will only return once the engine receives a shutdown/close signal from its environment
    // (e.g. window close; user close request; etc...).
    void        mainLoop();

private:
    // The name of the application which created the instance of the engine.
    // Should be set using either setApplicationName or Parameters::ApplicationName.
    dkString_t          applicationName;

    // Memory chunk preallocated at initialization time. It'll probably require some changes in the future.
    void*               allocatedTable;

    // Global allocator used for subsystem allocation (this is used to split the allocated memory table for each subsystem).
    LinearAllocator*    globalAllocator;

    // VirtualFileSystem instance (abstracts logical/physical FileSystem).
    VirtualFileSystem*  virtualFileSystem;

private:
    void        initializeIoSubsystems();
};

// Global pointer to the Dusk GameEngine instance. This pointer is automatically set once an instance
// of the class DuskEngine is created (otherwise expect this pointer to be nullptr!).
extern DuskEngine* g_DuskEngine;
