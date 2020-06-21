/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

namespace dk
{
    namespace core
    {
        void*   LoadRuntimeLibrary( const dkChar_t* libraryName );
        void    FreeRuntimeLibrary( void* libraryPointer );
        void*   LoadFuncFromLibrary( void* libraryPointer, const char* functionName );
    }
}
