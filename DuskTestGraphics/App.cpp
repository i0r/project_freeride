/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "App.h"

#include "Core/Environment.h"
#include "Core/CommandLineArgs.h"
#include "Core/Timer.h"
#include "Core/FramerateCounter.h"

#include "Core/Allocators/AllocationHelpers.h"
#include "Core/Allocators/LinearAllocator.h"

#include "Core/Display/DisplaySurface.h"

#include "FileSystem/VirtualFileSystem.h"
#include "FileSystem/FileSystemNative.h"
#include "FileSystem/FileSystemArchive.h"

#include "Framework/World.h"

#include "Rendering/RenderDevice.h"

#include "Input/InputMapper.h"
#include "Input/InputReader.h"
#include "Input/InputAxis.h"

#include "Maths/Matrix.h"
#include "Maths/MatrixTransformations.h"
#include "Maths/Vector.h"
#include "Maths/Helpers.h"

#include "Framework/StaticGeometry.h"

#include "Framework/Cameras/FreeCamera.h"
#include "Framework/Transform.h"

#include "Core/StringHelpers.h"

static char  g_BaseBuffer[128];
static void* g_AllocatedTable;

static LinearAllocator* g_GlobalAllocator;
static DisplaySurface* g_DisplaySurface;
static InputMapper* g_InputMapper;
static InputReader* g_InputReader;
static RenderDevice* g_RenderDevice;
static VirtualFileSystem* g_VirtualFileSystem;
static FileSystemNative* g_SaveFileSystem;
static FileSystemNative* g_DataFileSystem;
static FileSystemNative* g_EdAssetsFileSystem;
static FileSystemNative* g_RendererFileSystem;
static FileSystemArchive* g_GameFileSystem;

DUSK_ENV_VAR( WindowMode, WINDOWED_MODE, eWindowMode ) // Defines application window mode [Windowed/Fullscreen/Borderless]
DUSK_ENV_VAR( EnableVSync, true, bool ); // "Enable Vertical Synchronisation [false/true]"
DUSK_ENV_VAR( ScreenSize, dkVec2u( 1280, 720 ), dkVec2u ); // "Defines application screen size [0..N]"
DUSK_ENV_VAR( RefreshRate, 0, i32 ) // "Refresh rate. If -1, the application will find the highest refresh rate possible and use it"
DUSK_ENV_VAR_TRANSIENT( IsFirstLaunch, false, bool ) // "True if this is the first time the editor is launched" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseDebugLayer, false, bool ); // "Enable render debug context/layers" [false/true]
DUSK_ENV_VAR( MonitorIndex, 0, i32 ) // "Monitor index used for render device creation. If 0, will use the primary monitor as a display."

void InitializeIOSubsystems()
{
    DUSK_LOG_INFO( "Initializing I/O subsystems...\n" );

    g_VirtualFileSystem = dk::core::allocate<VirtualFileSystem>( g_GlobalAllocator );

    dkString_t cfgFilesDir;
    dk::core::RetrieveSavedGamesDirectory( cfgFilesDir );

    if ( cfgFilesDir.empty() ) {
        DUSK_LOG_WARN( "Failed to retrieve 'Saved Games' folder (this is expected behavior on Unix)\n" );

        dk::core::RetrieveHomeDirectory( cfgFilesDir );

        DUSK_RAISE_FATAL_ERROR( !cfgFilesDir.empty(), "Failed to retrieve a suitable directory for savegame storage on your system... (cfgFilesDir = %s)", cfgFilesDir.c_str() );
    }

    // Prepare files/folders stored on the system fs
    // For now, configuration/save files will be stored in the same folder
    // This might get refactored later (e.g. to implement profile specific config/save for each system user)
    FileSystemNative* saveFolder = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, cfgFilesDir );

