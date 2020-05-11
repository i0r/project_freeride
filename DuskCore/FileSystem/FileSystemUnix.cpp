/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_UNIX
#include "FileSystemUnix.h"

#include <sys/stat.h>

bool dk::core::FileExistsImpl( const dkString_t& filename )
{
    struct stat buffer;
    return ( stat( filename.c_str(), &buffer ) == 0 );
}

void dk::core::CreateFolderImpl( const dkString_t& folderName )
{
    mkdir( folderName.c_str(), S_IRUSR | S_IWUSR | S_IROTH );
}
#endif
