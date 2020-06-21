/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Logger.h"

#include <stdarg.h>
#include <string.h>

static i32  g_LogFileHandle = 0;

i32  g_LogHistoryPointer = 0;
dkChar_t  g_LogHistoryTest[2 * 4096] = { '\0' };

Timer g_LoggerTimer;

// TODO Temp hotfix for MSVC annoying deprecation crap
#ifdef DUSK_MSVC
#define DUSK_ISO_CALL( x ) _##x
#else
#define DUSK_ISO_CALL( x ) x 
#endif

#if DUSK_LOGGING_USE_DEBUGGER_OUTPUT
#if DUSK_MSVC
DUSK_INLINE void DebuggerOutput( const dkChar_t* stringOutput )
{
    OutputDebugString( stringOutput );
}
#else
// If there is no dedicated Output debug on the current platform,
// output to the console stream (safest way to print stuff)
DUSK_INLINE void DebuggerOutput( const char* stringOutput )
{
    printf( "%s", stringOutput );
}
#endif
#endif

void Logger::Write( const dkStringHash_t categoryHashcode, const dkChar_t* format, ... )
{
    DUSK_UNUSED_VARIABLE( categoryHashcode );

    thread_local dkChar_t BUFFER[4096];

    va_list argList;
    va_start( argList, format );

#if DUSK_UNICODE
    i32 bufferOutputSize = vswprintf( BUFFER, sizeof( BUFFER ), format, argList );
#else
    i32 bufferOutputSize = vsnprintf( BUFFER, sizeof( BUFFER ), format, argList );
#endif

    va_end( argList );

#if DUSK_LOGGING_USE_DEBUGGER_OUTPUT
    DebuggerOutput( BUFFER );
#endif

#if DUSK_LOGGING_USE_CONSOLE_OUTPUT
    wprintf( DUSK_STRING( "%s" ), BUFFER );
#endif

#if DUSK_LOGGING_USE_FILE_OUTPUT
    DUSK_ISO_CALL( write( g_LogFileHandle, BUFFER, static_cast<size_t>( bufferOutputSize ) * sizeof( dkChar_t ) ) );
#endif
    
    // TODO TEMP workaround to 5avoid buffer overrun
    // Should be fixed once the logging refactoring is done
    if ( (2 * 4096) <= ( g_LogHistoryPointer + bufferOutputSize ) ) {
        g_LogHistoryPointer = 0;
    }

    memcpy( &g_LogHistoryTest[g_LogHistoryPointer], BUFFER, static_cast<size_t>( bufferOutputSize ) * sizeof( dkChar_t ) );

    g_LogHistoryPointer += bufferOutputSize;
    g_LogHistoryTest[g_LogHistoryPointer] = '\0';
}

void Logger::SetLogOutputFile( const dkString_t& logFolder, const dkString_t& applicationName )
{
#if DUSK_LOGGING_USE_FILE_OUTPUT
    constexpr const dkChar_t* LOG_FILE_EXTENSION = static_cast<const dkChar_t* const>( DUSK_STRING( ".log" ) );
    dkString_t fileName = ( logFolder + applicationName + LOG_FILE_EXTENSION );

#if DUSK_MSVC
    _wsopen_s( &g_LogFileHandle, fileName.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE );
#else
    // Use Unix open() instead of Microsoft convoluted function
    g_LogFileHandle = open( fileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
#endif

#if DUSK_UNICODE
    static constexpr u16 UNICODE_MAGIC = 0xFEFF;
    DUSK_ISO_CALL( write( g_LogFileHandle, &UNICODE_MAGIC, sizeof( u16 ) ) );
#endif

    DUSK_ISO_CALL( write( g_LogFileHandle, g_LogHistoryTest, static_cast<size_t>( g_LogHistoryPointer ) * sizeof( dkChar_t ) ) );
#endif
}

void Logger::CloseOutputStreams()
{
#if DUSK_LOGGING_USE_FILE_OUTPUT
    DUSK_ISO_CALL( close( g_LogFileHandle ) );
#endif
}
