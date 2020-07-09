/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Editor.h"

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

#include "Graphics/RenderModules/PresentRenderPass.h"
#include "Graphics/RenderModules/AtmosphereRenderModule.h"
#include "Graphics/RenderModules/WorldRenderModule.h"
#include "Graphics/RenderModules/EditorGridRenderModule.h"

#include "Graphics/Material.h"

#include "Graphics/ShaderCache.h"
#include "Graphics/GraphicsAssetCache.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/HudRenderer.h"
#include "Graphics/LightGrid.h"
#include "Graphics/RenderWorld.h" // TODO Should be refactored/deprecated soon
#include "Graphics/DrawCommandBuilder.h"
#include "Graphics/RenderModules/EditorGridRenderModule.h"

#if DUSK_USE_RENDERDOC
#include "Graphics/RenderDocHelper.h"
#endif

#if DUSK_USE_IMGUI
#include "Core/ImGuiManager.h"
#include "Graphics/RenderModules/ImGuiRenderModule.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/ImGuizmo/ImGuizmo.h"

#include "Framework/LoggingConsole.h"
#endif

#include "Physics/BulletDynamicsWorld.h"

#include "Framework/MaterialEditor.h"
#include "Framework/EntityEditor.h"

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
#include "DefaultInputConfig.h"


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
static ShaderCache* g_ShaderCache;
static GraphicsAssetCache* g_GraphicsAssetCache;
static WorldRenderer* g_WorldRenderer;
static HUDRenderer* g_HUDRenderer;
static RenderWorld* g_RenderWorld;
static World* g_World;
static DrawCommandBuilder* g_DrawCommandBuilder;
static EntityEditor* g_EntityEditor;

static FreeCamera* g_FreeCamera;
static MaterialEditor* g_MaterialEditor;
static EditorGridModule* g_EditorGridModule;

static FileSystemNative* g_EdAssetsFileSystem;
static FileSystemNative* g_RendererFileSystem;
static FileSystemArchive* g_GameFileSystem;

#if DUSK_USE_RENDERDOC
static RenderDocHelper* g_RenderDocHelper;
#endif

#if DUSK_USE_IMGUI
static ImGuiManager* g_ImGuiManager;
static ImGuiRenderModule* g_ImGuiRenderModule;
#endif

static DynamicsWorld* g_DynamicsWorld;

static bool                    g_IsGamePaused = false;
static bool                    g_IsFirstLaunch = false;
static bool                    g_IsMouseOverViewportWindow = false;
static bool                    g_CanMoveCamera = false;
static bool                    g_IsResizing = true;
static bool                    g_RequestPickingUpdate = false;
static bool                    g_RightClickMenuOpened = false;
static dkVec2u                 g_CursorPosition = dkVec2u( 0, 0 );
static dkVec2u                 g_ViewportWindowPosition = dkVec2u( 0, 0 );
static Entity                  g_PickedEntity = Entity();

DUSK_ENV_VAR( EnableVSync, true, bool ); // "Enable Vertical Synchronisation [false/true]"
DUSK_ENV_VAR( ScreenSize, dkVec2u( 1280, 720 ), dkVec2u ); // "Defines application screen size [0..N]"
DUSK_ENV_VAR( MSAASamplerCount, 1, u32 ) // "MSAA sampler count [1..N]"
DUSK_ENV_VAR( ImageQuality, 1.0f, f32 ) // "Image Quality factor [0.1..N]"
DUSK_ENV_VAR( RefreshRate, -1, u32 ) // "Refresh rate. If -1, the application will find the highest refresh rate possible and use it"
DUSK_ENV_VAR( DefaultCameraFov, 90.0f, f32 ) // "Default Field Of View set to a new Camera"
DUSK_DEV_VAR_PERSISTENT( UseDebugLayer, false, bool ); // "Enable render debug context/layers" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseRenderDocCapture, false, bool );// "Use RenderDoc frame capture tool [false/true]"
                                                           // (note: renderdoc dynamic lib must be present in the working dir)

