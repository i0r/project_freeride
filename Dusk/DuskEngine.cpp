/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "DuskEngine.h"

#include "Core/Allocators/AllocationHelpers.h"
#include "Core/Allocators/LinearAllocator.h"
#include "Core/CommandLineArgs.h"
#include "Core/Environment.h"
#include "Core/Display/DisplaySurface.h"

#include "Framework/World.h"

#include "FileSystem/VirtualFileSystem.h"
#include "FileSystem/FileSystemNative.h"
#include "FileSystem/FileSystemArchive.h"

#include "Input/InputReader.h"
#include "Input/InputMapper.h"

#include "Graphics/WorldRenderer.h"
#include "Graphics/RenderWorld.h"
#include "Graphics/HudRenderer.h"
#include "Graphics/ShaderCache.h"
#include "Graphics/DrawCommandBuilder.h"
#include "Graphics/GraphicsAssetCache.h"

#include "Physics/DynamicsWorld.h"

static char    g_BaseBuffer[128]; 
DuskEngine     __DuskEngine_Instance__;

DuskEngine*    g_DuskEngine = &__DuskEngine_Instance__;

DUSK_ENV_VAR( GlobalMemoryTableSize, 1024 << 20, u32 ); // Size of the memory chunk reserved at launch for runtime allocation.
DUSK_ENV_VAR( WindowMode, WINDOWED_MODE, eWindowMode ) // Defines application window mode [Windowed/Fullscreen/Borderless]
DUSK_ENV_VAR( EnableVSync, true, bool ); // "Enable Vertical Synchronisation [false/true]"
DUSK_ENV_VAR( ScreenSize, dkVec2u( 1280, 720 ), dkVec2u ); // "Defines application screen size [0..N]"
DUSK_ENV_VAR( MSAASamplerCount, 1, u32 ) // "MSAA sampler count [1..N]"
DUSK_ENV_VAR( ImageQuality, 1.0f, f32 ) // "Image Quality factor [0.1..N]"
DUSK_ENV_VAR( RefreshRate, -1, i32 ) // "Refresh rate. If -1, the application will find the highest refresh rate possible and use it"
DUSK_ENV_VAR( DefaultCameraFov, 90.0f, f32 ) // "Default Field Of View set to a new Camera"
DUSK_ENV_VAR_TRANSIENT( IsFirstLaunch, false, bool ) // "True if this is the first time the application is launched" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseDebugLayer, false, bool ); // "Enable render debug context/layers" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseRenderDocCapture, false, bool );// "Use RenderDoc frame capture tool [false/true]" (note: renderdoc dynamic lib must be present in the working dir)
DUSK_DEV_VAR( DisplayCulledPrimCount, "Display the number of primitive culled for the Viewport[0]", false, bool );
DUSK_DEV_VAR( DisplayFramerate, "Display basic framerate infos", true, bool );
DUSK_ENV_VAR( MonitorIndex, 0, i32 ) // "Monitor index used for render device creation. If 0, will use the primary monitor as a display."
DUSK_DEV_VAR( LogicTickrate, "Number of logic tick executed per frame", 300, i32 ) //
DUSK_DEV_VAR( PhysicsTickrate, "Number of physics tick executed per frame", 100, i32 ) //

DuskEngine::DuskEngine()
    : applicationName( DUSK_STRING( "DuskEngine" ) )
    , deltaTime( 0.0f )
    , allocatedTable( nullptr )
    , globalAllocator( nullptr )
    , virtualFileSystem( nullptr )
    , dataFileSystem( nullptr )
    , gameFileSystem( nullptr )
    , saveFileSystem( nullptr )
#if DUSK_DEVBUILD
    , edAssetsFileSystem( nullptr )
    , rendererFileSystem( nullptr )
#endif
    , inputMapper( nullptr )
    , inputReader( nullptr )
#if DUSK_USE_RENDERDOC
    , renderDocHelper( nullptr )
#endif
    , mainDisplaySurface( nullptr )
    , renderDevice( nullptr )
    , shaderCache( nullptr )
    , graphicsAssetCache( nullptr )
    , worldRenderer( nullptr )
    , hudRenderer( nullptr )
    , renderWorld( nullptr )
    , drawCommandBuilder( nullptr )
    , world( nullptr )
    , dynamicsWorld( nullptr )
{
    static bool HasBeenCalledOnce = false;
    DUSK_RAISE_FATAL_ERROR( !HasBeenCalledOnce, "A game engine instance has already been created!" );
    HasBeenCalledOnce = true;
}

DuskEngine::~DuskEngine()
{

}

