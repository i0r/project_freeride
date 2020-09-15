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
#include "Graphics/RenderModules/CascadedShadowRenderModule.h"

#if DUSK_USE_RENDERDOC
#include "Graphics/RenderDocHelper.h"
#include "Framework/EditorWidgets/RenderDocHelper.h"
#endif

#if DUSK_USE_IMGUI
#include "Core/ImGuiManager.h"
#include "Graphics/RenderModules/ImGuiRenderModule.h"

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/ImGuizmo/ImGuizmo.h"

#include "Framework/ImGuiUtilities.h"
#include "Framework/EditorWidgets/LoggingConsole.h"

#include "ThirdParty/Google/IconsMaterialDesign.h"
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

#include "Framework/Transaction/TransactionHandler.h"
#include "Framework/EditorInterface.h"

#include "Core/StringHelpers.h"

static char  g_BaseBuffer[128];
static void* g_AllocatedTable;

static LinearAllocator* g_GlobalAllocator;
DisplaySurface* g_DisplaySurface;
static InputMapper* g_InputMapper;
static InputReader* g_InputReader;
RenderDevice* g_RenderDevice;
static VirtualFileSystem* g_VirtualFileSystem;
static FileSystemNative* g_SaveFileSystem;
static FileSystemNative* g_DataFileSystem;
static ShaderCache* g_ShaderCache;
static GraphicsAssetCache* g_GraphicsAssetCache;
WorldRenderer* g_WorldRenderer;
static HUDRenderer* g_HUDRenderer;
static RenderWorld* g_RenderWorld;
World* g_World;
static DrawCommandBuilder* g_DrawCommandBuilder;
EntityEditor* g_EntityEditor;

FreeCamera* g_FreeCamera;
MaterialEditor* g_MaterialEditor;
static EditorGridModule* g_EditorGridModule;

static FileSystemNative* g_EdAssetsFileSystem;
static FileSystemNative* g_RendererFileSystem;
static FileSystemArchive* g_GameFileSystem;

#if DUSK_USE_RENDERDOC
RenderDocHelper*                g_RenderDocHelper;
#endif

#if DUSK_USE_IMGUI
static ImGuiManager*            g_ImGuiManager;
static ImGuiRenderModule*       g_ImGuiRenderModule;
static EditorInterface*         g_EditorInterface;
#endif

static DynamicsWorld*          g_DynamicsWorld;

TransactionHandler*            g_TransactionHandler;

static bool                    g_IsGamePaused = false;
bool                    g_IsMouseOverViewportWindow = false;
static bool                    g_CanMoveCamera = false;
static bool                    g_WasRightButtonDown = false;
bool                    g_IsContextMenuOpened = false;
static Timer                   g_RightButtonTimer;
static bool                    g_RequestPickingUpdate = false;
bool                    g_RightClickMenuOpened = false;
static dkVec2u                 g_CursorPosition = dkVec2u( 0, 0 );
dkVec2u                 g_ViewportWindowPosition = dkVec2u( 0, 0 );
Entity                         g_PickedEntity = Entity();
dkVec2f viewportSize;
i32 shiftedMouseX;
i32 shiftedMouseY;
Viewport vp;
ScissorRegion vpSr;

DUSK_ENV_VAR( WindowMode, WINDOWED_MODE, eWindowMode ) // Defines application window mode [Windowed/Fullscreen/Borderless]
DUSK_ENV_VAR( EnableVSync, true, bool ); // "Enable Vertical Synchronisation [false/true]"
DUSK_ENV_VAR( ScreenSize, dkVec2u( 1280, 720 ), dkVec2u ); // "Defines application screen size [0..N]"
DUSK_ENV_VAR( MSAASamplerCount, 1, u32 ) // "MSAA sampler count [1..N]"
DUSK_ENV_VAR( ImageQuality, 1.0f, f32 ) // "Image Quality factor [0.1..N]"
DUSK_ENV_VAR( RefreshRate, -1, u32 ) // "Refresh rate. If -1, the application will find the highest refresh rate possible and use it"
DUSK_ENV_VAR( DefaultCameraFov, 90.0f, f32 ) // "Default Field Of View set to a new Camera"
DUSK_ENV_VAR_TRANSIENT( IsFirstLaunch, false, bool ) // "True if this is the first time the editor is launched" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseDebugLayer, false, bool ); // "Enable render debug context/layers" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseRenderDocCapture, false, bool );// "Use RenderDoc frame capture tool [false/true]"
                                                           // (note: renderdoc dynamic lib must be present in the working dir)
