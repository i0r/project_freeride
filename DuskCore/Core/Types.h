/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <cstdint>
#include <string>

#if DUSK_UNICODE
using dkChar_t = wchar_t;

#define DUSK_TO_STRING std::to_wstring
#define DUSK_STRING( str ) L##str
#else
using dkChar_t = char;

#define DUSK_TO_STRING std::to_string
#define DUSK_STRING( str ) str
#endif

using dkString_t = std::basic_string<dkChar_t>;

#if DUSK_WIN
#define DUSK_MAX_PATH MAX_PATH
#elif DUSK_UNIX
#include <linux/limits.h>
#define DUSK_MAX_PATH PATH_MAX
#endif

#if defined( DUSK_MSVC ) || defined( DUSK_GCC )
#define DUSK_RESTRICT __restrict
#endif

#if DUSK_GCC
#define DUSK_UNUSED __attribute__ ( ( unused ) )
#else
#define DUSK_UNUSED
#endif

#define DUSK_UNUSED_VARIABLE( x ) (void)x

#define DUSK_INLINE inline

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using byte = u8;

#include <Core/Hashing/CompileTimeCRC32.h>
#define DUSK_STRING_HASH( str ) dk::core::CompileTimeCRC32( str )

using dkStringHash_t = u32;
using dkStringHash32_t = u32;