void DuskEngine::create( const char* cmdLineArgs )
{
    Timer profileTimer;
    profileTimer.start();
    DUSK_LOG_RAW( "================================\n%s %s\n%hs\nCompiled with: %s\n================================\n\n", applicationName.c_str(), DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    // Allocate a memory chunk for every subsystem used by the engine.
    allocatedTable = dk::core::malloc( GlobalMemoryTableSize );
    DUSK_LOG_INFO( "Global memory table allocated at: 0x%x (%ull bytes)\n", allocatedTable, GlobalMemoryTableSize );
    globalAllocator = new ( g_BaseBuffer ) LinearAllocator( GlobalMemoryTableSize, allocatedTable );

    initializeIoSubsystems();

    // Wait until I/O subsystems are initialized (otherwise command line arguments
    // might get overriden by user configuration files; which is not the expected
    // behavior!)
    dk::core::ReadCommandLineArgs( cmdLineArgs );

    initializeInputSubsystems();
    initializeRenderSubsystems();
    initializeLogicSubsystems();

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void DuskEngine::shutdown()
{
    DUSK_LOG_RAW( "\n================================\nShutdown has started!\n================================\n\n" );

    // Forces buffer swapping in order to safely unload/destroy resources in use
    renderDevice->waitForPendingFrameCompletion();

    DUSK_LOG_INFO( "Releasing GPU resources...\n" );

    worldRenderer->destroy( renderDevice );
    renderWorld->destroy( *renderDevice );

    DUSK_LOG_INFO( "Calling subsystems destructors...\n" );

    // Input
    dk::core::free( globalAllocator, inputMapper );
    dk::core::free( globalAllocator, inputReader );

    // IO
#if DUSK_DEVBUILD
    dk::core::free( globalAllocator, edAssetsFileSystem );
    dk::core::free( globalAllocator, rendererFileSystem );
#endif

    dk::core::free( globalAllocator, saveFileSystem );
    dk::core::free( globalAllocator, gameFileSystem );
    dk::core::free( globalAllocator, dataFileSystem );
    dk::core::free( globalAllocator, virtualFileSystem );

    // Render
    dk::core::free( globalAllocator, mainDisplaySurface );
    dk::core::free( globalAllocator, renderDevice );
    dk::core::free( globalAllocator, shaderCache );
    dk::core::free( globalAllocator, graphicsAssetCache );
    dk::core::free( globalAllocator, worldRenderer );
    dk::core::free( globalAllocator, hudRenderer );
    dk::core::free( globalAllocator, renderWorld );
    dk::core::free( globalAllocator, drawCommandBuilder );

#if DUSK_USE_RENDERDOC
    dk::core::free( globalAllocator, renderDocHelper );
#endif

    dk::core::free( globalAllocator, world );
    dk::core::free( globalAllocator, dynamicsWorld );

    DUSK_LOG_INFO( "Freeing allocated memory...\n" );

    globalAllocator->clear();
    globalAllocator->~LinearAllocator();
    dk::core::free( globalAllocator );

    DUSK_LOG_INFO( "Closing logger file stream...\n" );

    Logger::CloseOutputStreams();
}

void DuskEngine::setApplicationName( const dkChar_t* name /*= DUSK_STRING( "Dusk GameEngine" ) */ )
{
    if ( name == nullptr ) {
        return;
    }

    applicationName = dkString_t( name );
}

void DuskEngine::mainLoop()
{
    // As ticks
    constexpr uint32_t LOGIC_TICKRATE = 300;
    constexpr uint32_t PHYSICS_TICKRATE = 100;

    // As milliseconds
    constexpr f64 LOGIC_DELTA = 1.0f / static_cast< float >( LOGIC_TICKRATE );
    constexpr f64 PHYSICS_DELTA = 1.0f / static_cast< float >( PHYSICS_TICKRATE );

    Timer logicTimer;
    logicTimer.start();

    deltaTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );

    f64 accumulator = 0.0;

    while ( 1 ) {
        mainDisplaySurface->pollSystemEvents( inputReader );

        if ( mainDisplaySurface->hasReceivedQuitSignal() ) {
            break;
        }

        if ( mainDisplaySurface->hasReceivedResizeEvent() ) {
            u32 surfaceWidth = mainDisplaySurface->getWidth();
            u32 surfaceHeight = mainDisplaySurface->getHeight();

            // Update screen size (it will update the widget dimensions at the same time).
            ScreenSize.x = surfaceWidth;
            ScreenSize.y = surfaceHeight;

            renderDevice->resizeBackbuffer( ScreenSize.x, ScreenSize.y );

            // ResizeEventDispatcher.notifyListeners();

            mainDisplaySurface->acknowledgeResizeEvent();
        }

        deltaTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );
        accumulator += static_cast< f64 >( deltaTime );

        while ( accumulator >= LOGIC_DELTA ) {
            DUSK_CPU_PROFILE_SCOPED( "Fixed Step Update" );

            // Update Input
            inputReader->onFrame( inputMapper );

            // Update Local Game Instance
            inputMapper->update( LOGIC_DELTA );
            inputMapper->clear();

//#if DUSK_USE_IMGUI
//            g_ImGuiManager->update( LOGIC_DELTA );
//#endif

            world->update( LOGIC_DELTA );

            // Game Logic
            //g_FreeCamera->update( LOGIC_DELTA );

            dynamicsWorld->tick( LOGIC_DELTA );

            accumulator -= LOGIC_DELTA;
        }

        // Update the main display surface (poll events and update the surface).

        // Update the subsystem logics (fixed step)

        // For each GameViewport
            // Update the viewport 
    }
}

