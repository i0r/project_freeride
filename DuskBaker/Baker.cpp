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

#include "Core/Hashing/MurmurHash3.h"
#include "Core/Hashing/HashingSeeds.h"

#include <Core/Lexer.h>
#include <Core/Parser.h>
#include <Core/RenderPassGenerator.h>

#include "Io/TextStreamHelpers.h"

#include "FileSystem/VirtualFileSystem.h"
#include "FileSystem/FileSystemNative.h"

#include "Graphics/RuntimeShaderCompiler.h"

#include "BakerCache.h"

#include <fstream>

#if DUSK_WIN
#include <FileSystem/FileSystemWin32.h>
#elif DUSK_UNIX
#include <FileSystem/FileSystemUnix.h>
#endif

#include <Rendering/RenderDevice.h>

static constexpr const char*    FAILED_SHADER_DUMP_FOLDER = "./failed_shaders/";

// Data structure holding output/input paths for the baking process.
struct BakingPaths
{
    // Path to the folder storing shader binaries.
	std::string CompiledShadersPath;

    // Path to the folder storing metadata headers.
	std::string GeneratedHeadersPath;

    // Path to the folder storing reflection headers. This is an optional feature so this 
    // path might/can be empty.
	std::string GeneratedReflectionHeadersPath;

    // Path to the folder storing user-made assets.
	std::string AssetsPath;

    // Return true if the given BakingPaths is valid (i.e. can be used to execute the baking),
    // false otherwise. Note that this function is static since we want to keep BakingPaths POD.
    static bool IsValid( BakingPaths& paths )
    {
        return ( !paths.AssetsPath.empty() 
              && !paths.CompiledShadersPath.empty() 
              && !paths.GeneratedHeadersPath.empty() );
    }
};

// Parse arguments for assets baking and return a BakingPaths object
// holding paths required for the baking process.
BakingPaths CreateBakingPathsFromCmdLine( const char* cmdLineArgs )
{
	DUSK_LOG_INFO( "Parsing command line arguments ('%hs')\n", cmdLineArgs );

    BakingPaths pathOutput;
	char* path = strtok( const_cast< char* >( cmdLineArgs ), " " );
	if ( path != nullptr ) {
        pathOutput.AssetsPath = std::string( path );
	}

	path = strtok( nullptr, " " );
	if ( path != nullptr ) {
        pathOutput.CompiledShadersPath = std::string( path );
	}

	path = strtok( nullptr, " " );
	if ( path != nullptr ) {
        pathOutput.GeneratedHeadersPath = std::string( path );
	}

	path = strtok( nullptr, " " );
	if ( path != nullptr ) {
        pathOutput.GeneratedReflectionHeadersPath = std::string( path );
	}

    return pathOutput;
}

