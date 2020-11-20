/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#ifdef DUSK_WIN
#include "Environment.h"

#include <Core/StringHelpers.h>

#include <Shlobj.h>
#include <shlwapi.h>
#include <intrin.h>
#include <VersionHelpers.h>

void dk::core::DeleteFile( const dkString_t& filePath )
{
    ::DeleteFile( filePath.c_str() );
}

void dk::core::GetFilesByExtension( const dkString_t& filePath, const dkString_t& extension, std::vector<dkString_t>& filesFound )
{
    WIN32_FIND_DATA fileInfo;
    HANDLE hFind;
    dkString_t fullPath = filePath + DUSK_STRING( "*.*" ) + extension;
    hFind = FindFirstFile( fullPath.c_str(), &fileInfo );
    if ( hFind != INVALID_HANDLE_VALUE ) {
        filesFound.push_back( filePath + fileInfo.cFileName );
        while ( FindNextFile( hFind, &fileInfo ) != 0 ) {
            filesFound.push_back( filePath + fileInfo.cFileName );
        }
    }
}

void dk::core::RetrieveWorkingDirectory( dkString_t& workingDirectory )
{
    workingDirectory.reserve( DUSK_MAX_PATH );

    GetModuleFileName( NULL, &workingDirectory[0], DUSK_MAX_PATH );

    // Remove executable name
    PathRemoveFileSpec( &workingDirectory[0] );
    workingDirectory = dkString_t( workingDirectory.c_str() );
    dk::core::SanitizeFilepathSlashes( workingDirectory );
    workingDirectory.append( DUSK_STRING( "/" ) );
}

void dk::core::RetrieveHomeDirectory( dkString_t& homeDirectory )
{
    PWSTR myDocuments;

    if ( SUCCEEDED( SHGetKnownFolderPath( FOLDERID_Documents, KF_FLAG_CREATE, NULL, &myDocuments ) ) ) {
        homeDirectory = dkString_t( myDocuments ) + DUSK_STRING( "/" );
        dk::core::SanitizeFilepathSlashes( homeDirectory );
    }
}

void dk::core::RetrieveSavedGamesDirectory( dkString_t& savedGamesDirectory )
{
    PWSTR mySavedGames;

    if ( SUCCEEDED( SHGetKnownFolderPath( FOLDERID_SavedGames, KF_FLAG_CREATE, NULL, &mySavedGames ) ) ) {
        savedGamesDirectory = dkString_t( mySavedGames ) + DUSK_STRING( "/" );
        dk::core::SanitizeFilepathSlashes( savedGamesDirectory );
    }
}

void dk::core::RetrieveCPUName( dkString_t& cpuName )
{
    dkString_t cpuInfosName;
    i32 cpuInfos[4] = { 0, 0, 0, 0 };

    __cpuid( cpuInfos, 0x80000000 );

    if ( cpuInfos[0] >= 0x80000004 ) {
        for ( i32 i = 0x80000002; i < 0x80000005; ++i ) {
            __cpuid( cpuInfos, i );

            for ( u32 info : cpuInfos ) {
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 0 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 1 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 2 ) ) & 0xFF );
                cpuInfosName += ( static_cast<char>( info >> ( 8 * 3 ) ) & 0xFF );
            }
        }
    }

    cpuName = cpuInfosName;
}

int32_t dk::core::GetCPUCoreCount()
{
    SYSTEM_INFO systemInfo = {};
    GetSystemInfo( &systemInfo );

    return systemInfo.dwNumberOfProcessors;
}

void dk::core::RetrieveOSName( dkString_t& osName )
{
    if ( IsWindows10OrGreater() ) {
        osName = DUSK_STRING( "Windows 10" );
    } else if ( IsWindows8Point1OrGreater() ) {
        osName = DUSK_STRING( "Windows 8.1" );
    } else if ( IsWindows8OrGreater() ) {
        osName = DUSK_STRING( "Windows 8" );
    } else if ( IsWindows7SP1OrGreater() ) {
        osName = DUSK_STRING( "Windows 7 SP1" );
    } else if ( IsWindows7OrGreater() ) {
        osName = DUSK_STRING( "Windows 7" );
    } else if ( IsWindowsVistaSP2OrGreater() ) {
        osName = DUSK_STRING( "Windows Vista SP2" );
    } else if ( IsWindowsVistaSP1OrGreater() ) {
        osName = DUSK_STRING( "Windows Vista SP1" );
    } else if ( IsWindowsVistaOrGreater() ) {
        osName = DUSK_STRING( "Windows Vista" );
    } else if ( IsWindowsXPSP3OrGreater() ) {
        osName = DUSK_STRING( "Windows XP SP3" );
    } else if ( IsWindowsXPSP2OrGreater() ) {
        osName = DUSK_STRING( "Windows XP SP2" );
    } else if ( IsWindowsXPSP1OrGreater() ) {
        osName = DUSK_STRING( "Windows XP SP1" );
    } else if ( IsWindowsXPOrGreater() ) {
        osName = DUSK_STRING( "Windows XP" );
    } else {
        osName = DUSK_STRING( "Windows NT" );
    }
}

std::size_t dk::core::GetTotalRAMSizeAsMB()
{
    MEMORYSTATUSEX memoryStatusEx = {};
    memoryStatusEx.dwLength = sizeof( memoryStatusEx );

    BOOL operationResult = GlobalMemoryStatusEx( &memoryStatusEx );
    return ( operationResult == TRUE ) ? static_cast<uint64_t>( memoryStatusEx.ullTotalPhys >> 20 ) : 0;
}
#endif
