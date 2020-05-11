/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_DEVBUILD
#if DUSK_WIN
#include "FileSystemWatchdogWin32.h"

#include <Core/Environment.h>
#include <Core/StringHelpers.h>

#include <Graphics/ShaderCache.h>

#include <thread>
#include <functional>

FileSystemWatchdog::FileSystemWatchdog()
    : watchdogHandle( nullptr )
    , shutdownSignal( false )
{

}

FileSystemWatchdog::~FileSystemWatchdog()
{
    // Send signal to the watchdog handle first (should work)
    CancelIoEx( watchdogHandle, nullptr );
    CloseHandle( watchdogHandle );
    watchdogHandle = nullptr;

    // Then toggle the atomic boolean (which should properly stop the thread)
    shutdownSignal.store( true );
}

void FileSystemWatchdog::create()
{
    dkString_t workingDirectory;
    dk::core::RetrieveWorkingDirectory( workingDirectory );
    watchdogHandle = ::CreateFile( workingDirectory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL );

    monitorThread = std::thread( std::bind( &FileSystemWatchdog::monitor, this ) );
}

void FileSystemWatchdog::onFrame( GraphicsAssetCache* graphicsAssetManager, ShaderCache* shaderCache )
{
    //while ( !materialsToReload.empty() ) {
    //    auto& materialToReload = materialsToReload.front();
    //    graphicsAssetManager->getMaterial( materialToReload.c_str(), true );

    //    materialsToReload.pop();
    //}

    //while ( !texturesToReload.empty() ) {
    //    auto& textureToReload = texturesToReload.front();
    //    graphicsAssetManager->getTexture( textureToReload.c_str(), true );

    //    texturesToReload.pop();
    //}

    //while ( !meshesToReload.empty() ) {
    //    auto& meshToReload = meshesToReload.front();
    //    graphicsAssetManager->getMesh( meshToReload.c_str(), true );

    //    meshesToReload.pop();
    //}

    //while ( !shadersToReload.empty() ) {
    //    auto& shaderToReload = shadersToReload.front();
    //    shaderCache->getOrUploadStage( shaderToReload.Filename, shaderToReload.StageType, true );

    //    shadersToReload.pop();
    //}
}

void FileSystemWatchdog::monitor()
{
    const DWORD MAX_BUFFER = 1024;

    BYTE Buffer[MAX_BUFFER] = {};
    WCHAR fileNameModified[FILENAME_MAX] = {};

    DWORD dwBytesReturned = 0;
    BOOL changesResult = FALSE;

    while ( 1 ) {
        if ( shutdownSignal.load() ) {
            break;
        }
            
        if ( changesResult && dwBytesReturned != 0 ) {
            const FILE_NOTIFY_INFORMATION* pNotifyInfo = ( FILE_NOTIFY_INFORMATION* )Buffer;
            memcpy( fileNameModified, pNotifyInfo->FileName, pNotifyInfo->FileNameLength );

            if ( pNotifyInfo->Action == FILE_ACTION_MODIFIED || pNotifyInfo->Action == FILE_ACTION_REMOVED ) {
                dkString_t relativeModifiedFilename = fileNameModified;

                // Get VFS File paths
                dk::core::RemoveWordFromString( relativeModifiedFilename, DUSK_STRING( "data\\" ) );
                dk::core::RemoveWordFromString( relativeModifiedFilename, DUSK_STRING( "dev\\" ) );
                dk::core::RemoveWordFromString( relativeModifiedFilename, DUSK_STRING( "\\" ), DUSK_STRING( "/" ) );

                auto extension = dk::core::GetFileExtensionFromPath( relativeModifiedFilename );
                dk::core::StringToLower( extension );

                if ( !extension.empty() ) {
                    DUSK_LOG_INFO( "'%ls' has been edited\n", relativeModifiedFilename.c_str() );

                    auto vfsPath = DUSK_STRING( "GameData/" ) + dkString_t( relativeModifiedFilename.c_str() );

                    auto extensionHashcode = dk::core::CRC32( extension );
                    switch ( extensionHashcode ) {
                    case DUSK_STRING_HASH( "amat" ):
                    case DUSK_STRING_HASH( "mat" ):
                        materialsToReload.push( vfsPath );
                        break;

                    case DUSK_STRING_HASH( "dds" ):
                    case DUSK_STRING_HASH( "bmp" ):
                    case DUSK_STRING_HASH( "jpeg" ):
                    case DUSK_STRING_HASH( "jpg" ):
                    case DUSK_STRING_HASH( "png" ):
                    case DUSK_STRING_HASH( "tiff" ):
                    case DUSK_STRING_HASH( "gif" ):
                        texturesToReload.push( vfsPath );
                        break;

                    case DUSK_STRING_HASH( "mesh" ):
                        meshesToReload.push( vfsPath );
                        break;

                    case DUSK_STRING_HASH( "vso" ):
                    case DUSK_STRING_HASH( ".gl.spvv" ):
                    case DUSK_STRING_HASH( ".vk.spvv" ):
                        dk::core::RemoveWordFromString( vfsPath, DUSK_STRING( "/CompiledShaders/" ) );
                        shadersToReload.push( { WideStringToString( vfsPath ), SHADER_STAGE_VERTEX } );
                        break;

                    case DUSK_STRING_HASH( "pso" ):
                    case DUSK_STRING_HASH( ".gl.spvp" ):
                    case DUSK_STRING_HASH( ".vk.spvp" ):
                        dk::core::RemoveWordFromString( vfsPath, DUSK_STRING( "/CompiledShaders/" ) );
                        shadersToReload.push( { WideStringToString( vfsPath ), SHADER_STAGE_PIXEL } );
                        break;

                    case DUSK_STRING_HASH( "cso" ):
                    case DUSK_STRING_HASH( ".gl.spvc" ):
                    case DUSK_STRING_HASH( ".vk.spvc" ):
                        dk::core::RemoveWordFromString( vfsPath, DUSK_STRING( "/CompiledShaders/" ) );
                        shadersToReload.push( { WideStringToString( vfsPath ), SHADER_STAGE_COMPUTE } );
                        break;
                    }
                }
            }
        }

        memset( Buffer, 0, MAX_BUFFER * sizeof( BYTE ) );
        memset( fileNameModified, 0, FILENAME_MAX * sizeof( WCHAR ) );

        changesResult = ReadDirectoryChangesW( watchdogHandle, Buffer, MAX_BUFFER, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, &dwBytesReturned, 0, 0 );
    }
}
#endif
#endif
