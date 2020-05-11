/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_WIN
#include "LibraryRuntime.h"

void* dk::core::LoadRuntimeLibrary( const dkChar_t* libraryName )
{
    return LoadLibrary( libraryName );
}

void dk::core::FreeRuntimeLibrary( void* libraryPointer )
{
    FreeLibrary( static_cast<HMODULE>( libraryPointer ) );
}

void* dk::core::LoadFuncFromLibrary( void* libraryPointer, const char* functionName )
{
    return GetProcAddress( static_cast<HMODULE>( libraryPointer ), functionName );
}
#endif
