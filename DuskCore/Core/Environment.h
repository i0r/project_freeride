/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <vector>

namespace dk
{
    namespace core
    {
        void        GetFilesByExtension( const dkString_t& filePath, const dkString_t& extension, std::vector<dkString_t>& filesFound );

        void        RetrieveWorkingDirectory( dkString_t& workingDirectory );
        void        RetrieveHomeDirectory( dkString_t& homeDirectory );
        void        RetrieveSavedGamesDirectory( dkString_t& savedGamesDirectory );

        void        RetrieveCPUName( dkString_t& cpuName );
        int32_t     GetCPUCoreCount();

        void        RetrieveOSName( dkString_t& osName );
        std::size_t GetTotalRAMSizeAsMB();
    }
}
