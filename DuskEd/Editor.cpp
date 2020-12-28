/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Editor.h"

#include "DuskEngine.h"

#include "Framework/EditorInterface.h"

#include "Core/ImGuiManager.h"

#include "Framework/Transaction/TransactionHandler.h"
#include "Framework/Cameras/FreeCamera.h"
#include "Framework/MaterialEditor.h"
#include "Framework/EntityEditor.h"
#include "Framework/Entity.h"
#include "Framework/World.h"

#include "Graphics/RenderModules/ImGuiRenderModule.h"
#include "Graphics/RenderModules/EditorGridRenderModule.h"

#include "Input/InputMapper.h"
#include "Input/InputReader.h"

FreeCamera* g_FreeCamera;
MaterialEditor* g_MaterialEditor;
static EditorGridModule* g_EditorGridModule;
EntityEditor* g_EntityEditor;

#if DUSK_USE_IMGUI
static ImGuiManager* g_ImGuiManager;
static ImGuiRenderModule* g_ImGuiRenderModule;
static EditorInterface* g_EditorInterface;

#include "Framework/ImGuiUtilities.h"
#endif

TransactionHandler* g_TransactionHandler;

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

void UpdateFreeCamera( MappedInput& input, f32 deltaTime )
{
    if ( !g_CanMoveCamera ) {
        return;
    }

    // Camera Controls
    f64 axisX = input.Ranges[DUSK_STRING_HASH( "CameraMoveHorizontal" )];
    f64 axisY = input.Ranges[DUSK_STRING_HASH( "CameraMoveVertical" )];

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
            g_DuskEngine->getInputMapper()->popContext();
        } else {
            g_DuskEngine->getInputMapper()->pushContext( DUSK_STRING_HASH( "Editor" ) );
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
            g_DuskEngine->getLogicWorld()->releaseEntity( g_PickedEntity );
            g_PickedEntity.setIdentifier( Entity::INVALID_ID );
        }
    }

#if DUSK_USE_IMGUI
    InputReader* inputReader = g_DuskEngine->getInputReader();

    // Forward input states to ImGui
    dkVec2f ScreenSize = *EnvironmentVariables::getVariable<dkVec2f>( DUSK_STRING_HASH( "ScreenSize" ) );

    f32 rawX = dk::maths::clamp( static_cast< f32 >( inputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_X ) ), 0.0f, static_cast< f32 >( ScreenSize.x ) );
    f32 rawY = dk::maths::clamp( static_cast< f32 >( inputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_Y ) ), 0.0f, static_cast< f32 >( ScreenSize.y ) );
    f32 mouseWheel = static_cast< f32 >( inputReader->getAbsoluteAxisValue( dk::input::eInputAxis::MOUSE_SCROLL_WHEEL ) );

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

    fnKeyStrokes_t keyStrokes = inputReader->getAndFlushKeyStrokes();
    for ( dk::input::eInputKey key : keyStrokes ) {
        io.AddInputCharacter( key );
    }
#endif
}

void RegisterInputContexts()
{
    InputMapper* inputMapper = g_DuskEngine->getInputMapper();

    inputMapper->pushContext( DUSK_STRING_HASH( "Game" ) );
    inputMapper->pushContext( DUSK_STRING_HASH( "Editor" ) );

    // Free Camera
    inputMapper->addCallback( &UpdateFreeCamera, 0 );
    inputMapper->addCallback( &UpdateEditor, -1 );
}

#if DUSK_USE_IMGUI
void RegisterImguiSubsystem()
{
    BaseAllocator* globalAllocator = g_DuskEngine->getGlobalAllocator();
    RenderDevice* renderDevice = g_DuskEngine->getRenderDevice();
    GraphicsAssetCache* graphicsAssetCache = g_DuskEngine->getGraphicsAssetCache();

    //g_ImGuiManager = dk::core::allocate<ImGuiManager>( globalAllocator );
    //g_ImGuiManager->create( g_DuskEngine->getMainDisplaySurface(), g_DuskEngine->getVirtualFileSystem(), globalAllocator );
    //g_ImGuiManager->setVisible( true );

    //g_ImGuiRenderModule = dk::core::allocate<ImGuiRenderModule>( globalAllocator );
    //g_ImGuiRenderModule->loadCachedResources( *renderDevice, *graphicsAssetCache );

    g_EditorInterface = dk::core::allocate<EditorInterface>( globalAllocator, globalAllocator );
    g_EditorInterface->loadCachedResources( graphicsAssetCache );
}
#endif

