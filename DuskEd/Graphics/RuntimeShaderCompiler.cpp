/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "RuntimeShaderCompiler.h"

#include <FileSystem/VirtualFileSystem.h>

#include <d3dcompiler.h>

#include <Core/StringHelpers.h>
#include <Io/TextStreamHelpers.h>

DUSK_DEV_VAR( DumpFailedShaders, "If true, dump the shader source code to the working directory in a text file.", true, bool );

class RuntimeInclude : public ID3DInclude {
public:
    RuntimeInclude( VirtualFileSystem* vfs )
        : ID3DInclude()
        , virtualFileSystem( vfs )
    {

    }

private:
    HRESULT Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes ) override
    {
        dkString_t filePath = dkString_t( DUSK_STRING( "EditorAssets/RenderPasses/Headers/" ) ) + StringToWideString( pFileName );
        FileSystemObject* file = virtualFileSystem->openFile( filePath.c_str(), eFileOpenMode::FILE_OPEN_MODE_READ );

        if ( file == nullptr || !file->isGood() ) {
            return S_FALSE;
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

    HRESULT Close( LPCVOID pData ) override
    {
        std::free( const_cast< void* >( pData ) );
        return S_OK;
    }

private:
    VirtualFileSystem* virtualFileSystem;
};

// Return the Shading Model 5 target corresponding to a given shader stage.
const char* GetSM5StageTarget( const eShaderStage shaderStage )
{
    switch ( shaderStage ) {
    case SHADER_STAGE_VERTEX:
        return "vs_5_0";
    case SHADER_STAGE_PIXEL:
        return "ps_5_0";
    case SHADER_STAGE_TESSELATION_CONTROL:
        return "ds_5_0";
    case SHADER_STAGE_TESSELATION_EVALUATION:
        return "hs_5_0";
    case SHADER_STAGE_COMPUTE:
        return "cs_5_0";
    }

    return "";
}

RuntimeShaderCompiler::RuntimeShaderCompiler( BaseAllocator* allocator, VirtualFileSystem* vfs )
    : memoryAllocator( allocator )
    , runtimeInclude( dk::core::allocate<RuntimeInclude>( allocator, vfs ) )
    , virtualFileSystem( vfs )
{

}

RuntimeShaderCompiler::~RuntimeShaderCompiler()
{
    dk::core::free( memoryAllocator, runtimeInclude );

    memoryAllocator = nullptr;
    virtualFileSystem = nullptr;
}

RuntimeShaderCompiler::GeneratedBytecode RuntimeShaderCompiler::compileShaderModel5( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName )
{
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    const char* modelTarget = GetSM5StageTarget( shaderStage );

    HRESULT result = D3DCompile( sourceCode, sourceCodeLength, NULL, NULL, runtimeInclude, "EntryPoint", modelTarget, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errorBlob );
    if ( FAILED( result ) ) {
        DUSK_LOG_ERROR( "'%hs' : Compilation Failed!\n%hs\n", shaderName, errorBlob->GetBufferPointer() );
       
        if ( DumpFailedShaders ) {
            dkString_t dumpFile = DUSK_STRING( "GameData/failed_shaders/" ) + StringToWideString( shaderName ) + DUSK_STRING( ".hlsl" );
			FileSystemObject* dumpStream = virtualFileSystem->openFile( dumpFile.c_str(), eFileOpenMode::FILE_OPEN_MODE_WRITE | eFileOpenMode::FILE_OPEN_MODE_BINARY );
			if ( dumpStream->isGood() ) {
                dumpStream->writeString( "/***********\n" );
				dumpStream->write( static_cast<u8*>( errorBlob->GetBufferPointer() ), errorBlob->GetBufferSize() - 1 );
				dumpStream->writeString( "***********/\n\n" );
                dumpStream->writeString( sourceCode, sourceCodeLength );
                dumpStream->close();
			}
        }

        if ( errorBlob != nullptr ) {
            errorBlob->Release();
        }

        if ( shaderBlob != nullptr ) {
            shaderBlob->Release();
        }

        return GeneratedBytecode( memoryAllocator, nullptr, 0ull );
    }

    DUSK_LOG_DEBUG( "'%hs' : Compilation Suceeded!\n", shaderName );

    ID3DBlob* StrippedBlob = nullptr;
    D3DStripShader( shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_PRIVATE_DATA | D3DCOMPILER_STRIP_ROOT_SIGNATURE, &StrippedBlob );

    size_t bytecodeLength = shaderBlob->GetBufferSize();
    u8* bytecodeArray = dk::core::allocateArray<u8>( memoryAllocator, bytecodeLength );
    memcpy( bytecodeArray, shaderBlob->GetBufferPointer(), bytecodeLength );

    shaderBlob->Release();

    return GeneratedBytecode( memoryAllocator, bytecodeArray, bytecodeLength );
}