void dk::baker::Start( const char* cmdLineArgs )
{
	static char BaseBuffer[128];

	DUSK_LOG_RAW( "================================\nDusk Baker %s\n%hs\nCompiled with: %s\n================================\n\n", DUSK_BUILD, DUSK_BUILD_DATE, DUSK_COMPILER );

    void* allocatedTable = dk::core::malloc( 1024 << 20 );
    LinearAllocator* globalAllocator = new ( BaseBuffer ) LinearAllocator( 1024 << 20, allocatedTable );

    // Parse cmdlist arguments for assets baking
    BakingPaths bakingPaths = CreateBakingPathsFromCmdLine( cmdLineArgs );
	if ( !BakingPaths::IsValid( bakingPaths ) ) {
		DUSK_LOG_ERROR( "Usage: DuskBaker [(IN)DRPL_PATH] [(OUT)PATH_TO_COMPILED_SHADERS] [(OUT)PATH_TO_GENERATED_HEADERS] [(OUT (OPTIONAL))PATH_TO_GENERATED_REFLECTION_HEADERS]" );
        return;
    }

    dkString_t assetsPath = StringToDuskString( bakingPaths.AssetsPath.c_str() );
    dkString_t generatedHeadersPath = StringToDuskString( bakingPaths.GeneratedHeadersPath.c_str() );

	DUSK_LOG_INFO( "Assets Path: %hs\n Generated Headers Path: %hs\n", bakingPaths.AssetsPath.c_str(), bakingPaths.GeneratedHeadersPath.c_str() );

    VirtualFileSystem* virtualFileSystem = dk::core::allocate<VirtualFileSystem>( globalAllocator );
    FileSystemNative* dataFS = dk::core::allocate<FileSystemNative>( globalAllocator, generatedHeadersPath + DUSK_STRING( "/../../" ) );
    virtualFileSystem->mount( dataFS, DUSK_STRING( "EditorAssets" ), 1 );

    RuntimeShaderCompiler* runtimeShaderCompiler = dk::core::allocate<RuntimeShaderCompiler>( globalAllocator, globalAllocator, virtualFileSystem );

    dkString_t compiledShadersPath = StringToDuskString( bakingPaths.CompiledShadersPath.c_str() );
	dk::core::CreateFolderImpl( compiledShadersPath );
    dk::core::CreateFolderImpl( compiledShadersPath + DUSK_STRING( "/sm5/" ) );
    dk::core::CreateFolderImpl( compiledShadersPath + DUSK_STRING( "/sm6/" ) );
	dk::core::CreateFolderImpl( compiledShadersPath + DUSK_STRING( "/spirv/" ) );
	dk::core::CreateFolderImpl( DUSK_STRING( "./failed_shaders/" ) );

    FileSystemNative* shaderOutputFS = dk::core::allocate<FileSystemNative>( globalAllocator, compiledShadersPath );
    virtualFileSystem->mount( shaderOutputFS, DUSK_STRING( "ShaderBinOutput" ), 2 );

    // This filesystem is required for logging/shader source code dumping.
    dkString_t workingDir;
    dk::core::RetrieveWorkingDirectory( workingDir );

	FileSystemNative* workingDirFS = dk::core::allocate<FileSystemNative>( globalAllocator, workingDir );
	virtualFileSystem->mount( workingDirFS, DUSK_STRING( "GameData" ), 0 );
    
    // Key is the library hashcode, value is the hash of its content.
    BakerCache cache;
    FileSystemObject* storedCacheStream = workingDirFS->openFile( workingDir + DUSK_STRING( "/baker_cache.bin" ), FILE_OPEN_MODE_READ | FILE_OPEN_MODE_BINARY );
    if ( storedCacheStream != nullptr ) {
        cache.load( storedCacheStream );
        storedCacheStream->close();
    }

	// Build renderlibs list.
	std::vector<dkString_t> renderLibs;
    dk::core::GetFilesByExtension( assetsPath, DUSK_STRING( "drpl" ), renderLibs );
    for ( dkString_t& renderLib : renderLibs ) {
        FileSystemObject* file = dataFS->openFile( renderLib.c_str() );
        if ( file == nullptr ) {
            DUSK_LOG_ERROR( "'%s': file does not exist! (check mounted fs)\n", renderLib.c_str() );
            continue;
        }

        std::string assetStr;
        dk::io::LoadTextFile( file, assetStr );

        // Check library dirtiness.
		u32 contentHashcode;
        MurmurHash3_x86_32( assetStr.c_str(), static_cast< i32 >( assetStr.size() ), dk::core::SeedFileSystemObject, &contentHashcode );

        dkStringHash_t fileHashcode = file->getHashcode();
        if ( !cache.isEntryDirty( fileHashcode, contentHashcode ) ) {
            continue;
        }

		bool isLibraryValid = true;

        DUSK_LOG_INFO( "%hs\n", renderLib.c_str() );

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

                std::ofstream headerStream( bakingPaths.GeneratedHeadersPath + "/" + libraryName + ".generated.h" );
                headerStream << libraryHeader;
                headerStream.close();

                if ( !bakingPaths.GeneratedReflectionHeadersPath.empty() && !libraryReflection.empty() ) {
                    std::ofstream reflectionHeaderStream( bakingPaths.GeneratedReflectionHeadersPath + "/" + libraryName + ".reflected.h" );
                    reflectionHeaderStream << libraryReflection;
                    reflectionHeaderStream.close();
                }

                for ( const RenderLibraryGenerator::GeneratedShader& shader : libraryShaders ) {
                    std::string shaderFilename = ( shader.OriginalName + "." + shader.Hashcode );

#ifdef DUSK_SUPPORT_SM5_COMPILATION
                    RuntimeShaderCompiler::GeneratedBytecode compiledShaderSM5 = runtimeShaderCompiler->compileShaderModel5( shader.ShaderStage, shader.GeneratedSource.c_str(), shader.GeneratedSource.size(), shaderFilename.c_str() );
                    RuntimeShaderCompiler::SaveToDisk( virtualFileSystem, DUSK_STRING( "ShaderBinOutput/sm5/" ), compiledShaderSM5, shader.Hashcode ); 
#endif

#ifdef DUSK_SUPPORT_SM6_COMPILATION
                    RuntimeShaderCompiler::GeneratedBytecode compiledShaderSM6 = runtimeShaderCompiler->compileShaderModel6( shader.ShaderStage, shader.GeneratedSource.c_str(), shader.GeneratedSource.size(), shaderFilename.c_str() );
                    RuntimeShaderCompiler::SaveToDisk( virtualFileSystem, DUSK_STRING( "ShaderBinOutput/sm6/" ), compiledShaderSM6, shader.Hashcode );

                    RuntimeShaderCompiler::GeneratedBytecode compiledShaderSpirv = runtimeShaderCompiler->compileShaderModel6Spirv( shader.ShaderStage, shader.GeneratedSource.c_str(), shader.GeneratedSource.size(), shaderFilename.c_str() );
                    RuntimeShaderCompiler::SaveToDisk( virtualFileSystem, DUSK_STRING( "ShaderBinOutput/spirv/" ), compiledShaderSpirv, shader.Hashcode );
 #endif
                }
            } break;
            default:
                break;
            }
		}

		// Update cache only if the compilation was successful.
		if ( isLibraryValid ) {
			cache.updateOrCreateEntry( fileHashcode, contentHashcode );
        }

        file->close();
    }

	DUSK_LOG_INFO( "Updating baker cache...\n" );
	FileSystemObject* outputCacheStream = workingDirFS->openFile( workingDir + DUSK_STRING( "/baker_cache.bin" ), FILE_OPEN_MODE_WRITE | FILE_OPEN_MODE_BINARY );
    if ( outputCacheStream != nullptr ) {
        cache.store( outputCacheStream );
        outputCacheStream->close();
    }

    DUSK_LOG_INFO( "Freeing allocated memory...\n" );

	dk::core::free( globalAllocator, workingDirFS );
	dk::core::free( globalAllocator, virtualFileSystem );

    globalAllocator->clear();
    globalAllocator->~LinearAllocator();
    dk::core::free( allocatedTable );
}
