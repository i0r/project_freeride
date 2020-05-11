/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#ifndef __DUSK_SHARED_HEADER__
#define __DUSK_SHARED_HEADER__ 1
#include "Core/BuildFlags.h"
#include "Core/SystemIncludes.h"
#include "Core/Types.h"
#include "Core/Logger.h"
#include "Core/DebugHelpers.h"
#include "Core/EnvVarsRegister.h"

#include "Core/Hashing/CompileTimeCRC32.h"
#include "Core/Hashing/CRC32.h"

#include "Core/Allocators/BaseAllocator.h"

#if DUSK_MSVC
// MSVC will flag a macro redefinition as a warning (even though this is
// the expected behavior)
#pragma warning( push )
#pragma warning( disable : 4651 )
#pragma warning( disable : 4005 )
#endif

#define DUSK_GEN_ENUM_ENTRY( optionName ) optionName,
#define DUSK_GEN_DUSK_STRING_ENTRY( optionName ) (dkChar_t*)DUSK_STRING( #optionName ),
#define DUSK_GEN_STRING_ENTRY( optionName ) (char*)#optionName,
#define DUSK_ENUM_TO_STRING_CASE( optionName ) case DUSK_STRING_HASH( #optionName ): return optionName;

// Generate a strongly typed enum lazily (create the enum, enum to string convertion, hashcode to enum convertion, stream operator overload, etc.)
//      optionList: an user defined macro holding each enum member name (e.g. #define optionList( operator ) operator( ENUM_MEMBER_1 ) operator( ENUM_MEMBER_2 ))
#define DUSK_LAZY_ENUM( optionList )\
enum e##optionList : u32\
{\
    optionList( DUSK_GEN_ENUM_ENTRY )\
    optionList##_COUNT\
};\
\
static constexpr dkChar_t* optionList##ToNyaString[e##optionList::optionList##_COUNT] = \
{\
    optionList( DUSK_GEN_DUSK_STRING_ENTRY )\
};\
static constexpr char* optionList##ToString[e##optionList::optionList##_COUNT] = \
{\
    optionList( DUSK_GEN_STRING_ENTRY )\
};\
\
static e##optionList StringTo##optionList( const dkStringHash_t hashcode )\
{\
    switch ( hashcode ) {\
        optionList( DUSK_ENUM_TO_STRING_CASE )\
    default:\
        return (e##optionList)0;\
    };\
}

template< typename T, typename R >
static DUSK_INLINE constexpr T Min( const T a, const R b )
{
    return static_cast< T >( ( a < b ) ? a : b );
}

template< typename T, typename R >
static DUSK_INLINE constexpr T Max( const T a, const R b )
{
    return static_cast< T >( ( a > b ) ? a : b );
}

template< typename T >
static DUSK_INLINE constexpr T Align16( const T a )
{
    return ( a + 0x0f ) & ( ~0x0f );
}

template <typename T>
static DUSK_INLINE constexpr T Align( const T a, const T b )
{
  return ( a + ( b - 1 ) ) & ( ~( b - 1 ) );
}

static DUSK_INLINE constexpr u32 MakeFourCC( const u32 a, const u32 b, const u32 c, const u32 d )
{
    return ( d << 24 ) | ( c << 16) | ( b << 8 ) | a;
}

#define DUSK_IS_MEMORY_ALIGNED_STATIC( obj, alignInBytes ) static_assert( sizeof( obj ) % alignInBytes == 0, #obj " alignement is invalid!" );
#endif
