/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_WIN
#include "FileSystemWin32.h"

bool dk::core::FileExistsImpl( const dkString_t& filename )
{
    return GetFileAttributes( filename.c_str() ) != INVALID_FILE_ATTRIBUTES;
}

void dk::core::CreateFolderImpl( const dkString_t& folderName )
{
    CreateDirectory( folderName.c_str(), nullptr );
}
#endif