void RegisterEditorSubsystems()
{
    GraphicsAssetCache* graphicsAssetCache = g_DuskEngine->getGraphicsAssetCache();
    LinearAllocator* globalAllocator = g_DuskEngine->getGlobalAllocator();
    VirtualFileSystem* virtualFileSystem = g_DuskEngine->getVirtualFileSystem();

    g_MaterialEditor = dk::core::allocate<MaterialEditor>( globalAllocator, globalAllocator, graphicsAssetCache, virtualFileSystem );

    g_EntityEditor = dk::core::allocate<EntityEditor>( globalAllocator, globalAllocator, graphicsAssetCache, virtualFileSystem, g_DuskEngine->getRenderWorld(), g_DuskEngine->getRenderDevice() );
    g_EntityEditor->setActiveWorld( g_DuskEngine->getLogicWorld() );
    g_EntityEditor->setActiveEntity( &g_PickedEntity );
    g_EntityEditor->openEditorWindow();

    g_EditorGridModule = dk::core::allocate<EditorGridModule>( globalAllocator );

    g_TransactionHandler = dk::core::allocate<TransactionHandler>( globalAllocator, globalAllocator );

    // TODO Retrieve pointer to camera instance from scene db
    f32 defaultCameraFov = *EnvironmentVariables::getVariable<f32>( DUSK_STRING_HASH( "DefaultCameraFov" ) );
    f32 imageQuality = *EnvironmentVariables::getVariable<f32>( DUSK_STRING_HASH( "ImageQuality" ) );
    u32 msaaSamplerCount = *EnvironmentVariables::getVariable<u32>( DUSK_STRING_HASH( "MSAASamplerCount" ) );

    g_FreeCamera = new FreeCamera();

    dkVec2f ScreenSize = *EnvironmentVariables::getVariable<dkVec2f>( DUSK_STRING_HASH( "ScreenSize" ) );
    g_FreeCamera->setProjectionMatrix( defaultCameraFov, static_cast< float >( ScreenSize.x ), static_cast< float >( ScreenSize.y ) );

    g_FreeCamera->setImageQuality( imageQuality );
    g_FreeCamera->setMSAASamplerCount( msaaSamplerCount );
}

void ShutdownEditor()
{
    LinearAllocator* globalAllocator = g_DuskEngine->getGlobalAllocator();
    RenderDevice* renderDevice = g_DuskEngine->getRenderDevice();

    dk::core::free( globalAllocator, g_TransactionHandler );
    dk::core::free( globalAllocator, g_EditorGridModule );
    dk::core::free( globalAllocator, g_EntityEditor );
    dk::core::free( globalAllocator, g_MaterialEditor );

#if DUSK_USE_IMGUI
    g_ImGuiRenderModule->destroy( *renderDevice );

    dk::core::free( globalAllocator, g_EditorInterface );
    dk::core::free( globalAllocator, g_ImGuiManager );
    dk::core::free( globalAllocator, g_ImGuiRenderModule );
#endif
}

void dk::editor::Start( const char* cmdLineArgs )
{
    g_DuskEngine->setApplicationName( DUSK_STRING( "DuskEd" ) );
    g_DuskEngine->create( cmdLineArgs );

    RegisterInputContexts();

    RegisterEditorSubsystems();

#if DUSK_USE_IMGUI
    RegisterImguiSubsystem();
#endif

    g_DuskEngine->mainLoop();

    ShutdownEditor();
    g_DuskEngine->shutdown();
}

