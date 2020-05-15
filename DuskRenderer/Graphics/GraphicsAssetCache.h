/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#pragma once

class RenderDevice;
class ShaderCache;
class VirtualFileSystem;

class Material;
struct Mesh;

struct Image;
struct FontDescriptor;

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
    Material*                                   getMaterialCopy( const dkChar_t* assetName );
    Material*                                   getMaterial( const dkChar_t* assetName, const bool forceReload = false );
    Mesh*                                       getMesh( const dkChar_t* assetName, const bool forceReload = false );

private:
    FreeListAllocator*                          assetStreamingHeap;

    RenderDevice*                               renderDevice;
    ShaderCache*                                shaderCache;
    VirtualFileSystem*                          virtualFileSystem;

    std::unordered_map<dkStringHash_t, Material*>         materialMap;
    std::unordered_map<dkStringHash_t, Mesh*>             meshMap;
    std::unordered_map<dkStringHash_t, Image*>            imageMap;
    std::unordered_map<dkStringHash_t, FontDescriptor*>   fontMap;

    Material*                                   defaultMaterial;
};
