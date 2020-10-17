/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_WIN
#include "DebugHelpers.h"

#if DUSK_DEVBUILD
#include <dbghelp.h>

typedef DWORD( _stdcall *SymSetOptionsProc )( DWORD ) ;
typedef DWORD( _stdcall *SymInitializeProc )( HANDLE, PCSTR, BOOL );
typedef DWORD( _stdcall *SymGetSymFromAddr64Proc )( HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64 );

void DumpStackBacktrace()
{
    HMODULE dbgHelp = LoadLibrary( DUSK_STRING( "dbghelp.dll" ) );

    SymSetOptionsProc symSetOptions = ( SymSetOptionsProc )GetProcAddress( dbgHelp, "SymSetOptions" );
    SymInitializeProc symInitialize = ( SymInitializeProc )GetProcAddress( dbgHelp, "SymInitialize" );
    SymGetSymFromAddr64Proc symGetSymFromAddr64 = ( SymGetSymFromAddr64Proc )GetProcAddress( dbgHelp, "SymGetSymFromAddr64" );

    HANDLE hProcess = GetCurrentProcess();
    symSetOptions( SYMOPT_DEFERRED_LOADS );
    symInitialize( hProcess, NULL, TRUE );

    // NOTE Explicitly ignore the first frame ( = the function itself)
    void* callers[62];
    WORD count = RtlCaptureStackBackTrace( 1, 62, callers, nullptr );

    unsigned char symbolBuffer[sizeof( IMAGEHLP_SYMBOL64 ) + 512];
    PIMAGEHLP_SYMBOL64 pSymbol = reinterpret_cast< PIMAGEHLP_SYMBOL64 >( symbolBuffer );
    memset( pSymbol, 0, sizeof( IMAGEHLP_SYMBOL64 ) + 512 );
    pSymbol->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL64 );
    pSymbol->MaxNameLength = 512;

    DUSK_LOG_INFO( "Dumping stack backtrace...\n==============================\n" );
    for ( i32 i = 0; i < count; i++ ) {
        // Retrieve frame infos
        if ( !symGetSymFromAddr64( hProcess, ( DWORD64 )callers[i], nullptr, pSymbol ) ) {
            continue;
        }

        DUSK_LOG_RAW( "%i : 0x%x | %s\n", i, pSymbol->Address, pSymbol->Name );
    }
    DUSK_LOG_RAW( "==============================\nEnd\n\n" );
}
#endif

bool IsDebuggerAttached()
{
    return IsDebuggerPresent();
}

[[noreturn]] void FatalError( const char* description, ... )
{
    char buffer[4096] = { 0 };

    va_list argList = {};
    va_start( argList, description );
    const i32 textLength = vsnprintf_s( buffer, sizeof( buffer ), sizeof( buffer ) - 1, description, argList );
    va_end( argList );

    MessageBoxA( NULL, buffer, "Dusk GameEngine: Fatal Error", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR );

#if DUSK_DEVBUILD
    // If you've reached this line, it means something really bad happened.
    // Have a look at the stack dump and log files
    DUSK_TRIGGER_BREAKPOINT;

    DumpStackBacktrace();
    MessageBoxA( NULL, "Stack Backtrace has been dumped to the log file", "Dusk GameEngine: Fatal Error", MB_OK | MB_SYSTEMMODAL | MB_ICONASTERISK );
#endif

    abort();
}
#endif

