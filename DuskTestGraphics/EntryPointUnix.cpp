/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#if DUSK_UNIX
#include <Shared.h>
#include "App.h"

// Forces dedicated GPU usage if the system uses an hybrid GPU (e.g. laptops)
extern "C"
{
    __attribute__( ( visibility( "default" ) ) ) u32   NvOptimusEnablement = 0x00000001;
    __attribute__( ( visibility( "default" ) ) ) i32    AmdPowerXpressRequestHighPerformance = 1;
}

// Application EntryPoint (Unix)
i32 main( i32 argc, char** argv )
{
    DUSK_LOG_INITIALIZE;

    // Stupid trick to concat the cmdline (to emulate Windows cmdline format)
    std::string concatCmdLine;
    for (int i = 1; i<argc; i++) {
        concatCmdLine += argv[i];

        if (i != argc-1) //this check prevents adding a space after last argument.
            concatCmdLine += " ";
    }

    dk::test::Start( concatCmdLine.c_str() );

    return 0;
}
#endif
