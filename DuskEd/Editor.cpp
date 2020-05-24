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

#include "Rendering/RenderDevice.h"

// TEMP FOR TEST CRAP; SHOULD BE REMOVED LATER
#include "Graphics/RenderModules/PresentRenderPass.h"
#include "Graphics/RenderModules/BrunetonSkyModel.h"
#include "Graphics/RenderModules/MSAAResolvePass.h"
#include "Graphics/RenderModules/FinalPostFxRenderPass.h"
#include "Graphics/RenderModules/AutomaticExposure.h"
#include "Graphics/RenderModules/BlurPyramid.h"
#include "Graphics/RenderModules/TextRenderingModule.h"
#include "Graphics/RenderModules/PrimitiveLightingTest.h"
#include "Graphics/RenderModules/GlareRenderModule.h"
#include "Graphics/RenderModules/FFTRenderPass.h"
// END TEMP

#include "Graphics/ShaderCache.h"
#include "Graphics/GraphicsAssetCache.h"
#include "Graphics/WorldRenderer.h"
#include "Graphics/LightGrid.h"
#include "Graphics/RenderWorld.h"

#if DUSK_USE_RENDERDOC
#include "Graphics/RenderDocHelper.h"
#endif

#if DUSK_USE_FBXSDK
#include "Parsing/FbxParser.h"
#endif

#if DUSK_USE_IMGUI
#include "Core/ImGuiManager.h"
#include "Graphics/RenderModules/ImGuiRenderModule.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"

#include "Framework/LoggingConsole.h"
#endif

#include "Input/InputMapper.h"
#include "Input/InputReader.h"
#include "Input/InputAxis.h"

#include "Maths/Matrix.h"
#include "Maths/MatrixTransformations.h"
#include "Maths/Vector.h"
#include "Maths/Helpers.h"

#include "Framework/Cameras/FreeCamera.h"
#include "DefaultInputConfig.h"

#include "Framework/MaterialEditor.h"

static char                 g_BaseBuffer[128];
static void*                g_AllocatedTable;

static LinearAllocator*     g_GlobalAllocator;
static DisplaySurface*      g_DisplaySurface;
static InputMapper*         g_InputMapper;
static InputReader*         g_InputReader;
static RenderDevice*        g_RenderDevice;
static VirtualFileSystem*   g_VirtualFileSystem;
static FileSystemNative*    g_SaveFileSystem;
static FileSystemNative*    g_DataFileSystem;
static ShaderCache*         g_ShaderCache;
static GraphicsAssetCache*  g_GraphicsAssetCache;
static WorldRenderer*       g_WorldRenderer;
static LightGrid*           g_LightGrid;
static RenderWorld*         g_RenderWorld;

static FreeCamera*          g_FreeCamera;
static MaterialEditor*      g_MaterialEditor;

#if DUSK_DEVBUILD
static FileSystemNative*    g_ShaderSourceFileSystem;
#if DUSK_USE_RENDERDOC
static RenderDocHelper*     g_RenderDocHelper;
#endif
#if DUSK_USE_IMGUI
static ImGuiManager*        g_ImGuiManager;
static ImGuiRenderModule*   g_ImGuiRenderModule;

#endif
#endif

static bool                    g_IsDevMenuVisible = true;
static bool                    g_IsGamePaused = false;
static bool                    g_IsFirstLaunch = false;
static bool                    g_IsMouseOverViewportWindow = false;
static bool                    g_CanMoveCamera = false;

DUSK_ENV_VAR( EnableVSync, true, bool ); // "Enable Vertical Synchronisation [false/true]"
DUSK_ENV_VAR( ScreenSize, dkVec2u( 1280, 720 ), dkVec2u ); // "Defines application screen size [0..N]"
DUSK_ENV_VAR( MSAASamplerCount, 1, u32 ) // "MSAA sampler count [1..N]"
DUSK_ENV_VAR( ImageQuality, 1.0f, f32 ) // "Image Quality factor [0.1..N]"
DUSK_ENV_VAR( DefaultCameraFov, 90.0f, f32 ) // "Default Field Of View set to a new Camera"
DUSK_DEV_VAR_PERSISTENT( UseDebugLayer, false, bool ); // "Enable render debug context/layers" [false/true]
DUSK_DEV_VAR_PERSISTENT( UseRenderDocCapture, false, bool );// "Use RenderDoc frame capture tool [false/true]"
                                                           // (note: renderdoc dynamic lib must be present in the working dir)

