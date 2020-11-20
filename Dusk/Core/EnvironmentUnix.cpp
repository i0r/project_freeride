/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_UNIX
#include "Environment.h"

#include "Core/StringHelpers.h"

#include <cpuid.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fstream>
#include <dirent.h>

#include <sys/types.h>
#include <sys/sysinfo.h>

void dk::core::DeleteFile( const dkString_t& filePath )
{
    std::remove( filePath.c_str() );
}

void dk::core::GetFilesByExtension( const dkString_t& filePath, const dkString_t& extension, std::vector<dkString_t>& filesFound )
{
    dkString_t fullPath = filePath;

    DIR* workingDirectory = opendir( fullPath.c_str() );
    if ( workingDirectory == nullptr ) {
        DUSK_LOG_WARN( "Failed to open dir '%s' (either malformated string or invalid path)\n", filePath.c_str() );
        return;
    }

    struct dirent* dirEntry = readdir( workingDirectory );
    while ( dirEntry != nullptr ) {
        dkString_t fileEntryExtension = GetFileExtensionFromPath( dirEntry->d_name );
        if ( fileEntryExtension != extension ) {
            DUSK_LOG_DEBUG( "Skipping; extension not matching (%s : %s)\n", fileEntryExtension.c_str(), extension.c_str() );
            dirEntry = readdir( workingDirectory );
            continue;
        }

        dkString_t path = filePath;
        if ( path.back() != '/' ) path.push_back( '/' );
        path.append( dirEntry->d_name );

        filesFound.push_back( path );
        dirEntry = readdir( workingDirectory );
    }

    closedir( workingDirectory );
}

void dk::core::RetrieveWorkingDirectory( dkString_t& workingDirectory )
{
    workingDirectory.reserve( DUSK_MAX_PATH );
    getcwd( &workingDirectory[0], DUSK_MAX_PATH );

    workingDirectory = dkString_t( workingDirectory.c_str() );
    workingDirectory.append( "/" );
}

void dk::core::RetrieveHomeDirectory( dkString_t& homeDirectory )
{
    const dkChar_t* envVarHome = getenv( "XDG_DATA_HOME" );

    if ( envVarHome == nullptr ) {
        DUSK_LOG_WARN( "$XDG_DATA_HOME is undefined; using $HOME as fallback...\n" );

        envVarHome = getenv( "HOME" );
    }

    homeDirectory = dkString_t( envVarHome ) + "/";
}

void dk::core::RetrieveSavedGamesDirectory( dkString_t& savedGamesDirectory )
{
    RetrieveHomeDirectory( savedGamesDirectory );
}

void dk::core::RetrieveCPUName( dkString_t& cpuName )
{
    dkString_t cpuInfosName;
    u32 cpuInfos[4] = { 0, 0, 0, 0 };

    __get_cpuid( 0x80000000, &cpuInfos[0], &cpuInfos[1], &cpuInfos[2], &cpuInfos[3] );

    if ( cpuInfos[0] >= 0x80000004 ) {
        for ( u32 i = 0x80000002; i < 0x80000005; ++i ) {
            __get_cpuid( i, &cpuInfos[0], &cpuInfos[1], &cpuInfos[2], &cpuInfos[3] );

            for ( u32 info : cpuInfos ) {
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 0 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 1 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 2 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 3 ) ) & 0xFF );
            }
        }
    } else {
        DUSK_LOG_ERROR( "Failed to retrieve CPU Name (EAX register returned empty)\n" );
    }

    cpuName = cpuInfosName;
}

int32_t dk::core::GetCPUCoreCount()
{
    return static_cast<int32_t>( sysconf( _SC_NPROCESSORS_ONLN ) );
}

void dk::core::RetrieveOSName( dkString_t& osName )
{
    std::string unixVersion = "Unix";

    std::ifstream unixVersionFile( "/proc/version" );

    if ( unixVersionFile.is_open() ) {
        unixVersionFile >> unixVersion;
        unixVersionFile.close();
    }

    osName = unixVersion;
}

std::size_t dk::core::GetTotalRAMSizeAsMB()
{
    struct sysinfo infos = {};

    if ( sysinfo( &infos ) == 0 ) {
        return ( infos.totalram >> 20 );
    }

    return 0;
}
#endif