void UpdateFreeCamera( MappedInput& input, f32 deltaTime )
{
    if ( !g_CanMoveCamera ) {
        return;
    }

    // Camera Controls
    auto axisX = input.Ranges[DUSK_STRING_HASH( "CameraMoveHorizontal" )];
    auto axisY = input.Ranges[DUSK_STRING_HASH( "CameraMoveVertical" )];

    g_FreeCamera->updateMouse( deltaTime, axisX, axisY );

    if ( input.States.find( DUSK_STRING_HASH( "CameraMoveRight" ) ) != input.States.end() ) {
        g_FreeCamera->moveRight( deltaTime );
    }

    if ( input.States.find( DUSK_STRING_HASH( "CameraMoveLeft" ) ) != input.States.end() ) {
        g_FreeCamera->moveLeft( deltaTime );
    }

    if ( input.States.find( DUSK_STRING_HASH( "CameraMoveForward" ) ) != input.States.end() ) {
        g_FreeCamera->moveForward( deltaTime );
    }

    if ( input.States.find( DUSK_STRING_HASH( "CameraMoveBackward" ) ) != input.States.end() ) {
        g_FreeCamera->moveBackward( deltaTime );
    }

    if ( input.States.find( DUSK_STRING_HASH( "CameraLowerAltitude" ) ) != input.States.end() ) {
        g_FreeCamera->lowerAltitude( deltaTime );
    }

    if ( input.States.find( DUSK_STRING_HASH( "CameraTakeAltitude" ) ) != input.States.end() ) {
        g_FreeCamera->takeAltitude( deltaTime );
    }
}

