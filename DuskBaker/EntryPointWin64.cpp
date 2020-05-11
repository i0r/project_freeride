/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#if DUSK_WIN
#include <Shared.h>
#include "Baker.h"

// Application EntryPoint (Windows)
i32 main( i32 argc, char** argv )
{
    DUSK_LOG_INITIALIZE;

    // Stupid trick to concat the cmdline (to emulate Windows cmdline format)
    std::string concatCmdLine;
    for ( int i = 1; i < argc; i++ ) {
        concatCmdLine += argv[i];

        if ( i != argc - 1 ) //this check prevents adding a space after last argument.
            concatCmdLine += " ";
    }

    dk::baker::Start( concatCmdLine.c_str() );

    return 0;
}
#endif