DUSK_DEV_VAR( DisplayCulledPrimCount, "Display the number of primitive culled for the Viewport[0]", false, bool );
DUSK_DEV_VAR( DisplayFramerate, "Display basic framerate infos", true, bool );

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

    if ( isRightButtonDown && !g_WasRightButtonDown ) {
        g_RightButtonTimer.reset();
    } else if ( !isRightButtonDown && g_WasRightButtonDown ) {
        const f64 elapsedTime = g_RightButtonTimer.getDeltaAsMiliseconds();
        if ( elapsedTime < 250.0 ) {
            g_IsContextMenuOpened = true;
        }
    }

    g_WasRightButtonDown = isRightButtonDown;
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
        if ( input.Actions.find( DUSK_STRING_HASH( "Undo" ) ) != input.Actions.end() ) {
            g_TransactionHandler->undo();
        }

        if ( input.Actions.find( DUSK_STRING_HASH( "Redo" ) ) != input.Actions.end() ) {
            g_TransactionHandler->redo();
        }
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
	if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/" ) ) ) {
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/materials/" ) );
		g_DataFileSystem->createFolder( DUSK_STRING( "./data/failed_shaders/" ) );
	}

    Logger::SetLogOutputFile( SaveFolder, DUSK_STRING( "DuskEd" ) );

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

        // We need to explicitly disable vendor extensions since those aren't
        // supported by RenderDoc.
        bool* disableVendorExtension = EnvironmentVariables::getVariable<bool>( DUSK_STRING_HASH( "DisableVendorExtensions" ) );
        *disableVendorExtension = true;
    }
#endif
#endif

    g_DisplaySurface = dk::core::allocate<DisplaySurface>( g_GlobalAllocator, g_GlobalAllocator );
    g_DisplaySurface->create( ScreenSize.x, ScreenSize.y, eDisplayMode::WINDOWED );
    g_DisplaySurface->setCaption( DUSK_STRING( "DuskEd" ) );

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

    if ( RefreshRate == -1 ) {
        // Update configuration file with the highest refresh rate possible.
        RefreshRate = g_RenderDevice->getActiveRefreshRate();

        DUSK_LOG_INFO( "RenderDevice returned default refresh rate: %uHz\n", RefreshRate );

        // Update Configuration file.
        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
        EnvironmentVariables::serialize( newEnvConfigurationFile );
        newEnvConfigurationFile->close();
    }

	g_ShaderCache = dk::core::allocate<ShaderCache>( g_GlobalAllocator, g_GlobalAllocator, g_RenderDevice, g_VirtualFileSystem );
	g_GraphicsAssetCache = dk::core::allocate<GraphicsAssetCache>( g_GlobalAllocator, g_GlobalAllocator, g_RenderDevice, g_ShaderCache, g_VirtualFileSystem );

	g_WorldRenderer = dk::core::allocate<WorldRenderer>( g_GlobalAllocator, g_GlobalAllocator );
	g_WorldRenderer->loadCachedResources( g_RenderDevice, g_ShaderCache, g_GraphicsAssetCache, g_VirtualFileSystem );

	g_HUDRenderer = dk::core::allocate<HUDRenderer>( g_GlobalAllocator, g_GlobalAllocator );
	g_HUDRenderer->loadCachedResources( *g_RenderDevice, *g_ShaderCache, *g_GraphicsAssetCache );

	g_RenderWorld = dk::core::allocate<RenderWorld>( g_GlobalAllocator, g_GlobalAllocator );
	g_RenderWorld->create( *g_RenderDevice );

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    // Wait until the RenderDevice has been created before doing the attachment
    if ( UseRenderDocCapture ) {
        g_RenderDocHelper->attachTo( *g_DisplaySurface, *g_RenderDevice );
    }
