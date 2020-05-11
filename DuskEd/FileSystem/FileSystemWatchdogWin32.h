/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_DEVBUILD
#if DUSK_WIN

#include <atomic>
#include <queue>
#include <thread>

#include <Rendering/RenderDevice.h>

class GraphicsAssetCache;
class ShaderCache;

class FileSystemWatchdog
{
public:
            FileSystemWatchdog();
            FileSystemWatchdog( FileSystemWatchdog& ) = delete;
            ~FileSystemWatchdog();

    void    create();
    void    onFrame( GraphicsAssetCache* graphicsAssetManager, ShaderCache* shaderCache );

private:
    struct ShaderStageToReload
    {
        std::string   Filename;
        eShaderStage  StageType;
    };

private:
    HANDLE              watchdogHandle;
    std::atomic_bool    shutdownSignal;
    std::thread         monitorThread;

    std::queue<dkString_t> materialsToReload;
    std::queue<dkString_t> texturesToReload;
    std::queue<dkString_t> meshesToReload;
    
    std::queue<ShaderStageToReload> shadersToReload;
    
private:
    void    monitor();
};
#endif
#endif
