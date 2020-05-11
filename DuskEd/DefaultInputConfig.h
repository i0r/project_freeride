/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Input/InputKeys.h>
#include <Input/InputLayouts.h>

#include <FileSystem/FileSystemObject.h>

namespace dk
{
    namespace core
    {
#define DUSK_DECLARE_DEFAULT_INPUT( cmd ) static constexpr dk::input::eInputKey cmd[dk::input::eInputLayout::InputLayout_COUNT] =
        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_MOVE_FORWARD )
        {
            dk::input::eInputKey::KEY_W,
            dk::input::eInputKey::KEY_Z,
            dk::input::eInputKey::KEY_W
        };

        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_MOVE_BACKWARD )
        {
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S
        };

        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_MOVE_LEFT )
        {
            dk::input::eInputKey::KEY_A,
            dk::input::eInputKey::KEY_Q,
            dk::input::eInputKey::KEY_A
        };

        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_MOVE_RIGHT )
        {
            dk::input::eInputKey::KEY_D,
            dk::input::eInputKey::KEY_D,
            dk::input::eInputKey::KEY_D
        };
        
        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_TAKE_ALTITUDE )
        {
            dk::input::eInputKey::KEY_E,
            dk::input::eInputKey::KEY_E,
            dk::input::eInputKey::KEY_E
        };
        
        DUSK_DECLARE_DEFAULT_INPUT( CAMERA_LOWER_ALTITUDE )
        {
            dk::input::eInputKey:: KEY_Q,
            dk::input::eInputKey::KEY_A,
            dk::input::eInputKey::KEY_Q
        };

        DUSK_DECLARE_DEFAULT_INPUT( LeftMouseButton )
        {
            dk::input::eInputKey::MOUSE_LEFT_BUTTON,
                dk::input::eInputKey::MOUSE_LEFT_BUTTON,
                dk::input::eInputKey::MOUSE_LEFT_BUTTON
        };

        DUSK_DECLARE_DEFAULT_INPUT( RightMouseButton )
        {
            dk::input::eInputKey::MOUSE_RIGHT_BUTTON,
                dk::input::eInputKey::MOUSE_RIGHT_BUTTON,
                dk::input::eInputKey::MOUSE_RIGHT_BUTTON
        };

        DUSK_DECLARE_DEFAULT_INPUT( MiddleMouseButton )
        {
            dk::input::eInputKey::MOUSE_MIDDLE_BUTTON,
                dk::input::eInputKey::MOUSE_MIDDLE_BUTTON,
                dk::input::eInputKey::MOUSE_MIDDLE_BUTTON
        };

        DUSK_DECLARE_DEFAULT_INPUT( PICK_OBJECT )
        {
            dk::input::eInputKey::MOUSE_LEFT_BUTTON,
            dk::input::eInputKey::MOUSE_LEFT_BUTTON,
            dk::input::eInputKey::MOUSE_LEFT_BUTTON
        };

        DUSK_DECLARE_DEFAULT_INPUT( MOVE_CAMERA )
        {
            dk::input::eInputKey::MOUSE_RIGHT_BUTTON,
            dk::input::eInputKey::MOUSE_RIGHT_BUTTON,
            dk::input::eInputKey::MOUSE_RIGHT_BUTTON
        };

        DUSK_DECLARE_DEFAULT_INPUT( KeyCtrl )
        {
            dk::input::eInputKey::KEY_CONTROL,
                dk::input::eInputKey::KEY_CONTROL,
                dk::input::eInputKey::KEY_CONTROL
        };

        DUSK_DECLARE_DEFAULT_INPUT( KeyShift )
        {
            dk::input::eInputKey::KEY_LEFTSHIFT,
                dk::input::eInputKey::KEY_LEFTSHIFT,
                dk::input::eInputKey::KEY_LEFTSHIFT
        };

        DUSK_DECLARE_DEFAULT_INPUT( KeyAlt )
        {
            dk::input::eInputKey::KEY_ALT,
                dk::input::eInputKey::KEY_ALT,
                dk::input::eInputKey::KEY_ALT
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_MOD1 )
        {
            dk::input::eInputKey::KEY_CONTROL,
            dk::input::eInputKey::KEY_CONTROL,
            dk::input::eInputKey::KEY_CONTROL
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_CopyNode )
        {
            dk::input::eInputKey::KEY_C,
            dk::input::eInputKey::KEY_C,
            dk::input::eInputKey::KEY_C
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_PasteNode )
        {
            dk::input::eInputKey::KEY_V,
            dk::input::eInputKey::KEY_V,
            dk::input::eInputKey::KEY_V
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_Undo )
        {
            dk::input::eInputKey::KEY_Z,
            dk::input::eInputKey::KEY_Z,
            dk::input::eInputKey::KEY_Z
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_Redo )
        {
            dk::input::eInputKey::KEY_Y,
            dk::input::eInputKey::KEY_Y,
            dk::input::eInputKey::KEY_Y
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_OpenDevMenu )
        {
            dk::input::eInputKey::KEY_TILDE,
            dk::input::eInputKey::KEY_TILDE,
            dk::input::eInputKey::KEY_TILDE
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_DeleteNode )
        {
            dk::input::eInputKey::KEY_DELETEFORWARD,
            dk::input::eInputKey::KEY_DELETEFORWARD,
            dk::input::eInputKey::KEY_DELETEFORWARD
        };

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_Save )
        {
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S
        }; 

        DUSK_DECLARE_DEFAULT_INPUT( DBGUI_Open )
        {
            dk::input::eInputKey::KEY_O,
            dk::input::eInputKey::KEY_O,
            dk::input::eInputKey::KEY_O
        };

        DUSK_DECLARE_DEFAULT_INPUT( MOVE_FORWARD )
        {
            dk::input::eInputKey::KEY_W,
            dk::input::eInputKey::KEY_Z,
            dk::input::eInputKey::KEY_W
        };

        DUSK_DECLARE_DEFAULT_INPUT( MOVE_BACKWARD )
        {
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S,
            dk::input::eInputKey::KEY_S
        };

        DUSK_DECLARE_DEFAULT_INPUT( MOVE_LEFT )
        {
            dk::input::eInputKey::KEY_A,
            dk::input::eInputKey::KEY_Q,
            dk::input::eInputKey::KEY_A
        };

        DUSK_DECLARE_DEFAULT_INPUT( MOVE_RIGHT )
        {
            dk::input::eInputKey::KEY_D,
            dk::input::eInputKey::KEY_D,
            dk::input::eInputKey::KEY_D
        };

        DUSK_DECLARE_DEFAULT_INPUT( SPRINT )
        {
            dk::input::eInputKey::KEY_LEFTSHIFT,
            dk::input::eInputKey::KEY_LEFTSHIFT,
            dk::input::eInputKey::KEY_LEFTSHIFT
        };