void UpdateEditor( MappedInput& input, f32 deltaTime )
{
    const bool previousCamState = g_CanMoveCamera;
    const bool isRightButtonDown = input.States.find( DUSK_STRING_HASH( "RightMouseButton" ) ) != input.States.end();
    const bool newCanMoveCamera = ( g_IsMouseOverViewportWindow && isRightButtonDown );

    g_CanMoveCamera = newCanMoveCamera;

    if ( previousCamState != g_CanMoveCamera ) {
        if ( g_CanMoveCamera ) {
            g_InputMapper->popContext();
        } else {
            g_InputMapper->pushContext( DUSK_STRING_HASH( "Editor" ) );
        }
    }

    // Default: Ctrl
    if ( input.States.find( DUSK_STRING_HASH( "Modifier1" ) ) != input.States.end() ) {
        /* if ( input.Actions.find( DUSK_STRING_HASH( "Undo" ) ) != input.Actions.end() ) {
             g_TransactionHandler->undo();
         }

         if ( input.Actions.find( DUSK_STRING_HASH( "Redo" ) ) != input.Actions.end() ) {
             g_TransactionHandler->redo();
         }*/
    }

    if ( g_IsMouseOverViewportWindow ) {
        if ( input.States.find( DUSK_STRING_HASH( "LeftMouseButton" ) ) != input.States.end() ) {
            g_RequestPickingUpdate = ( !g_RightClickMenuOpened && !g_EntityEditor->isManipulatingTransform() );
        }
    }

    if ( input.States.find( DUSK_STRING_HASH( "KeyDelete" ) ) != input.States.end() ) {
        if ( g_PickedEntity.getIdentifier() != Entity::INVALID_ID ) {
            g_World->releaseEntity( g_PickedEntity );
            g_PickedEntity.setIdentifier( Entity::INVALID_ID );
        }
    }

    // Forward input states to ImGui
    f32 rawX = dk::maths::clamp( static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_X ) ), 0.0f, static_cast< f32 >( ScreenSize.x ) );
    f32 rawY = dk::maths::clamp( static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_Y ) ), 0.0f, static_cast< f32 >( ScreenSize.y ) );
    f32 mouseWheel = static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_SCROLL_WHEEL ) );

    g_CursorPosition.x = static_cast< u32 >( rawX );
    g_CursorPosition.y = static_cast< u32 >( rawY );

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2( rawX, rawY );
    io.MouseWheel = mouseWheel;

    io.KeyCtrl = ( input.States.find( DUSK_STRING_HASH( "KeyCtrl" ) ) != input.States.end() );
    io.KeyShift = ( input.States.find( DUSK_STRING_HASH( "KeyShift" ) ) != input.States.end() );
    io.KeyAlt = ( input.States.find( DUSK_STRING_HASH( "KeyAlt" ) ) != input.States.end() );

    io.KeysDown[io.KeyMap[ImGuiKey_Backspace]] = ( input.States.find( DUSK_STRING_HASH( "KeyBackspace" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Delete]] = ( input.States.find( DUSK_STRING_HASH( "KeyDelete" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_A]] = ( input.States.find( DUSK_STRING_HASH( "KeyA" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_C]] = ( input.States.find( DUSK_STRING_HASH( "KeyC" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_V]] = ( input.States.find( DUSK_STRING_HASH( "KeyV" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_X]] = ( input.States.find( DUSK_STRING_HASH( "KeyX" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Y]] = ( input.States.find( DUSK_STRING_HASH( "KeyY" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Z]] = ( input.States.find( DUSK_STRING_HASH( "KeyZ" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Escape]] = ( input.States.find( DUSK_STRING_HASH( "KeyEscape" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Enter]] = ( input.States.find( DUSK_STRING_HASH( "KeyEnter" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Space]] = ( input.States.find( DUSK_STRING_HASH( "KeySpace" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Tab]] = ( input.States.find( DUSK_STRING_HASH( "KeyTab" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Insert]] = ( input.States.find( DUSK_STRING_HASH( "KeyInsert" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_End]] = ( input.States.find( DUSK_STRING_HASH( "KeyEnd" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_Home]] = ( input.States.find( DUSK_STRING_HASH( "KeyHome" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_PageDown]] = ( input.States.find( DUSK_STRING_HASH( "KeyPageDown" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_PageUp]] = ( input.States.find( DUSK_STRING_HASH( "KeyPageUp" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_DownArrow]] = ( input.States.find( DUSK_STRING_HASH( "KeyDownArrow" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_UpArrow]] = ( input.States.find( DUSK_STRING_HASH( "KeyUpArrow" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_RightArrow]] = ( input.States.find( DUSK_STRING_HASH( "KeyRightArrow" ) ) != input.States.end() );
    io.KeysDown[io.KeyMap[ImGuiKey_LeftArrow]] = ( input.States.find( DUSK_STRING_HASH( "KeyLeftArrow" ) ) != input.States.end() );

    io.MouseDown[0] = ( input.States.find( DUSK_STRING_HASH( "LeftMouseButton" ) ) != input.States.end() );
    io.MouseDown[1] = isRightButtonDown;
    io.MouseDown[2] = ( input.States.find( DUSK_STRING_HASH( "MiddleMouseButton" ) ) != input.States.end() );

    fnKeyStrokes_t keyStrokes = g_InputReader->getAndFlushKeyStrokes();
    for ( dk::input::eInputKey key : keyStrokes ) {
        io.AddInputCharacter( key );
    }
}

void RegisterInputContexts()
{
    g_InputMapper->pushContext( DUSK_STRING_HASH( "Game" ) );
	g_InputMapper->pushContext( DUSK_STRING_HASH( "Editor" ) );

    // Free Camera
    g_InputMapper->addCallback( &UpdateFreeCamera, 0 );
    g_InputMapper->addCallback( &UpdateEditor, -1 );
}

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
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/.dusked/" );
#else
    dkString_t configurationFolderName = DUSK_STRING( "SaveData/DuskEd/" );
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
    if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/cache/" ) ) ) {
        g_DataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
    }

    if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/materials/" ) ) ) {
        g_DataFileSystem->createFolder( DUSK_STRING( "./data/materials/" ) );
    }

    Logger::SetLogOutputFile( SaveFolder, DUSK_STRING( "DuskEd" ) );

    DUSK_LOG_INFO( "SaveData folder at : '%s'\n", SaveFolder.c_str() );

    g_SaveFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, SaveFolder );
    g_VirtualFileSystem->mount( g_SaveFileSystem, DUSK_STRING( "SaveData" ), UINT64_MAX );

    FileSystemObject* envConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( envConfigurationFile == nullptr ) {
        g_IsFirstLaunch = true;

        DUSK_LOG_INFO( "Creating default user configuration!\n" );
        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
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

void InitializeInputSubsystems()
{
    DUSK_LOG_INFO( "Initializing input subsystems...\n" );

    g_InputMapper = dk::core::allocate<InputMapper>( g_GlobalAllocator );
    g_InputReader = dk::core::allocate<InputReader>( g_GlobalAllocator );
    g_InputReader->create();

    auto inputConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( inputConfigurationFile == nullptr ) {
        DUSK_LOG_INFO( "Creating default input configuration file...\n" );

        auto newInputConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        dk::core::WriteDefaultInputCfg( newInputConfigurationFile, g_InputReader->getActiveInputLayout() );
        newInputConfigurationFile->close();

        inputConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/input.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
    }

    DUSK_LOG_INFO( "Loading input configuration...\n" );
    g_InputMapper->deserialize( inputConfigurationFile );
    inputConfigurationFile->close();
}

void InitializeRenderSubsystems()
{
    DUSK_LOG_INFO( "Initializing Render subsystems...\n" );

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    // For convenience, allocate RenderDoc Helper even if the environment variable is
    // set to false
    g_RenderDocHelper = dk::core::allocate<RenderDocHelper>( g_GlobalAllocator );

    // Create as early as possible to let RenderDoc hook API libraries
    if ( UseRenderDocCapture ) {
        g_RenderDocHelper->create();
    }
#endif
#endif

    g_DisplaySurface = dk::core::allocate<DisplaySurface>( g_GlobalAllocator, g_GlobalAllocator );
    g_DisplaySurface->create( ScreenSize.x, ScreenSize.y, eDisplayMode::WINDOWED );
    g_DisplaySurface->setCaption( DUSK_STRING( "DuskEd" ) );

    DUSK_LOG_INFO( "Creating RenderDevice (%s)...\n", RenderDevice::getBackendName() );

    g_RenderDevice = dk::core::allocate<RenderDevice>( g_GlobalAllocator, g_GlobalAllocator );
    g_RenderDevice->create( *g_DisplaySurface, RefreshRate, UseDebugLayer );
    g_RenderDevice->enableVerticalSynchronisation( EnableVSync );

    if ( RefreshRate == -1 ) {
        // Update configuration file with the highest refresh rate possible.
        RefreshRate = g_RenderDevice->getActiveRefreshRate();

        DUSK_LOG_INFO( "RenderDevice returned default refresh rate: %uHz\n", RefreshRate );

        // Update Configuration file.
        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    }

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    // Wait until the RenderDevice has been created before doing the attachment
    if ( UseRenderDocCapture ) {
        g_RenderDocHelper->attachTo( *g_DisplaySurface, *g_RenderDevice );
    }
#endif

    g_ShaderCache = dk::core::allocate<ShaderCache>( g_GlobalAllocator, g_GlobalAllocator, g_RenderDevice, g_VirtualFileSystem );
    g_GraphicsAssetCache = dk::core::allocate<GraphicsAssetCache>( g_GlobalAllocator, g_GlobalAllocator, g_RenderDevice, g_ShaderCache, g_VirtualFileSystem );

    g_WorldRenderer = dk::core::allocate<WorldRenderer>( g_GlobalAllocator, g_GlobalAllocator );
    g_WorldRenderer->loadCachedResources( g_RenderDevice, g_ShaderCache, g_GraphicsAssetCache, g_VirtualFileSystem );

    g_HUDRenderer = dk::core::allocate<HUDRenderer>( g_GlobalAllocator, g_GlobalAllocator );
    g_HUDRenderer->loadCachedResources( *g_RenderDevice, *g_ShaderCache, *g_GraphicsAssetCache );

    g_RenderWorld = dk::core::allocate<RenderWorld>( g_GlobalAllocator, g_GlobalAllocator );

#if DUSK_USE_IMGUI
    g_ImGuiManager = dk::core::allocate<ImGuiManager>( g_GlobalAllocator );
	g_ImGuiManager->create( *g_DisplaySurface );
	g_ImGuiManager->setVisible( true );

    g_ImGuiRenderModule = dk::core::allocate<ImGuiRenderModule>( g_GlobalAllocator );
    g_ImGuiRenderModule->loadCachedResources( *g_RenderDevice, *g_GraphicsAssetCache );
#endif
#endif

    g_DrawCommandBuilder = dk::core::allocate<DrawCommandBuilder>( g_GlobalAllocator, g_GlobalAllocator );

    // TODO Retrieve pointer to camera instance from scene db
    g_FreeCamera = new FreeCamera();
    g_FreeCamera->setProjectionMatrix( DefaultCameraFov, static_cast< float >( ScreenSize.x ), static_cast< float >( ScreenSize.y ) );

    g_FreeCamera->setImageQuality( ImageQuality );
    g_FreeCamera->setMSAASamplerCount( MSAASamplerCount );
    // END TEMP
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

    RegisterInputContexts();

    g_MaterialEditor = dk::core::allocate<MaterialEditor>( g_GlobalAllocator, g_GlobalAllocator, g_GraphicsAssetCache, g_VirtualFileSystem );

    g_World = dk::core::allocate<World>( g_GlobalAllocator, g_GlobalAllocator );
    g_World->create();

	g_EntityEditor = dk::core::allocate<EntityEditor>( g_GlobalAllocator, g_GlobalAllocator, g_GraphicsAssetCache, g_VirtualFileSystem, g_RenderWorld, g_RenderDevice );
	g_EntityEditor->setActiveWorld( g_World );
    g_EntityEditor->setActiveEntity( &g_PickedEntity );
    g_EntityEditor->openEditorWindow();

	g_EditorGridModule = dk::core::allocate<EditorGridModule>( g_GlobalAllocator );

    g_DynamicsWorld = dk::core::allocate<DynamicsWorld>( g_GlobalAllocator, g_GlobalAllocator );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void BuildThisFrameGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize )
{
    // Append the regular render pipeline.
    ResHandle_t presentRt = g_WorldRenderer->buildDefaultGraph( frameGraph, scenario, viewportSize );

    // Render editor grid.
    presentRt = g_EditorGridModule->addEditorGridPass( frameGraph, presentRt, g_WorldRenderer->getResolvedDepth() );

    // Render HUD.
    presentRt = g_HUDRenderer->buildDefaultGraph( frameGraph, presentRt );

    // TODO Make a separate EditorRenderer or something like that
#if DUSK_USE_IMGUI
        // ImGui Editor GUI.
    frameGraph.savePresentRenderTarget( presentRt );

    presentRt = g_ImGuiRenderModule->render( frameGraph, presentRt );
#endif

    // Copy to swapchain buffer.
    AddPresentRenderPass( frameGraph, presentRt );
}

void MainLoop()
{
    // As ticks
    constexpr uint32_t LOGIC_TICKRATE = 300;
    constexpr uint32_t PHYSICS_TICKRATE = 100;

    // As milliseconds
    constexpr f64 LOGIC_DELTA = 1.0f / static_cast< float >( LOGIC_TICKRATE );
    constexpr f64 PHYSICS_DELTA = 1.0f / static_cast< float >( PHYSICS_TICKRATE );

    // TEST TEST TEST
    Viewport vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = ScreenSize.x;
    vp.Height = ScreenSize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    ScissorRegion sr;
    sr.Top = 0;
    sr.Bottom = ScreenSize.y;
    sr.Left = 0;
    sr.Right = ScreenSize.x;

    dkVec2f viewportSize = dkVec2f( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) );

    Timer logicTimer;
    logicTimer.start();

    FramerateCounter framerateCounter;
    f32 frameTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );
    f64 accumulator = 0.0;

    while ( 1 ) {
        g_DisplaySurface->pollSystemEvents( g_InputReader );

        if ( g_DisplaySurface->hasReceivedQuitSignal() ) {
            break;
        }

        frameTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );

        framerateCounter.onFrame( frameTime );

        // Update Input
        g_InputReader->onFrame( g_InputMapper );

        accumulator += static_cast< f64 >( frameTime );

        while ( accumulator >= LOGIC_DELTA ) {
            // Update Local Game Instance
            g_InputMapper->update( LOGIC_DELTA );

#if DUSK_USE_IMGUI
            g_ImGuiManager->update( LOGIC_DELTA );
#endif
            g_World->update( LOGIC_DELTA );

            // Game Logic
            g_FreeCamera->update( LOGIC_DELTA );

            g_DynamicsWorld->tick( LOGIC_DELTA );

            accumulator -= LOGIC_DELTA;
        }

        g_InputMapper->clear();

        // Convert screenspace cursor position to viewport space.
		i32 shiftedMouseX = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.x - g_ViewportWindowPosition.x ), 0, vp.Width );
		i32 shiftedMouseY = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.y - g_ViewportWindowPosition.y ), 0, vp.Height );

        dkVec2u shiftedMouse = dkVec2u( shiftedMouseX, shiftedMouseY );

        // Wait for previous frame completion
        FrameGraph& frameGraph = g_WorldRenderer->prepareFrameGraph( vp, sr, &g_FreeCamera->getData() );
        frameGraph.acquireCurrentMaterialEdData( g_MaterialEditor->getRuntimeEditionData() );
        frameGraph.setScreenSize( ScreenSize );
        frameGraph.updateMouseCoordinates( shiftedMouse );
        
        // TODO We should use a snapshot of the world instead of having to wait the previous frame completion...
		g_World->collectRenderables( g_DrawCommandBuilder );

		g_DrawCommandBuilder->addWorldCameraToRender( &g_FreeCamera->getData() );
		g_DrawCommandBuilder->prepareAndDispatchCommands( g_WorldRenderer );

#if DUSK_USE_IMGUI
        // TODO Move all this crap to a dedicated subsystem!
            static bool IsRenderDocVisible = false;

            g_ImGuiRenderModule->lockForRendering();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            f32 menuBarHeight = 0.0f;
            if ( ImGui::BeginMainMenuBar() ) {
                menuBarHeight = ImGui::GetWindowSize().y;

                if ( ImGui::BeginMenu( "File" ) ) {
                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Edit" ) ) {
                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Debug" ) ) {
                    bool* DrawDebugSphere = EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "DisplayBoundingSphere" ) );
                
                    ImGui::Checkbox( "Display Geometry BoundingSphere", DrawDebugSphere );

                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Graphics" ) ) {
                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Window" ) ) {
                    if ( ImGui::MenuItem( "RenderDoc", nullptr, nullptr, g_RenderDocHelper->isAvailable() ) ) {
                        IsRenderDocVisible = true;
                    }

                    if ( ImGui::MenuItem( "Material Editor" ) ) {
                        g_MaterialEditor->openEditorWindow();
                    }

                    if ( ImGui::MenuItem( "Inspector" ) ) {
                        g_EntityEditor->openEditorWindow();
                    }

                    ImGui::EndMenu();
                }

            }
            ImGui::EndMainMenuBar();

            static bool active = false;
            ImGui::SetNextWindowSize( ImVec2( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) - menuBarHeight ) );
            ImGui::SetNextWindowPos( ImVec2( 0, menuBarHeight ) );
            ImGui::Begin( "Master Window", &active, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing );

            static bool NeedDockSetup = g_IsFirstLaunch;
            ImGuiIO& io = ImGui::GetIO();
            ImGuiID dockspaceID = ImGui::GetID( "MyDockspace" );

            if ( NeedDockSetup ) {
                NeedDockSetup = false;

                ImGuiContext* ctx = ImGui::GetCurrentContext();
                ImGui::DockBuilderRemoveNode( dockspaceID );
                ImGui::DockBuilderAddNode( dockspaceID );

                ImGuiID dock_main_id = dockspaceID;
                ImGuiID dock_id_prop = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id );
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode( dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id );

                ImGui::DockBuilderDockWindow( "Viewport", dock_main_id );
                ImGui::DockBuilderDockWindow( "Console", dock_id_bottom );
                ImGui::DockBuilderDockWindow( "Inspector", dock_id_prop );
                ImGui::DockBuilderDockWindow( "RenderDoc", dock_id_prop );
                ImGui::DockBuilderFinish( dockspaceID );
            }

            ImGui::DockSpace( dockspaceID );

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            if ( IsRenderDocVisible ) {
                ImGui::Begin( "RenderDoc", &IsRenderDocVisible );

                static bool g_AwaitingFrameCapture = false;
                static i32 g_FrameToCaptureCount = 1u;

                if ( g_AwaitingFrameCapture ) {
                    g_AwaitingFrameCapture = !g_RenderDocHelper->openLatestCapture();
                }

                ImGui::SetNextItemWidth( 60.0f );
                ImGui::DragInt( "Frame Count", &g_FrameToCaptureCount, 1.0f, 1, 60 );

                bool refAllResources = g_RenderDocHelper->isReferencingAllResources();
                if ( ImGui::Checkbox( "Reference All Resources", &refAllResources ) ) {
                    g_RenderDocHelper->referenceAllResources( refAllResources );
                }

                bool captureAllCmdLists = g_RenderDocHelper->isCapturingAllCmdLists();
                if ( ImGui::Checkbox( "Capture All Command Lists", &captureAllCmdLists ) ) {
                    g_RenderDocHelper->captureAllCmdLists( captureAllCmdLists );
                }

                if ( ImGui::Button( "Capture" ) ) {
                    g_RenderDocHelper->triggerCapture( g_FrameToCaptureCount );
                }

                ImGui::SameLine();

                if ( ImGui::Button( "Capture & Analyze" ) ) {
                    g_RenderDocHelper->triggerCapture( g_FrameToCaptureCount );

                    g_AwaitingFrameCapture = true;
                }

                ImGui::End();
            }

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            dk::editor::DisplayLoggingConsole();

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            g_MaterialEditor->displayEditorWindow();

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            if ( ImGui::Begin( "Time Of Day" ) ) {
                if ( ImGui::TreeNode( "Atmosphere" ) ) {
                    AtmosphereRenderModule* atmosphereRenderModule = g_WorldRenderer->getAtmosphereRenderingModule();
                    LightGrid* lightGrid = g_WorldRenderer->getLightGrid();
                    DirectionalLightGPU* sunLight = lightGrid->getDirectionalLightData();

                    if ( ImGui::Button( "Recompute LUTs" ) ) {
                        atmosphereRenderModule->triggerLutRecompute();
                    }

                    static bool overrideTod = true;
                    ImGui::Checkbox( "Override ToD", &overrideTod );
                    if ( overrideTod ) {
                        ImGui::Text( "Sun Settings" );

                        if ( ImGui::SliderFloat2( "Sun Pos", ( float* )&sunLight->SphericalCoordinates, -1.0f, 1.0f ) ) {
                            sunLight->NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( sunLight->SphericalCoordinates[0], sunLight->SphericalCoordinates[1] );
                            atmosphereRenderModule->setSunAngles( sunLight->SphericalCoordinates[0], sunLight->SphericalCoordinates[1] );
                        }

                        ImGui::InputFloat( "Angular Radius", &sunLight->AngularRadius );

                        // Cone solid angle formula = 2PI(1-cos(0.5*AD*(PI/180)))
                        const f32 solidAngle = ( 2.0f * dk::maths::PI<f32>() ) * ( 1.0f - cos( sunLight->AngularRadius ) );

                        f32 illuminance = sunLight->IlluminanceInLux / solidAngle;
                        if ( ImGui::InputFloat( "Illuminance (lux)", &illuminance ) ) {
                            sunLight->IlluminanceInLux = illuminance * solidAngle;
                        }

                        ImGui::ColorEdit3( "Color", ( float* )&sunLight->ColorLinearSpace );
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::End();

            // TODO Crappy stuff to prototype/test quickly.
            static ImVec2 viewportWinSize;
            static ImVec2 viewportWinPos;

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            if ( ImGui::Begin( "Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar ) ) {
                static ImVec2 prevWinSize = ImGui::GetWindowSize();
                
                ImVec2 winSize = ImGui::GetWindowSize();

                viewportWinSize = ImGui::GetWindowSize();
                viewportWinPos = ImGui::GetWindowPos();

				g_ViewportWindowPosition.x = static_cast< u32 >( viewportWinPos.x );
				g_ViewportWindowPosition.y = static_cast< u32 >( viewportWinPos.y );

                if ( !g_IsResizing && ( winSize.x != prevWinSize.x || winSize.y != prevWinSize.y ) ) {
                    g_IsResizing = true;
                } else if ( g_IsResizing ) {
                    f32 deltaX = ( winSize.x - prevWinSize.x );
                    f32 deltaY = ( winSize.y - prevWinSize.y );

                    // Wait until the drag is over to resize stuff internally.
                    if ( deltaX == 0.0f && deltaY == 0.0f ) {
                        viewportSize.x = winSize.x;
                        viewportSize.y = winSize.y;
                        g_FreeCamera->setProjectionMatrix( DefaultCameraFov, viewportSize.x, viewportSize.y );

                        vp.Width = static_cast< i32 >( viewportSize.x );
                        vp.Height = static_cast< i32 >( viewportSize.y );

                        sr.Right = static_cast< i32 >( viewportSize.x );
                        sr.Bottom = static_cast< i32 >( viewportSize.y );

                        g_IsResizing = false;
                    }
                }
                prevWinSize = winSize;

                g_IsMouseOverViewportWindow = ImGui::IsWindowHovered();

                winSize.x -= 4;
                winSize.y -= 4;

                ImGui::Image( static_cast< ImTextureID >( frameGraph.getPresentRenderTarget() ), winSize );
				if ( g_RightClickMenuOpened = ImGui::BeginPopupContextWindow( "Viewport Popup", 1 ) ) {
					if ( ImGui::BeginMenu( "New Entity..." ) ) {
						if ( ImGui::MenuItem( "Static Mesh" ) ) {
                            g_PickedEntity = g_World->createStaticMesh();

                            CameraData& cameraData = g_FreeCamera->getData();

                            const dkMat4x4f& projMat = cameraData.finiteProjectionMatrix;
                            const dkMat4x4f& viewMat = cameraData.viewMatrix;

                            dkMat4x4f inverseViewProj = ( viewMat * projMat ).inverse();

                            dkVec3f ray = {
                                ( ( ( 2.0f * shiftedMouseX ) / viewportWinSize.x ) - 1 ) / inverseViewProj[0][0],
                                -( ( ( 2.0f * shiftedMouseY ) / viewportWinSize.y ) - 1 ) / inverseViewProj[1][1],
                                1.0f
                            };

                            //dkVec3f rayOrigin =
                            //{
                            //    inverseViewProj[0][3],
                            //    inverseViewProj[1][3],
                            //    inverseViewProj[2][3]
                            //};


                            dkVec3f rayDirection =
                            {
                                ray.x * inverseViewProj[0][0] + ray.y * inverseViewProj[1][0] + ray.z * inverseViewProj[2][0],
                                ray.x * inverseViewProj[0][1] + ray.y * inverseViewProj[1][1] + ray.z * inverseViewProj[2][1],
                                ray.x * inverseViewProj[0][2] + ray.y * inverseViewProj[1][2] + ray.z * inverseViewProj[2][2]
                            };

                            f32 w = ray.x * inverseViewProj[0][3] + ray.y * inverseViewProj[1][3] + ray.z * inverseViewProj[2][3];

                            rayDirection *= ( 1.0f / w );
                            
                            f32 backup = rayDirection.y;
                            rayDirection.y = rayDirection.z;
                            rayDirection.z = backup;

                            dkVec3f entityPosition = cameraData.worldPosition + ( g_FreeCamera->getEyeDirection() * 10.0f + rayDirection );
                            TransformDatabase* transformDatabase = g_World->getTransformDatabase();
                            
                            TransformDatabase::EdInstanceData instanceData = transformDatabase->getEditorInstanceData( transformDatabase->lookup( g_PickedEntity ) );
                            instanceData.Position->x = entityPosition.x;
                            instanceData.Position->y = entityPosition.y;
                            instanceData.Position->z = entityPosition.z;
						}
						
                        ImGui::EndMenu();
					}

					if ( g_PickedEntity.getIdentifier() != Entity::INVALID_ID ) {
					    if ( ImGui::MenuItem( "Delete" ) ) {
							g_World->releaseEntity( g_PickedEntity );
							g_PickedEntity.setIdentifier( Entity::INVALID_ID );
						}
                    }

					ImGui::EndPopup();
				}
            }
            ImGui::End();
            
            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            dkVec4f viewportWidgetBounds = dkVec4f( viewportWinPos.x, viewportWinPos.y, viewportWinSize.x, viewportWinSize.y );
            g_EntityEditor->displayEditorWindow( g_FreeCamera->getData(), viewportWidgetBounds );

            // "Master Window" End
            ImGui::End();

            ImGui::Render();

            g_ImGuiRenderModule->unlock();
#endif
        // Rendering

        // TEST TEST TEST TEST
        std::string str = std::to_string( framerateCounter.AvgDeltaTime ).substr( 0, 6 )
            + " ms / "
            + std::to_string( framerateCounter.MinDeltaTime ).substr( 0, 6 )
            + " ms / "
            + std::to_string( framerateCounter.MaxDeltaTime ).substr( 0, 6 ) + " ms ("
            + std::to_string( framerateCounter.AvgFramePerSecond ).substr( 0, 6 ) + " FPS)";

        g_HUDRenderer->drawText( str.c_str(), 0.4f, 8.0f, 8.0f, dkVec4f( 1, 1, 1, 1 ), 0.5f );

        dkVec3f UpVector = g_FreeCamera->getUpVector();
        dkVec3f RightVector = g_FreeCamera->getRightVector();
        dkVec3f FwdVector = g_FreeCamera->getForwardVector();
        
        dkVec3f GuizmoLinesOrigin = dkVec3f( 40.0f, viewportSize.y - 32.0f, 0 );

        g_HUDRenderer->drawLine( GuizmoLinesOrigin, dkVec4f( 1, 0, 0, 1 ), GuizmoLinesOrigin - RightVector * 32.0f, dkVec4f( 1, 0, 0, 1 ), 2.0f );
        g_HUDRenderer->drawLine( GuizmoLinesOrigin, dkVec4f( 0, 1, 0, 1 ), GuizmoLinesOrigin - UpVector * 32.0f, dkVec4f( 0, 1, 0, 1 ), 2.0f );
        g_HUDRenderer->drawLine( GuizmoLinesOrigin, dkVec4f( 0, 0, 1, 1 ), GuizmoLinesOrigin - FwdVector * 32.0f, dkVec4f( 0, 0, 1, 1 ), 2.0f );

        Material::RenderScenario scenario = ( g_MaterialEditor->isUsingInteractiveMode() ) ? Material::RenderScenario::Default_Editor : Material::RenderScenario::Default;

        if ( g_RequestPickingUpdate ) {
            scenario = ( scenario == Material::RenderScenario::Default ) ? Material::RenderScenario::Default_Picking : Material::RenderScenario::Default_Picking_Editor;
            g_RequestPickingUpdate = false;
        }

        if ( g_WorldRenderer->WorldRendering->isPickingResultAvailable() ) {
            g_PickedEntity.setIdentifier( g_WorldRenderer->WorldRendering->getAndConsumePickedEntityId() );
        }

        // Build this frame FrameGraph
        BuildThisFrameGraph( frameGraph, scenario, viewportSize );
        
        g_WorldRenderer->drawWorld( g_RenderDevice, frameTime );
    }
}

void Shutdown()
{
    DUSK_LOG_RAW( "\n================================\nShutdown has started!\n================================\n\n" );

    // Forces buffer swapping in order to safely unload/destroy resources in use
    g_RenderDevice->waitForPendingFrameCompletion();

	DUSK_LOG_INFO( "Releasing GPU resources...\n" );

    g_WorldRenderer->destroy( g_RenderDevice );

    DUSK_LOG_INFO( "Calling subsystems destructors...\n" );

	dk::core::free( g_GlobalAllocator, g_EditorGridModule );

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    dk::core::free( g_GlobalAllocator, g_RenderDocHelper );
#endif

#if DUSK_USE_IMGUI
    g_ImGuiRenderModule->destroy( *g_RenderDevice );

    dk::core::free( g_GlobalAllocator, g_ImGuiManager );
    dk::core::free( g_GlobalAllocator, g_ImGuiRenderModule );
#endif

    dk::core::free( g_GlobalAllocator, g_EdAssetsFileSystem );
    dk::core::free( g_GlobalAllocator, g_RendererFileSystem );
#endif
    
    dk::core::free( g_GlobalAllocator, g_WorldRenderer );
    dk::core::free( g_GlobalAllocator, g_GraphicsAssetCache );
    dk::core::free( g_GlobalAllocator, g_ShaderCache );
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

void dk::editor::Start( const char* cmdLineArgs )
{
    Initialize( cmdLineArgs );
    MainLoop();
    Shutdown();
}