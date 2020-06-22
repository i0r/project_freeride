/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

#include "Shared.h"
#include "GraphicsAssetCache.h"

// Io
#include <FileSystem/VirtualFileSystem.h>
#include <FileSystem/FileSystemObject.h>

#include <Io/DirectDrawSurface.h>
#include <Io/FontDescriptor.h>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#include <Core/Allocators/BaseAllocator.h>
#include <Core/Allocators/FreeListAllocator.h>

// Assets
#include <Rendering/RenderDevice.h>
#include <Core/ViewFormat.h>
#include <Graphics/Material.h>

#include <Core/Hashing/CRC32.h>
#include <Core/StringHelpers.h>

using namespace dk::core;
using namespace dk::io;

int stbi_readcallback( void* user, char* data, int size )
{
    FileSystemObject* f = ( FileSystemObject* )user;
    f->read( ( uint8_t* )data, size );
    return size;
}

int stbi_eofcallback( void* user )
{
    FileSystemObject* f = ( FileSystemObject* )user;
    return ( int )f->isGood();
}

void stbi_skipcallback( void* user, int n )
{
    auto f = ( FileSystemObject* )user;
    f->skip( n );
}

GraphicsAssetCache::GraphicsAssetCache( BaseAllocator* allocator, RenderDevice* renderDevice, ShaderCache* shaderCache, VirtualFileSystem* virtualFileSystem )
    : assetStreamingHeap( dk::core::allocate<FreeListAllocator>( allocator, 32 * 1024 * 1024, allocator->allocate( 32 * 1024 * 1024 ) ) )
    , renderDevice( renderDevice )
    , shaderCache( shaderCache )
    , virtualFileSystem( virtualFileSystem )
{
    defaultMaterial = getMaterial( DUSK_STRING( "GameData/materials/default.mat" ) );
}

GraphicsAssetCache::~GraphicsAssetCache()
{
    /* for ( auto& meshes : meshMap ) {
         meshes.second->destroy( renderDevice );
     }

    for ( auto& mat : materialMap ) {
        mat.second->destroy( renderDevice );
    }*/

    for ( auto& image : imageMap ) {
        renderDevice->destroyImage( image.second );
    }

    materialMap.clear();
    fontMap.clear();
    imageMap.clear();
}

Image* GraphicsAssetCache::getImage( const dkChar_t* assetName, const bool forceReload )
{
    FileSystemObject* file = virtualFileSystem->openFile( assetName, eFileOpenMode::FILE_OPEN_MODE_READ | eFileOpenMode::FILE_OPEN_MODE_BINARY );
    if ( file == nullptr ) {
        DUSK_LOG_ERROR( "'%hs' does not exist!\n", assetName );
        return nullptr;
    }

    dkStringHash_t assetHashcode = file->getHashcode();
    auto mapIterator = imageMap.find( assetHashcode );
    const bool alreadyExists = ( mapIterator != imageMap.end() );

    if ( alreadyExists && !forceReload ) {
        file->close();
        return mapIterator->second;
    }

    auto texFileFormat = GetFileExtensionFromPath( assetName );
    StringToLower( texFileFormat );
    auto texFileFormatHashcode = CRC32( texFileFormat );

    switch ( texFileFormatHashcode ) {
    case DUSK_STRING_HASH( "dds" ): {
        DirectDrawSurface ddsData;
        LoadDirectDrawSurface( file, ddsData );

        if ( alreadyExists ) {
            renderDevice->destroyImage( imageMap[assetHashcode] );
        }

        // Converts ParsedImageDesc to GPU ImageDesc.
        ImageDesc desc;
        if ( ddsData.TextureDescription.ImageDimension == ParsedImageDesc::Dimension::DIMENSION_1D ) {
            desc.dimension = ImageDesc::DIMENSION_1D;
        } else if ( ddsData.TextureDescription.ImageDimension == ParsedImageDesc::Dimension::DIMENSION_2D ) {
            desc.dimension = ImageDesc::DIMENSION_2D;
        } else if ( ddsData.TextureDescription.ImageDimension == ParsedImageDesc::Dimension::DIMENSION_3D ) {
            desc.dimension = ImageDesc::DIMENSION_3D;
        } else {
            desc.dimension = ImageDesc::DIMENSION_UNKNOWN;
        }

        desc.width = ddsData.TextureDescription.Width;
        desc.height = ddsData.TextureDescription.Height;
        desc.depth = ddsData.TextureDescription.Depth;
        desc.arraySize = ddsData.TextureDescription.ArraySize;
        desc.mipCount = Max( 1u, ddsData.TextureDescription.MipCount );
        desc.samplerCount = 1;
        desc.format = ddsData.TextureDescription.Format;
        desc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
        desc.usage = RESOURCE_USAGE_STATIC;

        desc.miscFlags = 0;
        if ( ddsData.TextureDescription.IsCubemap ) {
            desc.miscFlags |= ImageDesc::IS_CUBE_MAP;
        }

        imageMap[assetHashcode] = renderDevice->createImage( desc, ddsData.TextureData.data(), ddsData.TextureData.size() );
        imageDescMap[assetHashcode] = desc;
    } break;

    case DUSK_STRING_HASH( "png16" ):
    case DUSK_STRING_HASH( "hmap" ): {
        stbi_io_callbacks callbacks;
        callbacks.read = stbi_readcallback;
        callbacks.skip = stbi_skipcallback;
        callbacks.eof = stbi_eofcallback;

        int w;
        int h;
        int comp;
        auto* image = stbi_load_16_from_callbacks( &callbacks, file, &w, &h, &comp, STBI_default );

        if ( alreadyExists ) {
            renderDevice->destroyImage( imageMap[assetHashcode] );
        }

        ImageDesc desc;
        desc.width = w;
        desc.height = h;
        desc.dimension = ImageDesc::DIMENSION_2D;
        desc.arraySize = 1;
        desc.mipCount = 1;
        desc.samplerCount = 1;
        desc.depth = 0;
        desc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
        desc.usage = RESOURCE_USAGE_STATIC;

        switch ( comp ) {
        case 1:
            desc.format = eViewFormat::VIEW_FORMAT_R16_UINT;
            break;
        case 2:
            desc.format = eViewFormat::VIEW_FORMAT_R16G16_UINT;
            break;  
        case 3:
        case 4:
            desc.format = eViewFormat::VIEW_FORMAT_R16G16B16A16_UINT;
            break;
        }

        imageMap[assetHashcode] = renderDevice->createImage( desc, image, w * comp );
        imageDescMap[assetHashcode] = desc;

        stbi_image_free( image );
    } break;
    
    case DUSK_STRING_HASH( "jpg" ):
    case DUSK_STRING_HASH( "jpeg" ):
    case DUSK_STRING_HASH( "png" ):
    case DUSK_STRING_HASH( "tga" ):
    case DUSK_STRING_HASH( "bmp" ):
    case DUSK_STRING_HASH( "psd" ):
    case DUSK_STRING_HASH( "gif" ):
    case DUSK_STRING_HASH( "hdr" ): {
        stbi_io_callbacks callbacks;
        callbacks.read = stbi_readcallback;
        callbacks.skip = stbi_skipcallback;
        callbacks.eof = stbi_eofcallback;

        int w;
        int h;
        int comp;
        unsigned char* image = stbi_load_from_callbacks( &callbacks, file, &w, &h, &comp, STBI_default );

        DUSK_DEV_ASSERT( image != nullptr, "STBI failed to load the given image!" );

        if ( alreadyExists ) {
            renderDevice->destroyImage( imageMap[assetHashcode] );
        }

        ImageDesc desc;
        desc.width = w;
        desc.height = h;
        desc.dimension = ImageDesc::DIMENSION_2D;
        desc.arraySize = 1;
        desc.mipCount = 1;
        desc.samplerCount = 1;
        desc.depth = 0;
        desc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
        desc.usage = RESOURCE_USAGE_STATIC;


        switch ( comp ) {
        case 1:
            desc.format = eViewFormat::VIEW_FORMAT_R8_UINT;
            break;
        case 2:
            desc.format = eViewFormat::VIEW_FORMAT_R8G8_UINT;
            break;  
        case 3:
        case 4:
            desc.format = eViewFormat::VIEW_FORMAT_R8G8B8A8_UNORM;
            break;
        }

        imageMap[assetHashcode] = renderDevice->createImage( desc, image, w * comp );
        imageDescMap[assetHashcode] = desc;

        stbi_image_free( image );
    } break;

    default:
        DUSK_RAISE_FATAL_ERROR( false, "Unsupported fileformat with extension %s\n", texFileFormat.c_str() );
        return nullptr;
    }

    file->close();
    renderDevice->setDebugMarker( *imageMap[assetHashcode], assetName );

    return imageMap[assetHashcode];
}