#undef DUSK_DECLARE_DEFAULT_INPUT

        static void WriteDefaultInputCfg( FileSystemObject* stream, const dk::input::eInputLayout inputLayout )
        {
            stream->writeString( "Context: \"Game\" {\n" );   
            stream->writeString( "\tMoveForward: { STATE, " + std::string( dk::input::InputKeyToString[MOVE_FORWARD[inputLayout]] ) + " }\n" );
            stream->writeString( "\tMoveLeft: { STATE, " + std::string( dk::input::InputKeyToString[MOVE_LEFT[inputLayout]] ) + " }\n" );
            stream->writeString( "\tMoveBackward: { STATE, " + std::string( dk::input::InputKeyToString[MOVE_BACKWARD[inputLayout]] ) + " }\n" );
            stream->writeString( "\tMoveRight: { STATE, " + std::string( dk::input::InputKeyToString[MOVE_RIGHT[inputLayout]] ) + " }\n" );
            stream->writeString( "\tSprint: { STATE, " + std::string( dk::input::InputKeyToString[SPRINT[inputLayout]] ) + " }\n" );

            stream->writeString( "\tHeadMoveHorizontal: { AXIS, MOUSE_X, 2000.0, -1000.0, 1000.0, -1.0, 1.0 }\n" );
            stream->writeString( "\tHeadMoveVertical: { AXIS, MOUSE_Y, 2000.0, -1000.0, 1000.0, -1.0, 1.0 }\n" );

            stream->writeString( "}\n" );
#if DUSK_DEVBUILD
            stream->writeString( "Context: \"Editor\" {\n" );
            stream->writeString( "\tOpenDevMenu: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_OpenDevMenu[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraMoveForward: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_MOVE_FORWARD[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraMoveLeft: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_MOVE_LEFT[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraMoveBackward: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_MOVE_BACKWARD[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraMoveRight: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_MOVE_RIGHT[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraLowerAltitude: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_LOWER_ALTITUDE[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCameraTakeAltitude: { STATE, " + std::string( dk::input::InputKeyToString[CAMERA_TAKE_ALTITUDE[inputLayout]] ) + " }\n\n" );

            stream->writeString( "\tCameraMoveHorizontal: { AXIS, MOUSE_X, 2000.0, -1000.0, 1000.0, -1.0, 1.0 }\n" );
            stream->writeString( "\tCameraMoveVertical: { AXIS, MOUSE_Y, 2000.0, -1000.0, 1000.0, -1.0, 1.0 }\n" );

            stream->writeString( "}\n" );
            stream->writeString( "Context: \"DebugUI\" {\n" );
            stream->writeString( "\tMouseClick: { STATE, " + std::string( dk::input::InputKeyToString[PICK_OBJECT[inputLayout]] ) + " }\n" );
            stream->writeString( "\tMoveCamera: { STATE, " + std::string( dk::input::InputKeyToString[MOVE_CAMERA[inputLayout]] ) + " }\n" );

            stream->writeString( "\tKeyCtrl: { STATE, " + std::string( dk::input::InputKeyToString[KeyCtrl[inputLayout]] ) + " }\n" );
            stream->writeString( "\tKeyShift: { STATE, " + std::string( dk::input::InputKeyToString[KeyShift[inputLayout]] ) + " }\n" );
            stream->writeString( "\tKeyAlt: { STATE, " + std::string( dk::input::InputKeyToString[KeyAlt[inputLayout]] ) + " }\n" );

            stream->writeString( "\tLeftMouseButton: { STATE, " + std::string( dk::input::InputKeyToString[LeftMouseButton[inputLayout]] ) + " }\n" );
            stream->writeString( "\tRightMouseButton: { STATE, " + std::string( dk::input::InputKeyToString[RightMouseButton[inputLayout]] ) + " }\n" );
            stream->writeString( "\tMiddleMouseButton: { STATE, " + std::string( dk::input::InputKeyToString[MiddleMouseButton[inputLayout]] ) + " }\n" );

            stream->writeString( "\tModifier1: { STATE, " + std::string( dk::input::InputKeyToString[DBGUI_MOD1[inputLayout]] ) + " }\n" );
            stream->writeString( "\tCopyNode: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_CopyNode[inputLayout]] ) + " }\n" );
            stream->writeString( "\tPasteNode: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_PasteNode[inputLayout]] ) + " }\n" );
            stream->writeString( "\tSaveScene: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_Save[inputLayout]] ) + " }\n" );
            stream->writeString( "\tOpenScene: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_Open[inputLayout]] ) + " }\n" );
            stream->writeString( "\tUndo: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_Undo[inputLayout]] ) + " }\n" );
            stream->writeString( "\tRedo: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_Redo[inputLayout]] ) + " }\n" );
            stream->writeString( "\tDeleteNode: { ACTION, " + std::string( dk::input::InputKeyToString[DBGUI_DeleteNode[inputLayout]] ) + " }\n" );
            stream->writeString( "}\n" );
#endif
        }
    }
}
