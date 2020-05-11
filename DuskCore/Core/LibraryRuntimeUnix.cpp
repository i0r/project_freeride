/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_UNIX
#include "LibraryRuntime.h"

#include <dlfcn.h>

void* dk::core::LoadRuntimeLibrary( const dkChar_t* libraryName )
{
    return dlopen( libraryName, RTLD_NOW );
}

void dk::core::FreeRuntimeLibrary( void* libraryPointer )
{
    dlclose( libraryPointer );
}

void* dk::core::LoadFuncFromLibrary( void* libraryPointer, const char* functionName )
{
    return dlsym( libraryPointer, functionName );
}
#endif
