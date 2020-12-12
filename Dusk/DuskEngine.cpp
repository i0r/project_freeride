/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "DuskEngine.h"

#include "Core/Allocators/AllocationHelpers.h"
#include "Core/Allocators/LinearAllocator.h"
#include "Core/CommandLineArgs.h"
#include "Core/Environment.h"

#include "FileSystem/VirtualFileSystem.h"

static char     g_BaseBuffer[128]; 
DuskEngine*     g_DuskEngine = nullptr;

DuskEngine::DuskEngine()
    : applicationName( DUSK_STRING( "DuskEngine" ) )
    , allocatedTable( nullptr )
{
    DUSK_RAISE_FATAL_ERROR( ( g_DuskEngine == nullptr ), "A game engine instance has already been created!" );
}

DuskEngine::~DuskEngine()
{
    g_DuskEngine = nullptr;
}

void DuskEngine::create( DuskEngine::Parameters creationParameters, const char* cmdLineArgs )
{
    // Retrieve the application name (if provided).
    setApplicationName( creationParameters.ApplicationName );

    Timer profileTimer;
    profileTimer.start();
    DUSK_LOG_RAW( "================================\n%s %s\n%hs\nCompiled with: %s\n================================\n\n", applicationName.c_str(), DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    // Allocate a memory chunk for every subsystem used by the engine.
    allocatedTable = dk::core::malloc( creationParameters.GlobalMemoryTableSize );
    DUSK_LOG_INFO( "Global memory table allocated at: 0x%x\n", allocatedTable );
    globalAllocator = new ( g_BaseBuffer ) LinearAllocator( creationParameters.GlobalMemoryTableSize, allocatedTable );

    // Wait until I/O subsystems are initialized (otherwise command line arguments
    // might get overriden by user configuration files; which is not the expected
    // behavior!)
    dk::core::ReadCommandLineArgs( cmdLineArgs );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

void DuskEngine::shutdown()
{
    DUSK_LOG_RAW( "\n================================\nShutdown has started!\n================================\n\n" );

}

void DuskEngine::setApplicationName( const dkChar_t* name /*= DUSK_STRING( "Dusk GameEngine" ) */ )
{
    if ( name == nullptr ) {
        return;
    }

    applicationName = dkString_t( name );
}

void DuskEngine::mainLoop()
{

}

void DuskEngine::initializeIoSubsystems()
{
    DUSK_LOG_INFO( "Initializing I/O subsystems...\n" );

    virtualFileSystem = dk::core::allocate<VirtualFileSystem>( globalAllocator );

    dkString_t cfgFilesDir;
    dk::core::RetrieveSavedGamesDirectory( cfgFilesDir );

    if ( cfgFilesDir.empty() ) {
        DUSK_LOG_WARN( "Failed to retrieve 'Saved Games' folder (this is expected behavior on Unix)\n" );

        dk::core::RetrieveHomeDirectory( cfgFilesDir );

        DUSK_RAISE_FATAL_ERROR( !cfgFilesDir.empty(), "Failed to retrieve a suitable directory for savegame storage on your system... (cfgFilesDir = %s)", cfgFilesDir.c_str() );
    }
//
//    // Prepare files/folders stored on the system fs
//    // For now, configuration/save files will be stored in the same folder
//    // This might get refactored later (e.g. to implement profile specific config/save for each system user)
//    FileSystemNative* saveFolder = dk::core::allocate<FileSystemNative>( globalAllocator, cfgFilesDir );
//
//#if DUSK_UNIX
//    // Use *nix style configuration folder name
//    dkString_t configurationFolderName = DUSK_STRING( "SaveData/.dusked/" );
//#else
//    dkString_t configurationFolderName = DUSK_STRING( "SaveData/DuskEd/" );
//#endif
//
//    DUSK_LOG_INFO( "Mounting filesystems...\n" );
//
//    g_DataFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./data/" ) );
//
//    g_VirtualFileSystem->mount( g_DataFileSystem, DUSK_STRING( "GameData" ), 1 );
//
//    g_GameFileSystem = dk::core::allocate<FileSystemArchive>( g_GlobalAllocator, g_GlobalAllocator, DUSK_STRING( "./Game.zip" ) );
//    // g_VirtualFileSystem->mount( g_GameFileSystem, DUSK_STRING( "GameData/" ), 0 );
//
//#if DUSK_DEVBUILD
//    DUSK_LOG_INFO( "Mounting devbuild filesystems...\n" );
//
//    // TODO Make this more flexible? (do not assume the current working directory).
//    g_EdAssetsFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./../../Assets/" ) );
//    g_RendererFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, DUSK_STRING( "./../../Dusk/Graphics/" ) );
//
//    g_VirtualFileSystem->mount( g_EdAssetsFileSystem, DUSK_STRING( "EditorAssets" ), 0 );
//    g_VirtualFileSystem->mount( g_RendererFileSystem, DUSK_STRING( "EditorAssets" ), 1 );
//    g_VirtualFileSystem->mount( g_EdAssetsFileSystem, DUSK_STRING( "GameData" ), 1 );
//#endif
//
//    dkString_t SaveFolder = saveFolder->resolveFilename( DUSK_STRING( "SaveData/" ), configurationFolderName );
//
//    if ( !saveFolder->fileExists( SaveFolder ) ) {
//        DUSK_LOG_INFO( "First run detected! Creating save folder at '%s')\n", SaveFolder.c_str() );
//
//        saveFolder->createFolder( SaveFolder );
//    }
//    dk::core::free( g_GlobalAllocator, saveFolder );
//
//    // TODO Store thoses paths somewhere so that we don't hardcode this everywhere...
//    if ( !g_DataFileSystem->fileExists( DUSK_STRING( "./data/" ) ) ) {
//        g_DataFileSystem->createFolder( DUSK_STRING( "./data/" ) );
//        g_DataFileSystem->createFolder( DUSK_STRING( "./data/cache/" ) );
//        g_DataFileSystem->createFolder( DUSK_STRING( "./data/materials/" ) );
//        g_DataFileSystem->createFolder( DUSK_STRING( "./data/failed_shaders/" ) );
//    }
//
//    Logger::SetLogOutputFile( SaveFolder, DUSK_STRING( "DuskEd" ) );
//
//    DUSK_LOG_INFO( "SaveData folder at : '%s'\n", SaveFolder.c_str() );
//
//    g_SaveFileSystem = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, SaveFolder );
//    g_VirtualFileSystem->mount( g_SaveFileSystem, DUSK_STRING( "SaveData" ), UINT64_MAX );
//
//    FileSystemObject* envConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_READ );
//    if ( envConfigurationFile == nullptr ) {
//        IsFirstLaunch = true;
//
//        DUSK_LOG_INFO( "Creating default user configuration!\n" );
//        FileSystemObject* newEnvConfigurationFile = g_VirtualFileSystem->openFile( DUSK_STRING( "SaveData/environment.cfg" ), eFileOpenMode::FILE_OPEN_MODE_WRITE );
//        EnvironmentVariables::serialize( newEnvConfigurationFile );
//        newEnvConfigurationFile->close();
//    } else {
//        DUSK_LOG_INFO( "Loading user configuration...\n" );
//        EnvironmentVariables::deserialize( envConfigurationFile );
//        envConfigurationFile->close();
//    }
//
//#if 0
//    g_FileSystemWatchdog = dk::core::allocate<FileSystemWatchdog>( g_GlobalAllocator );
//    g_FileSystemWatchdog->create();
//#endif
}
