/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#pragma once

class RenderDevice;
class ShaderCache;
class VirtualFileSystem;

class Material;

struct Image;
struct FontDescriptor;
struct ImageDesc;

class BaseAllocator;
class FreeListAllocator;

#include <unordered_map>
#include <Core/Types.h>

class GraphicsAssetCache
{
public:
                                                GraphicsAssetCache( BaseAllocator* allocator, RenderDevice* renderDevice, ShaderCache* shaderCache, VirtualFileSystem* virtualFileSystem );
                                                GraphicsAssetCache( GraphicsAssetCache& ) = delete;
	                                            ~GraphicsAssetCache();

    Image*                                      getImage( const dkChar_t* assetName, const bool forceReload = false );
    FontDescriptor*                             getFont( const dkChar_t* assetName, const bool forceReload = false );
    Material*                                   getMaterial( const dkChar_t* assetName, const bool forceReload = false );

    // Return the description of an image already loaded in memory.
    // Return null if the image does not exist, or if the image is not present in memory.
    ImageDesc*                                  getImageDescription( const dkChar_t* assetPath );

    Material* getDefaultMaterial() const
    {
        return defaultMaterial;
    }

private:
    FreeListAllocator*                          assetStreamingHeap;

    RenderDevice*                               renderDevice;
    ShaderCache*                                shaderCache;
    VirtualFileSystem*                          virtualFileSystem;

    std::unordered_map<dkStringHash_t, Material*>         materialMap;
    std::unordered_map<dkStringHash_t, Image*>            imageMap;
    std::unordered_map<dkStringHash_t, ImageDesc>         imageDescMap;
    std::unordered_map<dkStringHash_t, FontDescriptor*>   fontMap;

#if DUSK_DEVBUILD
	std::unordered_map<dkStringHash_t, dkString_t>        hashResolveMap;
#endif

    Material*                                           defaultMaterial;
};
