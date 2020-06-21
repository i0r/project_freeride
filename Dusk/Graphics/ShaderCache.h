/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <unordered_map>
#include <atomic>

#include <Rendering/RenderDevice.h>

class FileSystemObject;
class VirtualFileSystem;
class BaseAllocator;

struct Shader;

class ShaderCache
{
public:
                            ShaderCache( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVFS );
                            ShaderCache( ShaderCache& ) = delete;
                            ShaderCache& operator = ( ShaderCache& ) = delete;
                            ~ShaderCache();
	                        
    template<eShaderStage stageType>
    Shader*                 getOrUploadStage( const dkChar_t* shaderHashcode, const bool forceReload = false );

    // Compute the hashcode for a runtime generated permutation name and return the stage associated
    template<eShaderStage stageType>
    Shader*                 getOrUploadStageDynamic( const char* shadernameWithPermutationFlags, const bool forceReload = false );


private:
    VirtualFileSystem*                              virtualFileSystem;
    RenderDevice*                                   renderDevice;
    BaseAllocator*                                  memoryAllocator;

	std::unordered_map<dkStringHash_t, Shader*>	    cachedStages;

    // True if a thread is currently acessing the cache; false otherwise.
    std::atomic<bool>                               cacheLock;

    // Fallbacks incase of missing shader
    Shader*                                         defaultVertexStage;
    Shader*                                         defaultPixelStage;
    Shader*                                         defaultComputeStage;

private:
    // Return true if cache access is safe (no thread is trying to read/write from the cache); false otherwise.
    // Can be used for spinlock call.
    bool                    canAccessCache();
};
