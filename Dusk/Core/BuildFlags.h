/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if defined( _M_X64 ) || defined( __amd64 ) || defined( __amd64__ )
#define DUSK_X64
#define DUSK_BUILD_PROC DUSK_STRING( "x64" )
#elif defined( _M_IX86 )
#define DUSK_X86
#define DUSK_BUILD_PROC DUSK_STRING( "x86" )
#else
#define DUSK_BUILD_PROC
#endif

#if defined( DUSK_X64 ) || defined( DUSK_X86 )
#define DUSK_SSE42
#define DUSK_SIMD_LIB DUSK_STRING( "SSE_4.2" )
#elif defined( DUSK_ARM )
#define DUSK_NEON
#define DUSK_SIMD_LIB DUSK_STRING( "ARM_NEON" )
#endif

#if defined( _DEBUG ) || defined( DEBUG ) || !defined( NDEBUG )
#define DUSK_DEBUG
#define DUSK_BUILD_TYPE DUSK_STRING( "DEBUG" )
#else
#define DUSK_RELEASE
#define DUSK_BUILD_TYPE DUSK_STRING( "RELEASE" )
#endif

#if DUSK_WIN
#define DUSK_BUILD_PLATFORM DUSK_STRING( "WINDOWS" )
#elif DUSK_UNIX
#define DUSK_BUILD_PLATFORM DUSK_STRING( "UNIX" )
#else
#define DUSK_BUILD_PLATFORM DUSK_STRING( "UNKNOWN" )
#endif

#if DUSK_USE_BULLET
#define DUSK_USE_PHYSICS 1
#endif

#if !defined( DUSK_D3D12 ) || !defined( DUSK_VULKAN ) || !defined( DUSK_D3D11 )
#define DUSK_STUB 1
#endif

#if DUSK_D3D12
#define DUSK_GFX_BACKEND DUSK_STRING( "D3D12" )
#elif DUSK_VULKAN
#define DUSK_GFX_BACKEND DUSK_STRING( "VULKAN" )
#elif DUSK_D3D11
#define DUSK_GFX_BACKEND DUSK_STRING( "D3D11" )
#elif DUSK_STUB
#define DUSK_GFX_BACKEND DUSK_STRING( "HEADLESS" )
#endif

#if DUSK_MSVC
#if _MSC_VER >= 1910
#define DUSK_COMPILER DUSK_STRING( "Visual Studio 2017 (15.0)" )
#elif _MSC_VER >= 1900
#define DUSK_COMPILER DUSK_STRING( "Visual Studio 2015 (14.0)" )
#elif _MSC_VER >= 1800
#define DUSK_COMPILER DUSK_STRING( "Visual Studio 2013 (12.0)" )
#else
#define DUSK_COMPILER DUSK_STRING( "Visual Studio 2012 (11.0)" )
#endif
#elif DUSK_GCC
#if defined(__GNUC__)
#define DUSK_COMPILER DUSK_STRING( "gcc 5.1" )
#endif
#else
#define DUSK_COMPILER DUSK_STRING( "Unknown Compiler" )
#endif

#if DUSK_DEVBUILD
#define DUSK_VERSION_TYPE DUSK_STRING( "DEV_BUILD" )

#define DUSK_LOGGING_IS_ENABLED 1
#define DUSK_LOGGING_USE_DEBUGGER_OUTPUT 1 // Use OutputDebug (or any platform equivalent). Usually slow AS HELL, but super convenient if you use VisualStudio
#define DUSK_LOGGING_USE_FILE_OUTPUT 1 // Outputs log to a file saved on your hard drive.
#define DUSK_LOGGING_USE_CONSOLE_OUTPUT 1 // Use Console Output even if another output is available on your system
#else
#define DUSK_VERSION_TYPE DUSK_STRING( "PROD_BUILD" )
#endif

#define DUSK_VERSION DUSK_STRING( "0.02-" ) DUSK_VERSION_TYPE
#define DUSK_BUILD DUSK_BUILD_PLATFORM DUSK_STRING( "-" ) DUSK_BUILD_PROC DUSK_STRING( "-" ) DUSK_GFX_BACKEND DUSK_STRING( "-" )\
                  DUSK_BUILD_TYPE DUSK_STRING( "-" ) DUSK_VERSION

static constexpr const char* const DUSK_BUILD_DATE = __DATE__ " " __TIME__;
