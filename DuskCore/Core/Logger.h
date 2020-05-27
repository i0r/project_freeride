/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "BuildFlags.h"
#include <Shared.h>

class Logger
{
public:
    static void     Write( const dkStringHash_t, const dkChar_t*, ... );

    static void     SetLogOutputFile( const dkString_t& logFolder, const dkString_t& applicationName );
    static void     CloseOutputStreams();
};

#if DUSK_LOGGING_IS_ENABLED
#include "Timer.h"

extern Timer g_LoggerTimer;
extern dkChar_t g_LogHistoryTest[2 * 4096];
extern i32  g_LogHistoryPointer;

// Setup logging timer
// Should be called AS EARLY AS possible to ensure
// reliable logging times
#define DUSK_LOG_INITIALIZE g_LoggerTimer.start()
#define DUSK_LOG_RAW( format, ... ) Logger::Write( DUSK_STRING_HASH( DUSK_LOG_CATEGORY ), DUSK_STRING( format ), ##__VA_ARGS__ )

#ifndef DUSK_LOG_CATEGORY
#define DUSK_LOG_CATEGORY "Unknown"
#endif

#ifndef DUSK_FILENAME
#define DUSK_FILENAME ""
#endif

#define DUSK_STRINGIFY_EXPAND( x ) #x
#define DUSK_STRINGIFY( x ) DUSK_STRINGIFY_EXPAND( x )
#define DUSK_LOG_INFO( format, ... ) Logger::Write( DUSK_STRING_HASH( DUSK_LOG_CATEGORY ), "[%.3f][LOG]\t" DUSK_FILENAME  ":" DUSK_STRINGIFY( __LINE__ ) " >> " DUSK_STRING( format ), g_LoggerTimer.getElapsedTimeAsSeconds(), ##__VA_ARGS__ )
#define DUSK_LOG_WARN( format, ... ) Logger::Write( DUSK_STRING_HASH( DUSK_LOG_CATEGORY ), "[%.3f][WARN]\t" DUSK_FILENAME  ":" DUSK_STRINGIFY( __LINE__ ) " >> "  DUSK_STRING( format ), g_LoggerTimer.getElapsedTimeAsSeconds(), ##__VA_ARGS__ )
#define DUSK_LOG_ERROR( format, ... ) Logger::Write( DUSK_STRING_HASH( DUSK_LOG_CATEGORY ), "[%.3f][ERROR]\t" DUSK_FILENAME  ":" DUSK_STRINGIFY( __LINE__ ) " >> "  DUSK_STRING( format ), g_LoggerTimer.getElapsedTimeAsSeconds(), ##__VA_ARGS__ )
#define DUSK_LOG_DEBUG( format, ... ) Logger::Write( DUSK_STRING_HASH( DUSK_LOG_CATEGORY ), "[%.3f][DEBUG]\t" DUSK_FILENAME  ":" DUSK_STRINGIFY( __LINE__ ) " >> "  DUSK_STRING( format ), g_LoggerTimer.getElapsedTimeAsSeconds(), ##__VA_ARGS__ )
#else
// Setup logging timer
// Should be called AS EARLY AS possible to ensure
// reliable logging times
#define DUSK_LOG_INITIALIZE
#define DUSK_LOG_RAW( format, ... )

#define DUSK_LOG_INFO( format, ... )
#define DUSK_LOG_WARN( format, ... )
#define DUSK_LOG_ERROR( format, ... )
#define DUSK_LOG_DEBUG( format, ... )
#endif