void RegisterInputContexts()
{
    g_InputMapper->pushContext( DUSK_STRING_HASH( "Editor" ) );
    g_InputMapper->pushContext( DUSK_STRING_HASH( "DebugUI" ) );

    // Free Camera
    g_InputMapper->addCallback( [&]( MappedInput & input, float frameTime ) {
        if ( g_IsDevMenuVisible && !g_CanMoveCamera ) {
            return;
        }

        // Camera Controls
        auto axisX = input.Ranges[DUSK_STRING_HASH( "CameraMoveHorizontal" )];
        auto axisY = input.Ranges[DUSK_STRING_HASH( "CameraMoveVertical" )];

        g_FreeCamera->updateMouse( frameTime, axisX, axisY );

        if ( input.States.find( DUSK_STRING_HASH( "CameraMoveRight" ) ) != input.States.end() ) {
            g_FreeCamera->moveRight( frameTime );
        }

        if ( input.States.find( DUSK_STRING_HASH( "CameraMoveLeft" ) ) != input.States.end() ) {
            g_FreeCamera->moveLeft( frameTime );
        }

        if ( input.States.find( DUSK_STRING_HASH( "CameraMoveForward" ) ) != input.States.end() ) {
            g_FreeCamera->moveForward( frameTime );
        }

        if ( input.States.find( DUSK_STRING_HASH( "CameraMoveBackward" ) ) != input.States.end() ) {
            g_FreeCamera->moveBackward( frameTime );
        }

        if ( input.States.find( DUSK_STRING_HASH( "CameraLowerAltitude" ) ) != input.States.end() ) {
            g_FreeCamera->lowerAltitude( frameTime );
        }

        if ( input.States.find( DUSK_STRING_HASH( "CameraTakeAltitude" ) ) != input.States.end() ) {
            g_FreeCamera->takeAltitude( frameTime );
        }
    }, 0 );

    // DebugUI
    g_InputMapper->addCallback( [&]( MappedInput & input, float frameTime ) {
        if ( input.Actions.find( DUSK_STRING_HASH( "OpenDevMenu" ) ) != input.Actions.end() ) {
            g_IsDevMenuVisible = !g_IsDevMenuVisible;

            g_ImGuiManager->setVisible( g_IsDevMenuVisible );

            if ( g_IsDevMenuVisible ) {
                g_InputMapper->pushContext( DUSK_STRING_HASH( "DebugUI" ) );
            } else {
                g_InputMapper->popContext();
            }
        }

        const bool previousCamState = g_CanMoveCamera;
        const bool isRightButtonDown = input.States.find( DUSK_STRING_HASH( "RightMouseButton" ) ) != input.States.end();
        g_CanMoveCamera = ( g_IsMouseOverViewportWindow && isRightButtonDown );

        if ( previousCamState != g_CanMoveCamera ) {
            if ( g_CanMoveCamera ) {
                g_InputMapper->popContext();
            } else {
                g_InputMapper->pushContext( DUSK_STRING_HASH( "DebugUI" ) );
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

        //if ( !io.WantCaptureMouse ) {
        //    if ( input.Actions.find( DUSK_STRING_HASH( "PickNode" ) ) != input.Actions.end() ) {
        //        CameraData& cameraData = g_FreeCamera->getData();
        //        const dkMat4x4f& viewMat = cameraData.viewMatrix;
        //        const dkMat4x4f& projMat = cameraData.depthProjectionMatrix;

        //        dkMat4x4f inverseViewProj = ( projMat * viewMat ).inverse();

        //        dkVec4f ray =
        //        {
        //            ( io.MousePos.x / io.DisplaySize.x ) * 2.f - 1.f,
        //            ( 1.f - ( io.MousePos.y / io.DisplaySize.y ) ) * 2.f - 1.f,
        //            0.0f,
        //            1.0f
        //        };

        //        dkVec4f rayOrigin =
        //        {
        //            ray.x * inverseViewProj[0][0] + ray.y * inverseViewProj[1][0] + ray.z * inverseViewProj[2][0] + ray.w * inverseViewProj[3][0],
        //            ray.x * inverseViewProj[0][1] + ray.y * inverseViewProj[1][1] + ray.z * inverseViewProj[2][1] + ray.w * inverseViewProj[3][1],
        //            ray.x * inverseViewProj[0][2] + ray.y * inverseViewProj[1][2] + ray.z * inverseViewProj[2][2] + ray.w * inverseViewProj[3][2],
        //            ray.x * inverseViewProj[0][3] + ray.y * inverseViewProj[1][3] + ray.z * inverseViewProj[2][3] + ray.w * inverseViewProj[3][3],
        //        };
        //        rayOrigin *= ( 1.0f / rayOrigin.w );

        //        ray.z = 1.0f;
        //        dkVec4f rayEnd =
        //        {
        //            ray.x * inverseViewProj[0][0] + ray.y * inverseViewProj[1][0] + ray.z * inverseViewProj[2][0] + ray.w * inverseViewProj[3][0],
        //            ray.x * inverseViewProj[0][1] + ray.y * inverseViewProj[1][1] + ray.z * inverseViewProj[2][1] + ray.w * inverseViewProj[3][1],
        //            ray.x * inverseViewProj[0][2] + ray.y * inverseViewProj[1][2] + ray.z * inverseViewProj[2][2] + ray.w * inverseViewProj[3][2],
        //            ray.x * inverseViewProj[0][3] + ray.y * inverseViewProj[1][3] + ray.z * inverseViewProj[2][3] + ray.w * inverseViewProj[3][3],
        //        };
        //        rayEnd *= ( 1.0f / rayEnd.w );

        //        dkVec3f rayDir = ( rayEnd - rayOrigin ).normalize();

        //        dkVec3f rayDirection = dkVec3f( rayDir );
        //        dkVec3f rayOrig = dkVec3f( rayOrigin );

        //        /*g_PickingRay.origin = rayOrig;
        //        g_PickingRay.direction = rayDirection;

        //        g_PickedNode = g_SceneTest->intersect( g_PickingRay );*/
        //    }
        //}

        // Forward input states to ImGui
        f32 rawX = dk::maths::clamp( static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_X ) ), 0.0f, static_cast< f32 >( ScreenSize.x ) );
        f32 rawY = dk::maths::clamp( static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_Y ) ), 0.0f, static_cast< f32 >( ScreenSize.y ) );
        f32 mouseWheel = static_cast< f32 >( g_InputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_SCROLL_WHEEL ) );

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
        io.MouseDown[1] = ( input.States.find( DUSK_STRING_HASH( "RightMouseButton" ) ) != input.States.end() );
        io.MouseDown[2] = ( input.States.find( DUSK_STRING_HASH( "MiddleMouseButton" ) ) != input.States.end() );

        fnKeyStrokes_t keyStrokes = g_InputReader->getAndFlushKeyStrokes();
        for ( dk::input::eInputKey key : keyStrokes ) {
            io.AddInputCharacter( key );
        }
    }, -1 );
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