#if DUSK_UNIX
    // Use *nix style configuration folder name
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/.duskTest/" );
#else
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/DuskTest/" );
#endif

    DUSK_LOG_INFO( "Mounting filesystems...\n" );

    g_DataFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./data/" ) );

    g_VirtualFileSystem->mount( g_DataFileSystem, DUSK_STRING( "GameData" ), 1 );

    g_GameFileSystem = dk::core::allocate<FileSystemArchive>( g_GlobalAllocator, g_GlobalAllocator, DUSK_STRING( "./Game.zip" ) );
    g_VirtualFileSystem->mount( g_GameFileSystem, DUSK_STRING( "GameData/" ), 0 );

#if DUSK_DEVBUILD
    DUSK_LOG_INFO( "Mounting devbuild filesystems...\n" );

    // TODO Make this more flexible? (do not assume the current working directory).
    g_EdAssetsFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./../../Assets/" ) );
    g_RendererFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./../../Dusk/Graphics/" ) );

    g_VirtualFileSystem->mount( g_EdAssetsFileSystem, DUSK_STRING( "EditorAssets" ), 0 );
    g_VirtualFileSystem->mount( g_RendererFileSystem, DUSK_STRING( "EditorAssets" ), 1 );
    g_VirtualFileSystem->mount( g_EdAssetsFileSystem, DUSK_STRING( "GameData" ), 1 );
#endif

    dkString_t SaveFolder = saveFolder->resolveFilename( DUSK_STRING( "SaveData/" ), configurationFolderName );

    if ( !saveFolder->fileExists( SaveFolder ) ) {
        DUSK_LOG_INFO( "First run detected! Creating save folder at '%s')\n", SaveFolder.c_str() );

        saveFolder->createFolder( SaveFolder );
    }
    dk::core::free( g_GlobalAllocator, saveFolder );

	// TODO Store thoses paths somewhere so that we don't hardcode this everywhere...
	if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/" ) ) ) {
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/materials/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/failed_shaders/" ) );
	}

    Logger::SetLogOutputFile( SaveFolder, DUSK_STRING( "DuskTest" ) );

    DUSK_LOG_INFO( "SaveData folder at : '%s'\n", SaveFolder.c_str() );

    g_SaveFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, SaveFolder );
    g_VirtualFileSystem->mount( g_SaveFileSystem, DUSK_STRING( "SaveData" ), UINT64_MAX );

    FileSystemObject* envConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( envConfigurationFile == nullptr ) {
        IsFirstLaunch = true;

        DUSK_LOG_INFO( "Creating default user configuration!\n" );
        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    } else {
        DUSK_LOG_INFO( "Loading user configuration...\n" );
        EnvironmentVariables::deserialize( envConfigurationFile );
        envConfigurationFile->close();
    }
}

void InitializeRenderSubsystems()
{
    DUSK_LOG_INFO( "Initializing Render subsystems...\n" );

    g_DisplaySurface = dk::core::allocate<DisplaySurface>( g_GlobalAllocator, g_GlobalAllocator );
    g_DisplaySurface->create( ScreenSize.x, ScreenSize.y, eDisplayMode::WINDOWED, MonitorIndex );
    g_DisplaySurface->setCaption( DUSK_STRING( "DuskTest" ) );

    switch ( WindowMode ) {
    case WINDOWED_MODE:
        g_DisplaySurface->changeDisplayMode( eDisplayMode::WINDOWED );
        break;
    case FULLSCREEN_MODE:
        g_DisplaySurface->changeDisplayMode( eDisplayMode::FULLSCREEN );
        break;
    case BORDERLESS_MODE:
        g_DisplaySurface->changeDisplayMode( eDisplayMode::BORDERLESS );
        break;
    }
    
    DUSK_LOG_INFO( "Creating RenderDevice (%s)...\n", RenderDevice::getBackendName() );

    g_RenderDevice = dk::core::allocate<RenderDevice>( g_GlobalAllocator, g_GlobalAllocator );
    g_RenderDevice->create( *g_DisplaySurface, RefreshRate, UseDebugLayer );
    g_RenderDevice->enableVerticalSynchronisation( EnableVSync );

    if ( RefreshRate == 0 ) {
        // Update configuration file with the highest refresh rate possible.
        RefreshRate = g_RenderDevice->getActiveRefreshRate();

        DUSK_LOG_INFO( "RenderDevice returned default refresh rate: %uHz\n", RefreshRate );

        // Update Configuration file.
        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    }

    g_GpuProfiler.create( *g_RenderDevice );
}

