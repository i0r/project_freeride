/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "ShaderCache.h"

#include <Core/Hashing/MurmurHash3.h>
#include <Core/Hashing/Helpers.h>

#include <Rendering/RenderDevice.h>

#include <FileSystem/FileSystem.h>
#include <FileSystem/VirtualFileSystem.h>
#include <FileSystem/FileSystemObject.h>

#include <Io/Binary.h>

ShaderCache::ShaderCache( BaseAllocator* allocator, RenderDevice* activeRenderDevice, VirtualFileSystem* activeVFS )
    : virtualFileSystem( activeVFS )
    , renderDevice( activeRenderDevice )
    , memoryAllocator( allocator )
    , cacheLock( false )
{
    DUSK_LOG_INFO( "Loading default shaders...\n" );

    defaultVertexStage = getOrUploadStageDynamic<SHADER_STAGE_VERTEX>( "Error" );
    defaultPixelStage = getOrUploadStageDynamic<SHADER_STAGE_PIXEL>( "Error" );
    defaultComputeStage = getOrUploadStageDynamic<SHADER_STAGE_COMPUTE>( "Error" );
}

ShaderCache::~ShaderCache()
{
    while ( !canAccessCache() );

    for ( auto& shader : cachedStages ) {
        renderDevice->destroyShader( shader.second );
    }
    cachedStages.clear();

    defaultVertexStage = nullptr;
    defaultPixelStage = nullptr;
    defaultComputeStage = nullptr;

    virtualFileSystem = nullptr;
    renderDevice = nullptr;
    memoryAllocator = nullptr;
}

template<eShaderStage stageType>
Shader* ShaderCache::getOrUploadStage( const dkChar_t* shaderHashcode, const bool forceReload )
{
#if DUSK_VULKAN
    static constexpr const dkChar_t* SHADER_MODEL_PATH = DUSK_STRING( "GameData/shaders/spirv/" );
#elif DUSK_D3D11
    static constexpr const dkChar_t* SHADER_MODEL_PATH = DUSK_STRING( "GameData/shaders/sm5/" );
#elif DUSK_D3D12
    static constexpr const dkChar_t* SHADER_MODEL_PATH = DUSK_STRING( "GameData/shaders/sm6/" );
#endif

    dkString_t filePath = SHADER_MODEL_PATH;
    filePath.append( shaderHashcode );

    FileSystemObject* file = virtualFileSystem->openFile( filePath, eFileOpenMode::FILE_OPEN_MODE_READ | eFileOpenMode::FILE_OPEN_MODE_BINARY );
    if ( file == nullptr ) {
        DUSK_LOG_ERROR( "'%s': file does not exist!\n", shaderHashcode );

        // Returns shader fallback
        switch ( stageType ) {
        case eShaderStage::SHADER_STAGE_VERTEX:
            return defaultVertexStage;
        case eShaderStage::SHADER_STAGE_PIXEL:
            return defaultPixelStage;
        case eShaderStage::SHADER_STAGE_COMPUTE:
            return defaultComputeStage;
        case eShaderStage::SHADER_STAGE_TESSELATION_CONTROL:
        case eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION:
        default:
            return nullptr;
        }
    }

    const dkStringHash_t fileHashcode = file->getHashcode();

    while ( !canAccessCache() );

    cacheLock.store( true );
    auto it = cachedStages.find( fileHashcode );
    if ( it != cachedStages.end() ) {
        if ( !forceReload ) {
            file->close();
            cacheLock.store( false );
            return it->second;
        } else {
            // If asked for force reloading, destroy previously cached shader instance
            renderDevice->destroyShader( it->second );
        }
    }

    // Load precompiled shader
    {
        std::vector<uint8_t> precompiledShader;
        dk::io::LoadBinaryFile( file, precompiledShader );
        file->close();

        cachedStages[fileHashcode] = renderDevice->createShader( stageType, precompiledShader.data(), precompiledShader.size() );
    }

    Shader* cachedStage = cachedStages[fileHashcode];
    cacheLock.store( false );

    return cachedStage;
}

template<eShaderStage stageType>
Shader* ShaderCache::getOrUploadStageDynamic( const char* shadernameWithPermutationFlags, const bool forceReload )
{
    std::string permutationFullname = shadernameWithPermutationFlags;

    switch ( stageType ) {
    case SHADER_STAGE_VERTEX:
        permutationFullname.append( "vertex" );
        break;
    case SHADER_STAGE_TESSELATION_CONTROL:
        permutationFullname.append( "tesselationControl" );
        break;
    case SHADER_STAGE_TESSELATION_EVALUATION:
        permutationFullname.append( "tesselationEvaluation" );
        break;
    case SHADER_STAGE_PIXEL:
        permutationFullname.append( "pixel" );
        break;
    case SHADER_STAGE_COMPUTE:
        permutationFullname.append( "compute" );
        break;
    };

    Hash128 permutationHashcode;
    MurmurHash3_x64_128( permutationFullname.c_str(), static_cast< i32 >( permutationFullname.size() ), 19081996, &permutationHashcode );
    dkString_t filenameWithExtension = dk::core::GetHashcodeDigest128( permutationHashcode );

    return getOrUploadStage<stageType>( filenameWithExtension.c_str(), forceReload );
}

bool ShaderCache::canAccessCache()
{
    bool expected = false;
    return cacheLock.compare_exchange_weak( expected, false );
}