#if DUSK_DEVBUILD
    DUSK_LOG_INFO( "Mounting devbuild filesystems...\n" );
    g_ShaderSourceFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./../dusk/Shaders/" ) );
    g_VirtualFileSystem->mount( g_ShaderSourceFileSystem, DUSK_STRING( "ShaderSources" ), 0 );
#endif

    dkString_t SaveFolder = saveFolder->resolveFilename( DUSK_STRING( "SaveData/" ), configurationFolderName );

    if ( !saveFolder->fileExists( SaveFolder ) ) {
        DUSK_LOG_INFO( "First run detected! Creating save folder at '%s')\n", SaveFolder.c_str() );

        saveFolder->createFolder( SaveFolder );
    }
    dk::core::free( g_GlobalAllocator, saveFolder );

    if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/cache/" ) ) ) {
        g_DataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
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
    g_RenderDevice->create( *g_DisplaySurface, UseDebugLayer );
    g_RenderDevice->enableVerticalSynchronisation( EnableVSync );

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

    g_LightGrid = dk::core::allocate<LightGrid>( g_GlobalAllocator, g_GlobalAllocator );

    g_RenderWorld = dk::core::allocate<RenderWorld>( g_GlobalAllocator, g_GlobalAllocator );

#if DUSK_USE_IMGUI
    g_ImGuiManager = dk::core::allocate<ImGuiManager>( g_GlobalAllocator );
    g_ImGuiManager->create( *g_DisplaySurface );

    g_ImGuiRenderModule = dk::core::allocate<ImGuiRenderModule>( g_GlobalAllocator );
    g_ImGuiRenderModule->loadCachedResources( *g_RenderDevice, *g_ShaderCache, *g_GraphicsAssetCache );
