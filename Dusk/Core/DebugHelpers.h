/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Logger.h"

void Assert( const char* description, ... );
[[noreturn]] void FatalError( const char* description, ... );

#define DUSK_RAISE_FATAL_ERROR( condition, format, ... ) ( condition ) ? (void)0 : FatalError( format "\n", ##__VA_ARGS__ )

#if DUSK_DEVBUILD
static bool g_SkipAssertions = false;

#if DUSK_MSVC
#define DUSK_TRIGGER_BREAKPOINT __debugbreak();
#elif DUSK_GCC
#include <signal.h>
#define DUSK_TRIGGER_BREAKPOINT raise(SIGTRAP);
#endif
#define DUSK_DEV_ASSERT( condition, format, ... ) if ( !g_SkipAssertions ) DUSK_ASSERT( condition, format, ##__VA_ARGS__ )

void DumpStackBacktrace();

#else
#define DUSK_TRIGGER_BREAKPOINT
#define DUSK_DEV_ASSERT( condition, format, ... )
#endif

#define DUSK_ASSERT( condition, format, ... )\
if ( !( condition ) ) {\
    DUSK_TRIGGER_BREAKPOINT;\
    DUSK_LOG_ERROR( format "\n", ##__VA_ARGS__ );\
}
