/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Baker.h"

#include "Core/Environment.h"
#include "Core/CommandLineArgs.h"
#include "Core/Timer.h"
#include "Core/Logger.h"

#include "Core/Allocators/AllocationHelpers.h"
#include "Core/Allocators/LinearAllocator.h"

#include <Lexer.h>
#include <Parser.h>
#include <RenderPassGenerator.h>

#include "Io/TextStreamHelpers.h"

#include "FileSystem/FileSystemNative.h"

#include <d3dcompiler.h>
#include <fstream>

#if DUSK_WIN
#include <FileSystem/FileSystemWin32.h>
#elif DUSK_UNIX
#include <FileSystem/FileSystemUnix.h>
#endif

#include <Rendering/RenderDevice.h>

static char         g_BaseBuffer[128];
static void*        g_AllocatedTable;
static LinearAllocator* g_GlobalAllocator;
static dkString_t g_AssetsPath;
static dkString_t g_GeneratedHeaderPath;
static FileSystemNative* g_DataFS;

void Initialize()
{
    Timer profileTimer;
    profileTimer.start();

    DUSK_LOG_RAW( "================================\nDusk Baker %s\n%hs\nCompiled with: %s\n================================\n\n", DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    g_AllocatedTable = dk::core::malloc( 1024 << 20 );
    g_GlobalAllocator = new ( g_BaseBuffer ) LinearAllocator( 1024 << 20, g_AllocatedTable );

    DUSK_LOG_INFO( "Global memory table allocated at: 0x%x\n", g_AllocatedTable );

    DUSK_LOG_INFO( "Initialization done (took %.5f seconds)\n", profileTimer.getElapsedTimeAsSeconds() );
    DUSK_LOG_RAW( "\n================================\n\n" );
}

class BakerInclude : public ID3DInclude
{
    HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
    {
        std::wstring filePath = g_AssetsPath + DUSK_STRING( "Headers/" ) + StringToWideString( pFileName );
        FileSystemObject* file = g_DataFS->openFile( filePath.c_str() );

        if ( file == nullptr || !file->isGood() ) {
            std::wstring filePath = g_GeneratedHeaderPath + DUSK_STRING( "/../../RenderPasses/Headers/" ) + StringToWideString( pFileName );
            file = g_DataFS->openFile( filePath.c_str() );
        }

        std::string content;
        dk::io::LoadTextFile( file, content );
        file->close();
        dk::core::RemoveNullCharacters( content );

        *pBytes = static_cast< UINT >( content.size() );
        unsigned char* data = ( unsigned char* )( std::malloc( content.size() ) );
        memcpy( data, content.data(), content.size() * sizeof( unsigned char ) );
        *ppData = data;
        return S_OK;
    }

    HRESULT Close(LPCVOID pData) override
    {
        std::free(const_cast<void*>(pData));
        return S_OK;
    }
};

void dk::baker::Start( const char* cmdLineArgs )
{
    Initialize();

    // Parse cmdlist arguments for assets baking
    DUSK_LOG_INFO( "Parsing command line arguments ('%s')\n", StringToWideString( cmdLineArgs ).c_str() );

    std::string compiledShadersPath;
    std::string generatedHeadersPath;
    std::string generatedReflectionHeadersPath;
    std::string assetsPath;

    char* path = strtok( const_cast< char* >( cmdLineArgs ), " " );
    if ( path != nullptr ) {
        assetsPath = std::string( path );
    }

    path = strtok( nullptr, " " );
    if ( path != nullptr ) {
        compiledShadersPath = std::string( path );
    }

    path = strtok( nullptr, " " );
    if ( path != nullptr ) {
        generatedHeadersPath = std::string( path );
    }

    path = strtok( nullptr, " " );
    if ( path != nullptr ) {
        generatedReflectionHeadersPath = std::string( path );
    }

    if ( assetsPath.empty() || compiledShadersPath.empty() || generatedHeadersPath.empty() ) {
        DUSK_LOG_ERROR( "Usage: DuskBaker [(IN)DRPL_PATH] [(OUT)PATH_TO_COMPILED_SHADERS] [(OUT)PATH_TO_GENERATED_HEADERS] [(OUT)PATH_TO_GENERATED_REFLECTION_HEADERS]" );
        return;
    }

    g_AssetsPath = StringToWideString( assetsPath );
    g_GeneratedHeaderPath = StringToWideString( generatedHeadersPath );

    std::vector<dkString_t> renderLibs;
    dk::core::GetFilesByExtension( g_AssetsPath, DUSK_STRING( "*.drpl*" ), renderLibs );

    g_DataFS = dk::core::allocate<FileSystemNative>( g_GlobalAllocator, g_AssetsPath );

	dkString_t compileShaderPathWide = StringToWideString( compiledShadersPath );
	dk::core::CreateFolderImpl( compileShaderPathWide );
    dk::core::CreateFolderImpl( compileShaderPathWide + DUSK_STRING( "/sm5/" ) );
    dk::core::CreateFolderImpl( compileShaderPathWide + DUSK_STRING( "/sm6/" ) );
    dk::core::CreateFolderImpl( compileShaderPathWide + DUSK_STRING( "/spirv/" ) );

    // Search for every drpl (DuskRenderPassLibrary)
    //  for each drpl, parse its content and generate the library
    // Search for every fbx/obj (geometry)
    //  for each file, build its dgp (DuskGeometryPrimitive); see paper for specs
    // Search for every bitmap
    //  for each bitmap, compute its compressed version (aka DDS)
    // Precompute resources related to rendering (LUT etc.)
    // Search every dmat (DuskMATerial)
    //  for each material, compute its baked version (and precompute default PSO?)
    for ( dkString_t& renderLib : renderLibs ) {
        DUSK_LOG_INFO( "%s\n", renderLib.c_str() );

        FileSystemObject* file = g_DataFS->openFile( renderLib.c_str() );

        std::string assetStr;
        dk::io::LoadTextFile( file, assetStr );

        Parser parser( assetStr.c_str() );
        parser.generateAST();

        const u32 typeCount = parser.getTypeCount();
        for ( u32 t = 0; t < typeCount; t++ ) {
            const TypeAST& typeAST = parser.getType( t );
            switch ( typeAST.Type ) {
            case TypeAST::LIBRARY:
            {
                RenderLibraryGenerator renderLibGenerator;
                renderLibGenerator.generateFromAST( typeAST, true, true );

                const std::string& libraryHeader = renderLibGenerator.getGeneratedMetadata();
                const std::string& libraryName = renderLibGenerator.getGeneratedLibraryName();
                const std::string& libraryReflection = renderLibGenerator.getGeneratedReflection();
                const std::vector<RenderLibraryGenerator::GeneratedShader>& libraryShaders = renderLibGenerator.getGeneratedShaders();

                std::ofstream headerStream( generatedHeadersPath + "/" + libraryName + ".generated.h" );
                headerStream << libraryHeader;
                headerStream.close();

                if ( !generatedReflectionHeadersPath.empty() && !libraryReflection.empty() ) {
                    std::ofstream reflectionHeaderStream( generatedReflectionHeadersPath + "/" + libraryName + ".reflected.h" );
                    reflectionHeaderStream << libraryReflection;
                    reflectionHeaderStream.close();
                }

                BakerInclude include;
                for ( const RenderLibraryGenerator::GeneratedShader& shader : libraryShaders ) {
                    // TODO Compile for each Graphics API (only works for shader model 5.0 atm)
                    // Use DXC for Shader model 6.0; use spirv-cross for SPIRV bytecode
                    ID3DBlob* shaderBlob = nullptr;
                    ID3DBlob* errorBlob = nullptr;

                    std::string profile;
                    if ( shader.ShaderStage & eShaderStage::SHADER_STAGE_VERTEX )
                        profile = "vs_5_0";
                    else if ( shader.ShaderStage & eShaderStage::SHADER_STAGE_TESSELATION_CONTROL )
                        profile = "ds_5_0";
                    else if ( shader.ShaderStage & eShaderStage::SHADER_STAGE_TESSELATION_EVALUATION )
                        profile = "hs_5_0";
                    else if ( shader.ShaderStage & eShaderStage::SHADER_STAGE_PIXEL )
                        profile = "ps_5_0";
                    else if ( shader.ShaderStage & eShaderStage::SHADER_STAGE_COMPUTE )
                        profile = "cs_5_0";

                    HRESULT result = D3DCompile( shader.GeneratedSource.c_str(), shader.GeneratedSource.size(), NULL, NULL,
                             &include, "EntryPoint", profile.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errorBlob );
                   
                    if ( FAILED( result ) ) {
                        DUSK_LOG_ERROR( "%hs\n", std::string( ( char* )errorBlob->GetBufferPointer(), errorBlob->GetBufferSize() ).c_str() );
                        DUSK_LOG_ERROR( "%hs\n", shader.GeneratedSource.c_str() );
                        continue;
                    }

                    ID3DBlob* StrippedBlob = nullptr;
                    D3DStripShader( shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_PRIVATE_DATA | D3DCOMPILER_STRIP_ROOT_SIGNATURE, &StrippedBlob );
                    
                    //DUSK_LOG_DEBUG( "%hs\n", shader.GeneratedSource.c_str() );

                    std::ofstream shaderStrean( compiledShadersPath + "/sm5/" + shader.Hashcode, std::ios::binary | std::ios::trunc );
                    shaderStrean.write( (const char*)shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize() );
                    shaderStrean.close();
                }
            } break;
            default:
                break;
            }
        }
        file->close();
    }

    DUSK_LOG_INFO( "Freeing allocated memory...\n" );

    g_GlobalAllocator->clear();
    g_GlobalAllocator->~LinearAllocator();
    dk::core::free( g_AllocatedTable );
}