#endif
#endif

    // TODO Retrieve pointer to camera instance from scene db
    g_FreeCamera = new FreeCamera();
    g_FreeCamera->setProjectionMatrix( DefaultCameraFov, static_cast< float >( ScreenSize.x ), static_cast< float >( ScreenSize.y ) );

    g_FreeCamera->setImageQuality( ImageQuality );
    g_FreeCamera->setMSAASamplerCount( MSAASamplerCount ); 
    
    g_LightGrid->getDirectionalLightData()->NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( 0.5f, 0.5f );

    constexpr f32 kSunAngularRadius = 0.00935f / 2.0f;
    g_LightGrid->getDirectionalLightData()->AngularRadius = kSunAngularRadius;

    // Cone solid angle formula = 2PI(1-cos(0.5*AD*(PI/180)))
    const f32 solidAngle = ( 2.0f * dk::maths::PI<f32>() ) * ( 1.0f - cos( g_LightGrid->getDirectionalLightData()->AngularRadius ) );

    g_LightGrid->getDirectionalLightData()->IlluminanceInLux = 100000.0f * solidAngle;
    g_LightGrid->getDirectionalLightData()->ColorLinearSpace = dkVec3f( 1.0f, 1.0f, 1.0f );

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

    g_MaterialEditor = dk::core::allocate<MaterialEditor>( g_GlobalAllocator, g_GlobalAllocator );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void MainLoop()
{
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

    Timer logicTimer;
    logicTimer.start();

    FramerateCounter framerateCounter;
    f32 frameTime = static_cast<f32>( logicTimer.getDeltaAsSeconds() );
    f64 accumulator = 0.0;

    // TEST TEST TEST
    FbxParser fbxParser;
    fbxParser.create( g_GlobalAllocator );
    fbxParser.load( "./data/geometry/box.fbx" );

    Model* testModel = g_RenderWorld->addAndCommitParsedDynamicModel( g_RenderDevice, *fbxParser.getParsedModel() );
    dkMat4x4f* testModelInstance = g_RenderWorld->allocateModelInstance( testModel );
    // TEST TEST TEST

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

        // As ticks
        constexpr uint32_t LOGIC_TICKRATE = 300;
        constexpr uint32_t PHYSICS_TICKRATE = 100;

        // As milliseconds
        constexpr f64 LOGIC_DELTA = 1.0f / static_cast< float >( LOGIC_TICKRATE );
        constexpr f64 PHYSICS_DELTA = 1.0f / static_cast< float >( PHYSICS_TICKRATE );

        while ( accumulator >= LOGIC_DELTA ) {
            // Update Local Game Instance
            g_InputMapper->update( LOGIC_DELTA );

#if DUSK_USE_IMGUI
            if ( g_IsDevMenuVisible ) {
                g_ImGuiManager->update( LOGIC_DELTA );
            }
#endif

            // Game Logic
            g_FreeCamera->update( LOGIC_DELTA );

            accumulator -= LOGIC_DELTA;
        }

        g_InputMapper->clear();

        std::string str = std::to_string( framerateCounter.AvgDeltaTime ).substr( 0, 6 )
            + " ms / "
            + std::to_string( framerateCounter.MinDeltaTime ).substr( 0, 6 )
            + " ms / "
            + std::to_string( framerateCounter.MaxDeltaTime ).substr( 0, 6 ) + " ms ("
            + std::to_string( framerateCounter.AvgFramePerSecond ).substr( 0, 6 ) + " FPS)";

        // Wait for previous frame completion
        FrameGraph& frameGraph = g_WorldRenderer->prepareFrameGraph( vp, sr, &g_FreeCamera->getData() );

#if DUSK_USE_IMGUI
        if ( g_IsDevMenuVisible ) {
            static bool IsRenderDocVisible = false;

            g_ImGuiRenderModule->lockRenderList();
            ImGui::NewFrame();

            f32 menuBarHeight = 0.0f;
            if ( ImGui::BeginMainMenuBar() ) {
                menuBarHeight = ImGui::GetWindowSize().y;

                if ( ImGui::BeginMenu( "File" ) ) {
                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Edit" ) ) {
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

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            static bool active = false;
            ImGui::SetNextWindowSize( ImVec2( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) - menuBarHeight ) );
            ImGui::SetNextWindowPos( ImVec2( 0, menuBarHeight ) );
            ImGui::Begin( "Master Window", &active, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing );

            // static ImGuiID dockspaceID = 0;
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

            if ( IsRenderDocVisible && ImGui::Begin( "RenderDoc", &IsRenderDocVisible ) ) {
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
            ImGui::Begin( "Inspector" );
            static f32 cart[2] = { 0.5f, 0.5f };

            if ( ImGui::SliderFloat2( "Sun Pos", cart, -1.0f, 1.0f ) ) {
                g_LightGrid->getDirectionalLightData()->NormalizedDirection = dk::maths::SphericalToCarthesianCoordinates( cart[0], cart[1] );
                g_WorldRenderer->BrunetonSky->setSunSphericalPosition( cart[0], cart[1] );
            }
            ImGui::End();

            ImGui::SetNextWindowDockID( dockspaceID, ImGuiCond_FirstUseEver );
            ImGui::Begin( "Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar );
            ImVec2 winSize = ImGui::GetWindowSize();

            g_IsMouseOverViewportWindow = ImGui::IsWindowHovered();

            //g_FreeCamera->setProjectionMatrix( DefaultCameraFov, winSize.x, winSize.y );

            winSize.x -= 32;
            winSize.y -= 32;

            ImGui::Image( static_cast<ImTextureID>( frameGraph.getPresentRenderTarget() ), winSize );
            ImGui::End();
            ImGui::End();
            ImGui::Render();

            g_ImGuiRenderModule->unlockRenderList();
        }
#endif

        // Rendering
        g_WorldRenderer->TextRendering->addOutlinedText( str.c_str(), 0.4f, 8.0f, 8.0f, dkVec4f( 1, 1, 1, 1 ) );
        g_WorldRenderer->AutomaticExposure->importResourcesToGraph( frameGraph );
        
        LightGrid::Data lightGridData = g_LightGrid->updateClusters( frameGraph );
        
        auto primRenderPass = AddPrimitiveLightTest( frameGraph, testModel, lightGridData.PerSceneBuffer );
        ResHandle_t presentRt = g_WorldRenderer->BrunetonSky->renderSky( frameGraph, primRenderPass.OutputRenderTarget, primRenderPass.OutputDepthTarget );
        if ( MSAASamplerCount > 1 ) {
            presentRt = AddMSAAResolveRenderPass( frameGraph, presentRt, -1, -1, MSAASamplerCount, false );
        }
       
        // Glare Rendering.
        FFTPassOutput frequencyDomainRt = AddFFTComputePass( frameGraph, presentRt, static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) );
        FFTPassOutput convolutedFFT = g_WorldRenderer->GlareRendering->addGlareComputePass( frameGraph, frequencyDomainRt );
        ResHandle_t inverseFFT = AddInverseFFTComputePass( frameGraph, convolutedFFT, static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) );

        g_WorldRenderer->AutomaticExposure->computeExposure( frameGraph, presentRt, ScreenSize );
        presentRt = AddFinalPostFxRenderPass( frameGraph, presentRt, inverseFFT );
        presentRt = g_WorldRenderer->TextRendering->renderText( frameGraph, presentRt );

#if DUSK_USE_IMGUI
        if ( g_IsDevMenuVisible ) {
            frameGraph.copyAsPresentRenderTarget( presentRt );
            
            presentRt = g_ImGuiRenderModule->render( frameGraph, presentRt );
        }
#endif

        AddPresentRenderPass( frameGraph, presentRt );

        g_WorldRenderer->drawWorld( g_RenderDevice, frameTime );
    }
}

void Shutdown()
{
    DUSK_LOG_RAW( "\n================================\nShutdown has started!\n================================\n\n" );

    // Forces buffer swapping in order to safely unload/destroy resources in use
    g_RenderDevice->waitForPendingFrameCompletion();
    
    g_WorldRenderer->destroy( g_RenderDevice );

    DUSK_LOG_INFO( "Calling subsystems destructors...\n" );

#if DUSK_DEVBUILD
#if DUSK_USE_RENDERDOC
    dk::core::free( g_GlobalAllocator, g_RenderDocHelper );
#endif

#if DUSK_USE_IMGUI
    g_ImGuiRenderModule->destroy( *g_RenderDevice );

    dk::core::free( g_GlobalAllocator, g_ImGuiManager );
    dk::core::free( g_GlobalAllocator, g_ImGuiRenderModule );
#endif

    dk::core::free( g_GlobalAllocator, g_ShaderSourceFileSystem );
#endif

    dk::core::free( g_GlobalAllocator, g_WorldRenderer );
    dk::core::free( g_GlobalAllocator, g_GraphicsAssetCache );
    dk::core::free( g_GlobalAllocator, g_ShaderCache );
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