//
//void BuildThisFrameGraph( FrameGraph& frameGraph, const Material::RenderScenario scenario, const dkVec2f& viewportSize )
//{
//    DUSK_CPU_PROFILE_FUNCTION;
//
//    // Append the regular render pipeline.
//    FGHandle presentRt = g_WorldRenderer->buildDefaultGraph( frameGraph, scenario, viewportSize, g_RenderWorld );
//
//    // Render editor grid.
//    presentRt = g_EditorGridModule->addEditorGridPass( frameGraph, presentRt, g_WorldRenderer->getResolvedDepth() );
//
//    // Render HUD.
//    presentRt = g_HUDRenderer->buildDefaultGraph( frameGraph, presentRt );
//
//#if DUSK_USE_IMGUI
//    // ImGui Editor GUI.
//    frameGraph.savePresentRenderTarget( presentRt );
//
//    presentRt = g_ImGuiRenderModule->render( frameGraph, presentRt );
//#endif
//
//    // Copy to swapchain buffer.
//    AddPresentRenderPass( frameGraph, presentRt );
//}
//
//void MainLoop()
//{
//  
//    // TEST TEST TEST
//    vp.X = 0;
//    vp.Y = 0;
//    vp.Width = ScreenSize.x;
//    vp.Height = ScreenSize.y;
//    vp.MinDepth = 0.0f;
//    vp.MaxDepth = 1.0f;
//
//    vpSr.Top = 0;
//    vpSr.Bottom = ScreenSize.y;
//    vpSr.Left = 0;
//    vpSr.Right = ScreenSize.x;
//
//    viewportSize = dkVec2f( static_cast< f32 >( ScreenSize.x ), static_cast< f32 >( ScreenSize.y ) );
//
//    Timer logicTimer;
//    logicTimer.start();
//
//    FramerateCounter framerateCounter;
//    f32 frameTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );
//    f64 accumulator = 0.0;
//
//    while ( 1 ) {
//        g_DisplaySurface->pollSystemEvents( g_InputReader );
//
//        if ( g_DisplaySurface->hasReceivedQuitSignal() ) {
//            break;
//        }
//
//        if ( g_DisplaySurface->hasReceivedResizeEvent() ) {
//            u32 surfaceWidth = g_DisplaySurface->getWidth();
//            u32 surfaceHeight = g_DisplaySurface->getHeight();
//
//            g_RenderDevice->resizeBackbuffer( surfaceWidth, surfaceHeight );
//
//#if DUSK_USE_IMGUI
//            g_ImGuiManager->resize( surfaceWidth, surfaceHeight );
//#endif
//
//            // Update editor camera aspect ratio.
//            g_FreeCamera->setProjectionMatrix( DefaultCameraFov, (f32)ScreenSize.x, ( f32 )ScreenSize.y );
//
//            // Clear the surface flag.
//            g_DisplaySurface->acknowledgeResizeEvent();
//        }
//
//        frameTime = static_cast< f32 >( logicTimer.getDeltaAsSeconds() );
//
//        framerateCounter.onFrame( frameTime );
//
//        accumulator += static_cast< f64 >( frameTime );
//
//        while ( accumulator >= LOGIC_DELTA ) {
//            DUSK_CPU_PROFILE_SCOPED( "Fixed Step Update" );
//
//            // Update Input
//            g_InputReader->onFrame( g_InputMapper );
//
//            // Update Local Game Instance
//            g_InputMapper->update( LOGIC_DELTA );
//            g_InputMapper->clear();
//
//#if DUSK_USE_IMGUI
//            g_ImGuiManager->update( LOGIC_DELTA );
//#endif
//
//            g_World->update( LOGIC_DELTA );
//
//            // Game Logic
//            g_FreeCamera->update( LOGIC_DELTA );
//
//            g_DynamicsWorld->tick( LOGIC_DELTA );
//
//            accumulator -= LOGIC_DELTA;
//        }
//
//        // Convert screenspace cursor position to viewport space.
//		shiftedMouseX = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.x - g_ViewportWindowPosition.x ), 0, vp.Width );
//		shiftedMouseY = dk::maths::clamp( static_cast< i32 >( g_CursorPosition.y - g_ViewportWindowPosition.y ), 0, vp.Height );
//
//        dkVec2u shiftedMouse = dkVec2u( shiftedMouseX, shiftedMouseY );
//
//        // Wait for previous frame completion
//        FrameGraph& frameGraph = g_WorldRenderer->prepareFrameGraph( vp, vpSr, &g_FreeCamera->getData() );
//        frameGraph.acquireCurrentMaterialEdData( g_MaterialEditor->getRuntimeEditionData() );
//        frameGraph.setScreenSize( ScreenSize );
//        frameGraph.updateMouseCoordinates( shiftedMouse );
//
//        g_GpuProfiler.update( *g_RenderDevice );
//
//        // TODO We should use a snapshot of the world instead of having to wait the previous frame completion...
//		g_World->collectRenderables( g_DrawCommandBuilder, g_WorldRenderer->getLightGrid() );
//
//        g_RenderWorld->update( g_RenderDevice );
//
//		g_DrawCommandBuilder->addWorldCameraToRender( &g_FreeCamera->getData() );
//		g_DrawCommandBuilder->prepareAndDispatchCommands( g_WorldRenderer );
//
//        // Rendering
//        // TEST TEST TEST TEST
//        if ( DisplayFramerate ) {
//            std::string str = std::to_string( framerateCounter.AvgDeltaTime ).substr( 0, 6 )
//                + " ms / "
//                + std::to_string( framerateCounter.MinDeltaTime ).substr( 0, 6 )
//                + " ms / "
//                + std::to_string( framerateCounter.MaxDeltaTime ).substr( 0, 6 ) + " ms ("
//                + std::to_string( framerateCounter.AvgFramePerSecond ).substr( 0, 6 ) + " FPS)";
//
//            g_HUDRenderer->drawText( str.c_str(), 0.4f, 8.0f, 8.0f, dkVec4f( 1, 1, 1, 1 ), 0.5f );
//        }
//
//        if ( DisplayCulledPrimCount ) {
//            std::string culledPrimitiveCount = "Culled Primitive(s): " + std::to_string( g_DrawCommandBuilder->getCulledGeometryPrimitiveCount() );
//            g_HUDRenderer->drawText( culledPrimitiveCount.c_str(), 0.4f, 8.0f, 24.0f, dkVec4f( 1, 1, 1, 1 ), 0.5f );
//        }
//
//        dkMat4x4f rotationMat = g_FreeCamera->getData().viewMatrix;
//
//        // Remove translation.
//		rotationMat[3].x = 0.0f;
//		rotationMat[3].y = 0.0f;
//		rotationMat[3].z = 0.0f;
//		rotationMat[3].w = 1.0f;
//
//		dkVec3f FwdVector = dkVec4f( 1, 0, 0, 0 ) * rotationMat;
//		dkVec3f UpVector = dkVec4f( 0, 1, 0, 0 ) * rotationMat;
//		dkVec3f RightVector = dkVec4f( 0, 0, 1, 0 ) * rotationMat;
//        
//        dkVec3f GuizmoLinesOrigin = dkVec3f( 40.0f, viewportSize.y - 32.0f, 0 );
//
//        g_HUDRenderer->drawLine( GuizmoLinesOrigin - RightVector * 32.0f, dkVec4f( 1, 0, 0, 1 ), GuizmoLinesOrigin, dkVec4f( 1, 0, 0, 1 ), 2.0f );
//        g_HUDRenderer->drawLine( GuizmoLinesOrigin - UpVector * 32.0f, dkVec4f( 0, 1, 0, 1 ), GuizmoLinesOrigin, dkVec4f( 0, 1, 0, 1 ), 2.0f );
//        g_HUDRenderer->drawLine( GuizmoLinesOrigin - FwdVector * 32.0f, dkVec4f( 0, 0, 1, 1 ), GuizmoLinesOrigin, dkVec4f( 0, 0, 1, 1 ), 2.0f );
//
//        Material::RenderScenario scenario = ( g_MaterialEditor->isUsingInteractiveMode() ) ? Material::RenderScenario::Default_Editor : Material::RenderScenario::Default;
//
//        if ( g_RequestPickingUpdate ) {
//            scenario = ( scenario == Material::RenderScenario::Default ) ? Material::RenderScenario::Default_Picking : Material::RenderScenario::Default_Picking_Editor;
//            g_RequestPickingUpdate = false;
//        }
//
//        if ( g_WorldRenderer->WorldRendering->isPickingResultAvailable() ) {
//            g_PickedEntity.setIdentifier( g_WorldRenderer->WorldRendering->getAndConsumePickedEntityId() );
//        }
//        // Build this frame FrameGraph
//        BuildThisFrameGraph( frameGraph, scenario, viewportSize );
//
//#if DUSK_USE_IMGUI
//		g_EditorInterface->display( frameGraph, g_ImGuiRenderModule );
//#endif
//
//        // TODO Should be renamed (this is simply a dispatch call for frame graph passes...).
//        g_WorldRenderer->drawWorld( g_RenderDevice, frameTime );
//    }
//}
