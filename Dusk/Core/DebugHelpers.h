/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "Logger.h"

// Should be called whenever the application can't recover from a fatal error.
[[noreturn]] void FatalError( const char* description, ... );

// Return true if a debugger is currently attached to this instance; false otherwise.
bool IsDebuggerAttached();

// Raise a fatal error whenever the condition provided is false (see FatalError above). This should be kept as macro to allow
// smooth function call stripping.
#define DUSK_RAISE_FATAL_ERROR( condition, format, ... ) ( condition ) ? (void)0 : FatalError( format "\n", ##__VA_ARGS__ )

#if DUSK_DEVBUILD
// If true, ignore any assertion failure in dev/release builds.
static bool g_SkipAssertions = false;

// If true, trigger a debugger break whenever an assertion fails. Lead to an early app termination if no debugger is attached.
static bool g_BreakOnAssertionFailure = true;

#if DUSK_MSVC
#define DUSK_TRIGGER_BREAKPOINT __debugbreak();
#elif DUSK_GCC
#include <signal.h>
#define DUSK_TRIGGER_BREAKPOINT raise(SIGTRAP);
#else 
#define DUSK_TRIGGER_BREAKPOINT
#endif

// Evaluate a given assertion. If the assertion fails, an error message will be logged and a software breakpoint might
// be raised (see g_BreakOnAssertionFailure). Assertions can be disabled at compile time/runtime by toggling g_SkipAssertions
// (see above).
#define DUSK_DEV_ASSERT( condition, format, ... ) if ( !g_SkipAssertions ) DUSK_ASSERT( condition, format, ##__VA_ARGS__ )

void DumpStackBacktrace();

#else
#define DUSK_TRIGGER_BREAKPOINT
#define DUSK_DEV_ASSERT( condition, format, ... )
#endif

#define DUSK_ASSERT( condition, format, ... )\
if ( !( condition ) ) {\
    if ( g_BreakOnAssertionFailure ) {\
        DUSK_TRIGGER_BREAKPOINT;\
    }\
    DUSK_LOG_ERROR( format "\n", ##__VA_ARGS__ );\
}