#endif
#endif

#if DUSK_USE_IMGUI
    g_ImGuiManager = dk::core::allocate<ImGuiManager>( g_GlobalAllocator );
	g_ImGuiManager->create( *g_DisplaySurface, g_VirtualFileSystem, g_GlobalAllocator );
	g_ImGuiManager->setVisible( true );

    g_ImGuiRenderModule = dk::core::allocate<ImGuiRenderModule>( g_GlobalAllocator );
    g_ImGuiRenderModule->loadCachedResources( *g_RenderDevice, *g_GraphicsAssetCache );

    g_EditorInterface = dk::core::allocate<EditorInterface>( g_GlobalAllocator, g_GlobalAllocator );
    g_EditorInterface->loadCachedResources( g_GraphicsAssetCache );
#endif

    g_DrawCommandBuilder = dk::core::allocate<DrawCommandBuilder>( g_GlobalAllocator, g_GlobalAllocator );

    g_GpuProfiler.create( *g_RenderDevice );

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
    g_TransactionHandler = dk::core::allocate<TransactionHandler>( g_GlobalAllocator, g_GlobalAllocator );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void BuildThisFrameGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize )
{
    // Append the regular render pipeline.
    FGHandle presentRt = g_WorldRenderer->buildDefaultGraph( frameGraph, scenario, viewportSize, g_RenderWorld );

    // Render editor grid.
    presentRt = g_EditorGridModule->addEditorGridPass( frameGraph, presentRt, g_WorldRenderer->getResolvedDepth() );

    // Render HUD.
    presentRt = g_HUDRenderer->buildDefaultGraph( frameGraph, presentRt );

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
    vp.X = 0;
    vp.Y = 0;
    vp.Width = ScreenSize.x;
    vp.Height = ScreenSize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    vpSr.Top = 0;
    vpSr.Bottom = ScreenSize.y;
    vpSr.Left = 0;
    vpSr.Right = ScreenSize.x;

    viewportSize = dkVec2f( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) );

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

        if ( g_DisplaySurface->hasReceivedResizeEvent() ) {
            u32 surfaceWidth = g_DisplaySurface->getWidth();
            u32 surfaceHeight = g_DisplaySurface->getHeight();

            g_RenderDevice->resizeBackbuffer( surfaceWidth, surfaceHeight );

#if DUSK_USE_IMGUI
            g_ImGuiManager->resize( surfaceWidth, surfaceHeight );
#endif

            // Update screen size (it will update the widget dimensions at the same time).
            ScreenSize.x = surfaceWidth;
            ScreenSize.y = surfaceHeight;

            // Update editor camera aspect ratio.
            g_FreeCamera->setProjectionMatrix( DefaultCameraFov, (f32)ScreenSize.x, ( f32 )ScreenSize.y );

            // Clear the surface flag.
            g_DisplaySurface->acknowledgeResizeEvent();
        }

        frameTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );

        framerateCounter.onFrame( frameTime );

        accumulator += static_cast< f64 >( frameTime );

        while ( accumulator >= LOGIC_DELTA ) {
            // Update Input
            g_InputReader->onFrame( g_InputMapper );

            // Update Local Game Instance
            g_InputMapper->update( LOGIC_DELTA );
            g_InputMapper->clear();

#if DUSK_USE_IMGUI
            g_ImGuiManager->update( LOGIC_DELTA );
#endif

            g_World->update( LOGIC_DELTA );

            // Game Logic
            g_FreeCamera->update( LOGIC_DELTA );

            g_DynamicsWorld->tick( LOGIC_DELTA );

            accumulator -= LOGIC_DELTA;
        }

        g_GpuProfiler.update( *g_RenderDevice );

        // Convert screenspace cursor position to viewport space.
		shiftedMouseX = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.x - g_ViewportWindowPosition.x ), 0, vp.Width );
		shiftedMouseY = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.y - g_ViewportWindowPosition.y ), 0, vp.Height );

        dkVec2u shiftedMouse = dkVec2u( shiftedMouseX, shiftedMouseY );

        // Wait for previous frame completion
        FrameGraph& frameGraph = g_WorldRenderer->prepareFrameGraph( vp, vpSr, &g_FreeCamera->getData() );
        frameGraph.acquireCurrentMaterialEdData( g_MaterialEditor->getRuntimeEditionData() );
        frameGraph.setScreenSize( ScreenSize );
        frameGraph.updateMouseCoordinates( shiftedMouse );
        
        // TODO We should use a snapshot of the world instead of having to wait the previous frame completion...
		g_World->collectRenderables( g_DrawCommandBuilder );

        g_RenderWorld->update( g_RenderDevice );

		g_DrawCommandBuilder->addWorldCameraToRender( &g_FreeCamera->getData() );
		g_DrawCommandBuilder->prepareAndDispatchCommands( g_WorldRenderer );

        // Rendering
        // TEST TEST TEST TEST
        if ( DisplayFramerate ) {
            std::string str = std::to_string( framerateCounter.AvgDeltaTime ).substr( 0, 6 )
                + " ms / "
                + std::to_string( framerateCounter.MinDeltaTime ).substr( 0, 6 )
                + " ms / "
                + std::to_string( framerateCounter.MaxDeltaTime ).substr( 0, 6 ) + " ms ("
                + std::to_string( framerateCounter.AvgFramePerSecond ).substr( 0, 6 ) + " FPS)";

            g_HUDRenderer->drawText( str.c_str(), 0.4f, 8.0f, 8.0f, dkVec4f( 1, 1, 1, 1 ), 0.5f );
        }

        if ( DisplayCulledPrimCount ) {
            std::string culledPrimitiveCount = "Culled Primitive(s): " + std::to_string( g_DrawCommandBuilder->getCulledGeometryPrimitiveCount() );
            g_HUDRenderer->drawText( culledPrimitiveCount.c_str(), 0.4f, 8.0f, 24.0f, dkVec4f( 1, 1, 1, 1 ), 0.5f );
        }

        dkMat4x4f rotationMat = g_FreeCamera->getData().viewMatrix;

        // Remove translation.
		rotationMat[3].x = 0.0f;
		rotationMat[3].y = 0.0f;
		rotationMat[3].z = 0.0f;
		rotationMat[3].w = 1.0f;

		dkVec3f FwdVector = dkVec4f( 1, 0, 0, 0 ) * rotationMat;
		dkVec3f UpVector = dkVec4f( 0, 1, 0, 0 ) * rotationMat;
		dkVec3f RightVector = dkVec4f( 0, 0, 1, 0 ) * rotationMat;
        
        dkVec3f GuizmoLinesOrigin = dkVec3f( 40.0f, viewportSize.y - 32.0f, 0 );

        g_HUDRenderer->drawLine( GuizmoLinesOrigin - RightVector * 32.0f, dkVec4f( 1, 0, 0, 1 ), GuizmoLinesOrigin, dkVec4f( 1, 0, 0, 1 ), 2.0f );
        g_HUDRenderer->drawLine( GuizmoLinesOrigin - UpVector * 32.0f, dkVec4f( 0, 1, 0, 1 ), GuizmoLinesOrigin, dkVec4f( 0, 1, 0, 1 ), 2.0f );
        g_HUDRenderer->drawLine( GuizmoLinesOrigin - FwdVector * 32.0f, dkVec4f( 0, 0, 1, 1 ), GuizmoLinesOrigin, dkVec4f( 0, 0, 1, 1 ), 2.0f );

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

#if DUSK_USE_IMGUI
		g_EditorInterface->display( frameGraph, g_ImGuiRenderModule );
#endif

        // TODO Should be renamed (this is simply a dispatch call for frame graph passes...).
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
    g_RenderWorld->destroy( *g_RenderDevice );

    DUSK_LOG_INFO( "Calling subsystems destructors...\n" );

	dk::core::free( g_GlobalAllocator, g_EditorGridModule );

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    dk::core::free( g_GlobalAllocator, g_RenderDocHelper );
#endif

#if DUSK_USE_IMGUI
    g_ImGuiRenderModule->destroy( *g_RenderDevice );

	dk::core::free( g_GlobalAllocator, g_EditorInterface );
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