f32 DuskEngine::getDeltaTime() const
{
    return deltaTime;
}

void DuskEngine::initializeIoSubsystems()
{
    DUSK_LOG_INFO( "Initializing I/O subsystems...\n" );

    virtualFileSystem = dk::core::allocate<VirtualFileSystem>( globalAllocator );

    dkString_t cfgFilesDir;
    dk::core::RetrieveSavedGamesDirectory( cfgFilesDir );

    if ( cfgFilesDir.empty() ) {
        DUSK_LOG_WARN( "Failed to retrieve 'Saved Games' folder (this is expected behavior on Unix)\n" );

        dk::core::RetrieveHomeDirectory( cfgFilesDir );

        DUSK_RAISE_FATAL_ERROR( !cfgFilesDir.empty(), "Failed to retrieve a suitable directory for savegame storage on your system... (cfgFilesDir = %s)", cfgFilesDir.c_str() );
    }

#if DUSK_UNIX
    // Use *nix style configuration folder name
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/.dusked/" );
#else
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/DuskEd/" );
#endif

    FileSystemNative saveFs( cfgFilesDir );
    dkString_t saveFolder = saveFs.resolveFilename( DUSK_STRING( "SaveData/" ), configurationFolderName );

    if ( !saveFs.fileExists( saveFolder ) ) {
        DUSK_LOG_INFO( "First run detected! Creating save folder at '%s')\n", saveFolder.c_str() );

        saveFs.createFolder( saveFolder );
    }

    Logger::SetLogOutputFile( saveFolder, DUSK_STRING( "DuskEd" ) );

    DUSK_LOG_INFO( "SaveData folder at : '%s'\n", saveFolder.c_str() );

    DUSK_LOG_INFO( "Mounting filesystems...\n" );

    dataFileSystem = dk::core::allocate<FileSystemNative>( globalAllocator, DUSK_STRING( "./data/" ) );
    virtualFileSystem->mount( dataFileSystem, DUSK_STRING( "GameData" ), 1 );

    // TODO Store thoses paths somewhere so that we don't hardcode this everywhere...
    if ( !dataFileSystem->fileExists( DUSK_STRING( "./data/" ) ) ) {
        dataFileSystem->createFolder( DUSK_STRING( "./data/" ) );
        dataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
        dataFileSystem->createFolder( DUSK_STRING( "./data/materials/" ) );
        dataFileSystem->createFolder( DUSK_STRING( "./data/failed_shaders/" ) );
    }

    gameFileSystem = dk::core::allocate<FileSystemArchive>( globalAllocator, globalAllocator, DUSK_STRING( "./Game.zip" ) );
    // virtualFileSystem->mount( gameFileSystem, DUSK_STRING( "GameData/" ), 0 );

    saveFileSystem = dk::core::allocate<FileSystemNative>( globalAllocator, saveFolder );
    virtualFileSystem->mount( saveFileSystem, DUSK_STRING( "SaveData" ), UINT64_MAX );

#if DUSK_DEVBUILD
    DUSK_LOG_INFO( "Mounting devbuild filesystems...\n" );

    // TODO Make this more flexible? (do not assume the current working directory).
    edAssetsFileSystem = dk::core::allocate<FileSystemNative>( globalAllocator, DUSK_STRING( "./../../Assets/" ) );
    rendererFileSystem = dk::core::allocate<FileSystemNative>( globalAllocator, DUSK_STRING( "./../../Dusk/Graphics/" ) );

    virtualFileSystem->mount( edAssetsFileSystem, DUSK_STRING( "EditorAssets" ), 0 );
    virtualFileSystem->mount( rendererFileSystem, DUSK_STRING( "EditorAssets" ), 1 );
    virtualFileSystem->mount( edAssetsFileSystem, DUSK_STRING( "GameData" ), 1 );
#endif

    FileSystemObject* envConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( envConfigurationFile == nullptr ) {
        IsFirstLaunch = true;

        DUSK_LOG_INFO( "Creating default user configuration!\n" );
        FileSystemObject* newEnvConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    } else {
        DUSK_LOG_INFO( "Loading user configuration...\n" );
        EnvironmentVariables::deserialize( envConfigurationFile );
        envConfigurationFile->close();
    }

#if 0
    g_FileSystemWatchdog = dk::core::allocate<FileSystemWatchdog>( g_GlobalAllocator );
    g_FileSystemWatchdog->create();
#endif
}

