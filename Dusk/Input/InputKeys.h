/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace input
    {
#define InputKey( option )\
        option( KEY_NONE )\
        option( KEY_A )\
        option( KEY_B )\
        option( KEY_C )\
        option( KEY_D )\
        option( KEY_E )\
        option( KEY_F )\
        option( KEY_G )\
        option( KEY_H )\
        option( KEY_I )\
        option( KEY_J )\
        option( KEY_K )\
        option( KEY_L )\
        option( KEY_M )\
        option( KEY_N )\
        option( KEY_O )\
        option( KEY_P )\
        option( KEY_Q )\
        option( KEY_R )\
        option( KEY_S )\
        option( KEY_T )\
        option( KEY_U )\
        option( KEY_V )\
        option( KEY_W )\
        option( KEY_X )\
        option( KEY_Y )\
        option( KEY_Z )\
        option( KEY_1 )\
        option( KEY_2 )\
        option( KEY_3 )\
        option( KEY_4 )\
        option( KEY_5 )\
        option( KEY_6 )\
        option( KEY_7 )\
        option( KEY_8 )\
        option( KEY_9 )\
        option( KEY_0 )\
        option( KEY_ENTER )\
        option( KEY_ESCAPE )\
        option( KEY_BACKSPACE )\
        option( KEY_TAB )\
        option( KEY_SPACE )\
        option( KEY_MINUS )\
        option( KEY_EQUALS )\
        option( KEY_LEFTBRACKET )\
        option( KEY_RIGHTBRACKET )\
        option( KEY_BACKSLASH )\
        option( KEY_SEMICOLON )\
        option( KEY_QUOTE )\
        option( KEY_GRAVE )\
        option( KEY_COMMA )\
        option( KEY_PERIOD )\
        option( KEY_SLASH )\
        option( KEY_CAPSLOCK )\
        option( KEY_F1 )\
        option( KEY_F2 )\
        option( KEY_F3 )\
        option( KEY_F4 )\
        option( KEY_F5 )\
        option( KEY_F6 )\
        option( KEY_F7 )\
        option( KEY_F8 )\
        option( KEY_F9 )\
        option( KEY_F10 )\
        option( KEY_F11 )\
        option( KEY_F12 )\
        option( KEY_PRINTSCREEN )\
        option( KEY_SCROLLLOCK )\
        option( KEY_PAUSE )\
        option( KEY_INSERT )\
        option( KEY_HOME )\
        option( KEY_PAGEUP )\
        option( KEY_DELETEFORWARD )\
        option( KEY_END )\
        option( KEY_PAGEDOWN )\
        option( KEY_RIGHT )\
        option( KEY_LEFT )\
        option( KEY_DOWN )\
        option( KEY_UP )\
        option( KEY_KEYPAD_NUMLOCK )\
        option( KEY_KEYPAD_DIVIDE )\
        option( KEY_KEYPAD_MULTIPLY )\
        option( KEY_KEYPAD_SUBTRACT )\
        option( KEY_KEYPAD_ADD )\
        option( KEY_KEYPAD_ENTER )\
        option( KEY_KEYPAD_1 )\
        option( KEY_KEYPAD_2 )\
        option( KEY_KEYPAD_3 )\
        option( KEY_KEYPAD_4 )\
        option( KEY_KEYPAD_5 )\
        option( KEY_KEYPAD_6 )\
        option( KEY_KEYPAD_7 )\
        option( KEY_KEYPAD_8 )\
        option( KEY_KEYPAD_9 )\
        option( KEY_KEYPAD_0 )\
        option( KEY_KEYPAD_POINT )\
        option( KEY_KEYPAD_SEPARATOR )\
        option( KEY_NONUSBACKSLASH )\
        option( KEY_KEYPAD_EQUALS )\
        option( KEY_F13 )\
        option( KEY_F14 )\
        option( KEY_F15 )\
        option( KEY_F16 )\
        option( KEY_F17 )\
        option( KEY_F18 )\
        option( KEY_F19 )\
        option( KEY_F20 )\
        option( KEY_F21 )\
        option( KEY_F22 )\
        option( KEY_F23 )\
        option( KEY_F24 )\
        option( KEY_HELP )\
        option( KEY_LEFTCONTROL )\
        option( KEY_LEFTSHIFT )\
        option( KEY_LEFTALT )\
        option( KEY_LEFTGUI )\
        option( KEY_RIGHTCONTROL )\
        option( KEY_RIGHTSHIFT )\
        option( KEY_RIGHTALT )\
        option( KEY_RIGHTGUI )\
        option( KEY_CONTROL )\
        option( KEY_ALT )\
        option( KEY_CLEAR )\
        option( KEY_EXCLAMATION )\
        option( KEY_DOUBLE_QUOTE )\
        option( KEY_TILDE )\
        option( KEY_KANA )\
        option( KEY_PLUS )\
        option( KEY_LSUPER )\
        option( KEY_RSUPER )\
        option( MOUSE_LEFT_BUTTON )\
        option( MOUSE_RIGHT_BUTTON )\
        option( MOUSE_MIDDLE_BUTTON )\
        option( MOUSE_EXTRA_1_BUTTON )\
        option( MOUSE_EXTRA_2_BUTTON )\
        option( JOYSTICK_BUTTON_0 )\
        option( JOYSTICK_BUTTON_1 )\
        option( JOYSTICK_BUTTON_2 )\
        option( JOYSTICK_BUTTON_3 )\
        option( JOYSTICK_BUTTON_4 )\
        option( JOYSTICK_BUTTON_5 )\
        option( JOYSTICK_BUTTON_6 )\
        option( JOYSTICK_BUTTON_7 )\
        option( JOYSTICK_BUTTON_8 )\
        option( JOYSTICK_BUTTON_9 )\
        option( JOYSTICK_BUTTON_10 )\
        option( JOYSTICK_BUTTON_11 )\
        option( JOYSTICK_BUTTON_12 )\
        option( JOYSTICK_BUTTON_13 )\
        option( JOYSTICK_BUTTON_14 )\
        option( JOYSTICK_BUTTON_15 )\
        option( JOYSTICK_BUTTON_16 )\
        option( JOYSTICK_BUTTON_17 )\
        option( JOYSTICK_BUTTON_18 )\
        option( JOYSTICK_BUTTON_19 )\
        option( JOYSTICK_BUTTON_20 )\
        option( JOYSTICK_BUTTON_21 )\
        option( JOYSTICK_BUTTON_22 )\
        option( JOYSTICK_BUTTON_23 )\
        option( JOYSTICK_BUTTON_24 )\
        option( JOYSTICK_BUTTON_25 )\
        option( JOYSTICK_BUTTON_26 )\
        option( JOYSTICK_BUTTON_27 )\
        option( JOYSTICK_BUTTON_28 )\
        option( JOYSTICK_BUTTON_29 )\
        option( JOYSTICK_BUTTON_30 )\
        option( JOYSTICK_BUTTON_31 )\
        option( JOYSTICK_BUTTON_32 )

        DUSK_LAZY_ENUM( InputKey )
#undef InputKey

        // Keyboard key count
        static constexpr int KEYBOARD_KEY_COUNT = 256;

        static constexpr eInputKey UnixKeys[KEYBOARD_KEY_COUNT] =
        {
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_SPACE,
            KEY_EXCLAMATION,
            KEY_DOUBLE_QUOTE,

            KEY_NONE, // numbersign
            KEY_NONE, // dollar
            KEY_NONE, // percent
            KEY_NONE, // ampersad
            KEY_NONE, // apostrophe

            KEY_QUOTE,

            KEY_NONE, // parentleft
            KEY_NONE, // parentright
            KEY_NONE, // asterik
            KEY_NONE, // plus

            KEY_COMMA,
            KEY_MINUS,
            KEY_PERIOD,
            KEY_SLASH,
            KEY_0,
            KEY_1,
            KEY_2,
            KEY_3,
            KEY_4,
            KEY_5,
            KEY_6,
            KEY_7,
            KEY_8,
            KEY_9,

            KEY_NONE, // colon

            KEY_SEMICOLON,

            KEY_NONE, // less

            KEY_EQUALS,

            KEY_NONE, // greater
            KEY_NONE, // question

            KEY_NONE, // at

            KEY_A,
            KEY_B,
            KEY_C,
            KEY_D,
            KEY_E,
            KEY_F,
            KEY_G,
            KEY_H,
            KEY_I,
            KEY_J,
            KEY_K,
            KEY_L,
            KEY_M,
            KEY_N,
            KEY_O,
            KEY_P,
            KEY_Q,
            KEY_R,
            KEY_S,
            KEY_T,
            KEY_U,
            KEY_V,
            KEY_W,
            KEY_X,
            KEY_Y,
            KEY_Z,

            KEY_LEFTBRACKET,
            KEY_BACKSLASH,
            KEY_RIGHTBRACKET,

            KEY_NONE,
            KEY_NONE,

            KEY_GRAVE,
            KEY_QUOTE,

            KEY_A,
            KEY_B,
            KEY_C,
            KEY_D,
            KEY_E,
            KEY_F,
            KEY_G,
            KEY_H,
            KEY_I,
            KEY_J,
            KEY_K,
            KEY_L,
            KEY_M,
            KEY_N,
            KEY_O,
            KEY_P,
            KEY_Q,
            KEY_R,
            KEY_S,
            KEY_T,
            KEY_U,
            KEY_V,
            KEY_W,
            KEY_X,
            KEY_Y,
            KEY_Z,
            // count: 123 keys

            // MISC_1 : from 0xff08 to 0xff1b
            KEY_BACKSPACE,
            KEY_TAB,

            KEY_NONE,

            KEY_CLEAR,

            KEY_NONE,

            KEY_ENTER,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_NONE,

            KEY_PAUSE,
            KEY_SCROLLLOCK,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_ESCAPE,
            // count: 20

            // MISC_2 : from 0xff50 to 0xff57
            KEY_HOME,
            KEY_LEFT,
            KEY_UP,
            KEY_RIGHT,
            KEY_DOWN,

            KEY_NONE,

            KEY_PAGEUP,

            KEY_NONE,

            KEY_PAGEDOWN,
            KEY_END,

            // count: 10
            // MISC_3 : from 0xff61 to 0xff6a
            KEY_PRINTSCREEN,

            KEY_NONE,

            KEY_INSERT,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_HELP,

            // count: 9
            // MISC_4 : from 0x0ffaa to 0xffbd
            KEY_KEYPAD_MULTIPLY,
            KEY_KEYPAD_ADD,
            KEY_KEYPAD_POINT,
            KEY_KEYPAD_SUBTRACT,
            KEY_KEYPAD_POINT,
            KEY_KEYPAD_DIVIDE,

            KEY_KEYPAD_0,
            KEY_KEYPAD_1,
            KEY_KEYPAD_2,
            KEY_KEYPAD_3,
            KEY_KEYPAD_4,
            KEY_KEYPAD_5,
            KEY_KEYPAD_6,
            KEY_KEYPAD_7,
            KEY_KEYPAD_8,
            KEY_KEYPAD_9,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_KEYPAD_EQUALS,

            // MISC_5: from 0xffbe to 0xffeb
            KEY_F1,
            KEY_F2,
            KEY_F2,
            KEY_F3,
            KEY_F4,
            KEY_F5,
            KEY_F6,
            KEY_F7,
            KEY_F8,
            KEY_F9,
            KEY_F10,
            KEY_F11,
            KEY_F12,
            KEY_F13,
            KEY_F14,
            KEY_F15,
            KEY_F16,
            KEY_F17,
            KEY_F18,
            KEY_F19,
            KEY_F20,
            KEY_F21,
            KEY_F22,
            KEY_F23,
            KEY_F24,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_LEFTSHIFT,
            KEY_RIGHTSHIFT,
            KEY_LEFTCONTROL,
            KEY_RIGHTCONTROL,
            KEY_CAPSLOCK,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_LEFTALT,
            KEY_RIGHTALT,
            KEY_LEFTGUI,
            KEY_RIGHTGUI,
        };

        static constexpr eInputKey WindowsKeys[KEYBOARD_KEY_COUNT] =
        {
            KEY_NONE,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_BACKSPACE, // VK_BACK
            KEY_TAB, // VK_TAB

            KEY_NONE,
            KEY_NONE,

            KEY_CLEAR,
            KEY_ENTER,

            KEY_NONE,
            KEY_NONE,

            KEY_LEFTSHIFT,
            KEY_CONTROL,
            KEY_ALT,
            KEY_PAUSE,
            KEY_CAPSLOCK,
            KEY_KANA,

            KEY_NONE,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_ESCAPE,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_SPACE,
            KEY_PAGEUP,
            KEY_PAGEDOWN,
            KEY_END,
            KEY_HOME,
            KEY_LEFT,
            KEY_UP,
            KEY_RIGHT,
            KEY_DOWN,

            KEY_NONE,

            KEY_PRINTSCREEN,

            KEY_NONE,

            KEY_PRINTSCREEN,
            KEY_INSERT,
            KEY_DELETEFORWARD,
            KEY_HELP, // 47 = 0x2F

            KEY_0,
            KEY_1,
            KEY_2,
            KEY_3,
            KEY_4,
            KEY_5,
            KEY_6,
            KEY_7,
            KEY_8,
            KEY_9,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_A,
            KEY_B,
            KEY_C,
            KEY_D,
            KEY_E,
            KEY_F,
            KEY_G,
            KEY_H,
            KEY_I,
            KEY_J,
            KEY_K,
            KEY_L,
            KEY_M,
            KEY_N,
            KEY_O,
            KEY_P,
            KEY_Q,
            KEY_R,
            KEY_S,
            KEY_T,
            KEY_U,
            KEY_V,
            KEY_W,
            KEY_X,
            KEY_Y,
            KEY_Z,

            KEY_LSUPER,
            KEY_RSUPER,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_KEYPAD_0,
            KEY_KEYPAD_1,
            KEY_KEYPAD_2,
            KEY_KEYPAD_3,
            KEY_KEYPAD_4,
            KEY_KEYPAD_5,
            KEY_KEYPAD_6,
            KEY_KEYPAD_7,
            KEY_KEYPAD_8,
            KEY_KEYPAD_9,
            KEY_KEYPAD_MULTIPLY,
            KEY_KEYPAD_ADD,
            KEY_KEYPAD_SEPARATOR,
            KEY_KEYPAD_SUBTRACT,
            KEY_KEYPAD_POINT,
            KEY_KEYPAD_DIVIDE,

            KEY_F1,
            KEY_F2,
            KEY_F2,
            KEY_F3,
            KEY_F4,
            KEY_F5,
            KEY_F6,
            KEY_F7,
            KEY_F8,
            KEY_F9,
            KEY_F10,
            KEY_F11,
            KEY_F12,
            KEY_F13,
            KEY_F14,
            KEY_F15,
            KEY_F16,
            KEY_F17,
            KEY_F18,
            KEY_F19,
            KEY_F20,
            KEY_F21,
            KEY_F22,
            KEY_F23,
            KEY_F24,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_KEYPAD_NUMLOCK,
            KEY_SCROLLLOCK,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_LEFTSHIFT,
            KEY_RIGHTSHIFT,
            KEY_LEFTCONTROL,
            KEY_RIGHTCONTROL,
            KEY_LEFTGUI,
            KEY_RIGHTGUI,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_SEMICOLON,
            KEY_PLUS,
            KEY_COMMA,
            KEY_MINUS,
            KEY_PERIOD,
            KEY_SLASH,
            KEY_TILDE,

            KEY_NONE,
            KEY_NONE,

            // Gamepad ( 0xC3 )
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            // 0xDA
            KEY_LEFTBRACKET,
            KEY_BACKSLASH,
            KEY_RIGHTBRACKET,
            KEY_DOUBLE_QUOTE,
            KEY_NONE,
            KEY_NONE,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,

            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
            KEY_NONE,
        };
    }
}
