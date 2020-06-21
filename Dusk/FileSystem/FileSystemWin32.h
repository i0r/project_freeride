/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_WIN
namespace dk
{
    namespace core
    {
        bool    FileExistsImpl( const dkString_t& filename );
        void    CreateFolderImpl( const dkString_t& folderName );
    }
}
#endif