void DuskEngine::initializeInputSubsystems()
{
    DUSK_LOG_INFO( "Initializing input subsystems...\n" );

    inputMapper = dk::core::allocate<InputMapper>( globalAllocator );
    inputReader = dk::core::allocate<InputReader>( globalAllocator );
    inputReader->create();

    FileSystemObject* inputConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( inputConfigurationFile == nullptr ) {
        DUSK_LOG_INFO( "Creating default input configuration file...\n" );

        FileSystemObject* newInputConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        //dk::core::WriteDefaultInputCfg( newInputConfigurationFile, g_InputReader->getActiveInputLayout() );
        newInputConfigurationFile->close();

        inputConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    }

    DUSK_LOG_INFO( "Loading input configuration...\n" );
    inputMapper->deserialize( inputConfigurationFile );
    inputConfigurationFile->close();
}

void DuskEngine::initializeRenderSubsystems()
{
    DUSK_LOG_INFO( "Initializing Render subsystems...\n" );

#if DUSK_USE_RENDERDOC
    // For convenience, allocate RenderDoc Helper even if the environment variable is
    // set to false
    renderDocHelper = dk::core::allocate<RenderDocHelper>( globalAllocator );

    // Create as early as possible to let RenderDoc hook API libraries
    if ( UseRenderDocCapture ) {
        renderDocHelper->create();

        // We need to explicitly disable vendor extensions since those aren't
        // supported by RenderDoc.
        bool* disableVendorExtension = EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "DisableVendorExtensions" ) );
        *disableVendorExtension = true;
    }
#endif

    mainDisplaySurface = dk::core::allocate<DisplaySurface>( globalAllocator, globalAllocator );
    mainDisplaySurface->create( ScreenSize.x, ScreenSize.y, eDisplayMode::WINDOWED, MonitorIndex );
    mainDisplaySurface->setCaption( DUSK_STRING( "DuskEd" ) );

    switch ( WindowMode ) {
    case WINDOWED_MODE:
        mainDisplaySurface->changeDisplayMode( eDisplayMode::WINDOWED );
        break;
    case FULLSCREEN_MODE:
        mainDisplaySurface->changeDisplayMode( eDisplayMode::FULLSCREEN );
        break;
    case BORDERLESS_MODE:
        mainDisplaySurface->changeDisplayMode( eDisplayMode::BORDERLESS );
        break;
    }

    DUSK_LOG_INFO( "Creating RenderDevice (%s)...\n", RenderDevice::getBackendName() );

    renderDevice = dk::core::allocate<RenderDevice>( globalAllocator, globalAllocator );
    renderDevice->create( *mainDisplaySurface, RefreshRate, UseDebugLayer );
    renderDevice->enableVerticalSynchronisation( EnableVSync );

    if ( RefreshRate == -1 ) {
        // Update configuration file with the highest refresh rate possible.
        RefreshRate = renderDevice->getActiveRefreshRate();

        DUSK_LOG_INFO( "RenderDevice returned default refresh rate: %uHz\n", RefreshRate );

        // Update Configuration file.
        FileSystemObject* newEnvConfigurationFile = virtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    }

    shaderCache = dk::core::allocate<ShaderCache>( globalAllocator, globalAllocator, renderDevice, virtualFileSystem );
    graphicsAssetCache = dk::core::allocate<GraphicsAssetCache>( globalAllocator, globalAllocator, renderDevice, shaderCache, virtualFileSystem );

    worldRenderer = dk::core::allocate<WorldRenderer>( globalAllocator, globalAllocator );
    worldRenderer->loadCachedResources( renderDevice, shaderCache, graphicsAssetCache, virtualFileSystem );

    hudRenderer = dk::core::allocate<HUDRenderer>( globalAllocator, globalAllocator );
    hudRenderer->loadCachedResources( *renderDevice, *shaderCache, *graphicsAssetCache );

    renderWorld = dk::core::allocate<RenderWorld>( globalAllocator, globalAllocator );
    renderWorld->create( *renderDevice );

#if DUSK_USE_RENDERDOC
    // Wait until the RenderDevice has been created before doing the attachment
    if ( UseRenderDocCapture ) {
        renderDocHelper->attachTo( *mainDisplaySurface, *renderDevice );
    }
#endif

    drawCommandBuilder = dk::core::allocate<DrawCommandBuilder>( globalAllocator, globalAllocator );

    g_GpuProfiler.create( *renderDevice );
}

void DuskEngine::initializeLogicSubsystems()
{
    world = dk::core::allocate<World>( globalAllocator, globalAllocator );
    world->create();

    dynamicsWorld = dk::core::allocate<DynamicsWorld>( globalAllocator, globalAllocator );
}
