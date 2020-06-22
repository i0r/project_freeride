/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#if DUSK_WIN
#include <Shared.h>
#include "Editor.h"

#if DUSK_FORCE_DEDICATED_GPU
// Forces dedicated GPU usage if the system uses an hybrid GPU (e.g. laptops)
extern "C"
{
    __declspec( dllexport ) DWORD NvOptimusEnablement = 0x00000001;
    __declspec( dllexport ) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// Application EntryPoint (Windows)
int WINAPI CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
    DUSK_LOG_INITIALIZE;

    SetProcessWorkingSetSize( GetCurrentProcess(), 256 << 20, 2048 << 20 );

    HANDLE crtHeap = GetProcessHeap();    
    ULONG HeapInformation = 2;
    if ( !HeapSetInformation( crtHeap, HeapCompatibilityInformation, &HeapInformation, sizeof( ULONG ) ) ) {
        return 1;
    }

    dk::editor::Start( lpCmdLine );

    return 0;
}
#endif