FontDescriptor* GraphicsAssetCache::getFont( const dkChar_t* assetName, const bool forceReload )
{
    auto file = virtualFileSystem->openFile( assetName, eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( file == nullptr ) {
        DUSK_LOG_ERROR( "'%hs' does not exist!\n", assetName );
        return nullptr;
    }

    dkStringHash_t assetHashcode = CRC32( assetName );

    auto mapIterator = fontMap.find( assetHashcode );
    const bool alreadyExists = ( mapIterator != fontMap.end() );

    if ( alreadyExists && !forceReload ) {
        return mapIterator->second;
    }

    if ( !alreadyExists ) {
        fontMap[assetHashcode] = dk::core::allocate<FontDescriptor>( assetStreamingHeap );
    }

    auto font = fontMap[assetHashcode];

    dk::io::LoadFontFile( file, *font );


    return fontMap[assetHashcode];
}

Material* GraphicsAssetCache::getMaterial( const dkChar_t* assetName, const bool forceReload )
{
    auto file = virtualFileSystem->openFile( assetName, eFileOpenMode::FILE_OPEN_MODE_READ );
    if ( file == nullptr ) {
        DUSK_LOG_ERROR( "'%s' does not exist!\n", assetName );
        return defaultMaterial;
    }

    auto assetHashcode = file->getHashcode();
    auto mapIterator = materialMap.find( assetHashcode );
    const bool alreadyExists = ( mapIterator != materialMap.end() );

    if ( alreadyExists && !forceReload ) {
        return mapIterator->second;
    }

    if ( !alreadyExists ) {
        materialMap[assetHashcode] = dk::core::allocate<Material>( assetStreamingHeap, assetStreamingHeap );
    }

    auto materialInstance = materialMap[assetHashcode];
    materialInstance->deserialize( file );

    file->close();

    return materialMap[assetHashcode];
}

ImageDesc* GraphicsAssetCache::getImageDescription( const dkChar_t* assetPath )
{
    // TODO This is slightly stupid to call the VFS simply to retrieve the file hashcode...
    FileSystemObject* file = virtualFileSystem->openFile( assetPath, eFileOpenMode::FILE_OPEN_MODE_READ | eFileOpenMode::FILE_OPEN_MODE_BINARY );
    if ( file == nullptr ) {
        DUSK_LOG_ERROR( "'%hs' does not exist!\n", assetPath );
        return nullptr;
    }

    dkStringHash_t assetHashcode = file->getHashcode();
    file->close();

    auto mapIterator = imageDescMap.find( assetHashcode );
    if ( mapIterator != imageDescMap.end() ) {
        return &mapIterator->second;
    }

    return nullptr;
}