void InitializeInputSubsystems()
{
    DUSK_LOG_INFO( "Initializing input subsystems...\n" );

    g_InputMapper = dk::core::allocate<InputMapper>( g_GlobalAllocator );
    g_InputReader = dk::core::allocate<InputReader>( g_GlobalAllocator );
    g_InputReader->create();
}

void Initialize( const char* cmdLineArgs )
{
    Timer profileTimer;
    profileTimer.start();

    DUSK_LOG_RAW( "================================\nDusk Editor %s\n%hs\nCompiled with: %s\n================================\n\n", DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    g_AllocatedTable = dk::core::malloc( 1024 << 20 );
    g_GlobalAllocator = new ( g_BaseBuffer ) LinearAllocator( 1024 << 20, g_AllocatedTable );

    DUSK_LOG_INFO( "Global memory table allocated at: 0x%x\n", g_AllocatedTable );

    InitializeIOSubsystems();

    // Wait until I/O subsystems are initialized (otherwise command line arguments
    // might get overriden by user configuration files; which is not the expected
    // behavior!)
    dk::core::ReadCommandLineArgs( cmdLineArgs );

    InitializeInputSubsystems();
    InitializeRenderSubsystems();

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void MainLoop()
{
    while ( 1 ) {
        g_DisplaySurface->pollSystemEvents( g_InputReader );

        if ( g_DisplaySurface->hasReceivedQuitSignal() ) {
            break;
        }

        if ( g_DisplaySurface->hasReceivedResizeEvent() ) {
            u32 surfaceWidth = g_DisplaySurface->getWidth();
            u32 surfaceHeight = g_DisplaySurface->getHeight();

            g_RenderDevice->resizeBackbuffer( surfaceWidth, surfaceHeight );

            // Update screen size (it will update the widget dimensions at the same time).
            ScreenSize.x = surfaceWidth;
            ScreenSize.y = surfaceHeight;

            // Clear the surface flag.
            g_DisplaySurface->acknowledgeResizeEvent();
        }

        g_InputReader->onFrame( g_InputMapper );

        // Update Local Game Instance
        g_InputMapper->update( 0.0f );
        g_InputMapper->clear();

        g_RenderDevice->present();
    }
}

void Shutdown()
{
    DUSK_LOG_RAW( "\n================================\nShutdown has started!\n================================\n\n" );

    // Forces buffer swapping in order to safely unload/destroy resources in use
    g_RenderDevice->waitForPendingFrameCompletion();

    DUSK_LOG_INFO( "Calling subsystems destructors...\n" );

#if DUSK_DEVBUILD
    dk::core::free( g_GlobalAllocator, g_EdAssetsFileSystem );
    dk::core::free( g_GlobalAllocator, g_RendererFileSystem );
#endif
    
    dk::core::free( g_GlobalAllocator, g_GameFileSystem );
    dk::core::free( g_GlobalAllocator, g_DataFileSystem );
    dk::core::free( g_GlobalAllocator, g_SaveFileSystem );
    dk::core::free( g_GlobalAllocator, g_VirtualFileSystem );
    dk::core::free( g_GlobalAllocator, g_RenderDevice );
    dk::core::free( g_GlobalAllocator, g_DisplaySurface );

    DUSK_LOG_INFO( "Freeing allocated memory...\n" );

    g_GlobalAllocator->clear();
    g_GlobalAllocator->~LinearAllocator();
    dk::core::free( g_AllocatedTable );

    DUSK_LOG_INFO( "Closing logger file stream...\n" );

    Logger::CloseOutputStreams();
}

void dk::test::Start( const char* cmdLineArgs )
{
    Initialize( cmdLineArgs );
    MainLoop();
    Shutdown();
}
