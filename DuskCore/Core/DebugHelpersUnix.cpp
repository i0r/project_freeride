/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_UNIX
#include "DebugHelpers.h"

#include <stdarg.h>

#if DUSK_DEVBUILD
void DumpStackBacktrace()
{
    DUSK_LOG_INFO( "Dumping stack backtrace...\n==============================\n" );
    //DUSK_LOG_RAW( "%i : 0x%x | %s\n", i, pSymbol->Address, pSymbol->Name );
    DUSK_LOG_RAW( "==============================\nEnd\n\n" );
}
#endif

[[noreturn]] void FatalError( const char* description, ... )
{
    char buffer[4096] = { 0 };

    va_list argList = {};
    va_start( argList, description );
    vsnprintf( buffer, sizeof( buffer ), description, argList );
    va_end( argList );

    //MessageBoxA( NULL, buffer, "Dusk GameEngine: Fatal Error", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );

#if DUSK_DEVBUILD
    // If you've reached this line, it means something really bad happened.
    // Have a look at the stack dump and log files
    DUSK_TRIGGER_BREAKPOINT

    DumpStackBacktrace();
    //MessageBoxA( NULL, "Stack Backtrace has been dumped to the log file", "Dusk GameEngine: Fatal Error", MB_OK | MB_SYSTEMMODAL | MB_ICONASTERISK );
#endif

    abort();
}
#endif

