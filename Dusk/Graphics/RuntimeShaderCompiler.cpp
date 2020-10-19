/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "RuntimeShaderCompiler.h"

#include <FileSystem/VirtualFileSystem.h>

#include <Core/StringHelpers.h>
#include <Io/TextStreamHelpers.h>

DUSK_DEV_VAR( DumpFailedShaders, "If true, dump the shader source code to the working directory in a text file.", true, bool );

#ifdef DUSK_SUPPORT_SM5_COMPILATION
#include <d3dcompiler.h>
#endif

#ifdef DUSK_SUPPORT_SM5_COMPILATION
class RuntimeIncludeSM5 : public ID3DInclude {
public:
    RuntimeIncludeSM5( VirtualFileSystem* vfs )
        : ID3DInclude()
        , virtualFileSystem( vfs )
    {

    }

private:
    HRESULT Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes ) override
    {
        dkString_t filePath = dkString_t( DUSK_STRING( "EditorAssets/ShaderHeaders/" ) ) + StringToWideString( pFileName );
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
#endif

#ifdef DUSK_SUPPORT_SM6_COMPILATION
template<typename... Ts, typename TObject>
HRESULT DoBasicQueryInterface( TObject* self, REFIID iid, void** ppvObject )
{
    if ( ppvObject == nullptr ) return E_POINTER;

    // Support INoMarshal to void GIT shenanigans.
    if ( IsEqualIID( iid, __uuidof( IUnknown ) ) ||
         IsEqualIID( iid, __uuidof( INoMarshal ) ) ) {
        *ppvObject = reinterpret_cast< IUnknown* >( self );
        reinterpret_cast< IUnknown* >( self )->AddRef();
        return S_OK;
    }

    return DoBasicQueryInterface_recurse<TObject, Ts...>( self, iid, ppvObject );
}

template<typename TObject>
HRESULT DoBasicQueryInterface_recurse( TObject* self, REFIID iid, void** ppvObject )
{
    return E_NOINTERFACE;
}
template<typename TObject, typename TInterface, typename... Ts>
HRESULT DoBasicQueryInterface_recurse( TObject* self, REFIID iid, void** ppvObject )
{
    if ( ppvObject == nullptr ) return E_POINTER;
    if ( IsEqualIID( iid, __uuidof( TInterface ) ) ) {
        *( TInterface** )ppvObject = self;
        self->AddRef();
        return S_OK;
    }
    return DoBasicQueryInterface_recurse<TObject, Ts...>( self, iid, ppvObject );
}

#define DXC_MICROCOM_REF_FIELD(m_dwRef)                                        \
  std::atomic<u32> m_dwRef = {0};
#define DXC_MICROCOM_ADDREF_IMPL(m_dwRef)                                      \
  ULONG STDMETHODCALLTYPE AddRef() override {                                  \
    return (ULONG)++m_dwRef;                                                   \
  }
#define DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)                              \
  DXC_MICROCOM_ADDREF_IMPL(m_dwRef)                                            \
  ULONG STDMETHODCALLTYPE Release() override {                                 \
    ULONG result = (ULONG)--m_dwRef;                                           \
    return result;                                                             \
  }

class RuntimeIncludeSM6 : public IDxcIncludeHandler
{
public:
    // Macro atrocity to implement the reference tracking interface (copied straight from Microsoft sample).
    DXC_MICROCOM_REF_FIELD( dwRef )
    DXC_MICROCOM_ADDREF_RELEASE_IMPL( dwRef )

public:
    RuntimeIncludeSM6( VirtualFileSystem* vfs, IDxcLibrary* dxcLib )
        : IDxcIncludeHandler()
        , virtualFileSystem( vfs )
        , dxcLibrary( dxcLib )
    {

    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void** ppvObject ) override
    {
        return DoBasicQueryInterface<IDxcIncludeHandler>( this, iid, ppvObject );
    }

    HRESULT LoadSource( LPCWSTR pFilename, IDxcBlob** ppIncludeSource ) override
    {
        dkString_t filePath = dkString_t( DUSK_STRING( "EditorAssets/ShaderHeaders/" ) ) + pFilename;
        FileSystemObject* file = virtualFileSystem->openFile( filePath.c_str(), eFileOpenMode::FILE_OPEN_MODE_READ );

        if ( file == nullptr || !file->isGood() ) {
            return S_FALSE;
        }

        std::string content;
        dk::io::LoadTextFile( file, content );
        file->close();
        dk::core::RemoveNullCharacters( content );

        HRESULT opResult = dxcLibrary->CreateBlobWithEncodingOnHeapCopy( content.data(), static_cast<UINT32>( content.size() ), CP_UTF8, ( IDxcBlobEncoding** )ppIncludeSource );
        return opResult;
    }

private:
    VirtualFileSystem* virtualFileSystem;
    IDxcLibrary* dxcLibrary;
};

// Return the Shading Model 6 target corresponding to a given shader stage.
// Note that we explicitly use wide chars. since the DXC library is built this
// way.
const wchar_t* GetSM6StageTarget( const eShaderStage shaderStage )
{
    switch ( shaderStage ) {
    case SHADER_STAGE_VERTEX:
        return L"vs_6_0";
    case SHADER_STAGE_PIXEL:
        return L"ps_6_0";
    case SHADER_STAGE_TESSELATION_CONTROL:
        return L"ds_6_0";
    case SHADER_STAGE_TESSELATION_EVALUATION:
        return L"hs_6_0";
    case SHADER_STAGE_COMPUTE:
        return L"cs_6_0";
    }

    return L"";
}
#endif

RuntimeShaderCompiler::RuntimeShaderCompiler( BaseAllocator* allocator, VirtualFileSystem* vfs )
    : memoryAllocator( allocator )
#ifdef DUSK_SUPPORT_SM5_COMPILATION
    , runtimeInclude( dk::core::allocate<RuntimeIncludeSM5>( allocator, vfs ) )
#endif
    , virtualFileSystem( vfs )
{
#ifdef DUSK_SUPPORT_SM6_COMPILATION
    dxcHelper.Initialize();

    HRESULT instanceCreationRes = dxcHelper.CreateInstance( CLSID_DxcLibrary, &dxcLibrary );
    DUSK_ASSERT( instanceCreationRes == S_OK, "DXC lib loading FAILED! (error code 0x%x)", instanceCreationRes )

    dxcHelper.CreateInstance( CLSID_DxcCompiler, &dxcCompiler );

    runtimeIncludeSM6 = dk::core::allocate<RuntimeIncludeSM6>( allocator, vfs, dxcLibrary );
#endif
}

RuntimeShaderCompiler::~RuntimeShaderCompiler()
{
#ifdef DUSK_SUPPORT_SM5_COMPILATION
    dk::core::free( memoryAllocator, runtimeInclude );
#endif

#ifdef DUSK_SUPPORT_SM6_COMPILATION
    dk::core::free( memoryAllocator, runtimeIncludeSM6 );
#endif

    memoryAllocator = nullptr;
    virtualFileSystem = nullptr;
}

#ifdef DUSK_SUPPORT_SM5_COMPILATION
RuntimeShaderCompiler::GeneratedBytecode RuntimeShaderCompiler::compileShaderModel5( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName )
{
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    const char* modelTarget = GetSM5StageTarget( shaderStage );

    HRESULT result = D3DCompile( sourceCode, sourceCodeLength, NULL, NULL, runtimeInclude, "EntryPoint", modelTarget, D3DCOMPILE_OPTIMIZATION_LEVEL2, 0, &shaderBlob, &errorBlob );
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
#endif

#ifdef DUSK_SUPPORT_SM6_COMPILATION
RuntimeShaderCompiler::GeneratedBytecode RuntimeShaderCompiler::compileShaderModel6( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName )
{
    const wchar_t* modelTarget = GetSM6StageTarget( shaderStage );

    IDxcBlobEncoding* blob = NULL;
    dxcLibrary->CreateBlobWithEncodingFromPinned( ( LPBYTE )sourceCode, ( UINT32 )sourceCodeLength, 0, &blob );
    
    const wchar_t* pArgs[] =
    {
        L"-Zpr",			//Row-major matrices
        L"-WX",				//Warnings as errors
        L"-O2",				//Optimization level 2
    };

    
    IDxcOperationResult* shaderBlob = nullptr;
    dxcCompiler->Compile( blob, NULL, DUSK_STRING( "EntryPoint" ), modelTarget, &pArgs[0], sizeof( pArgs ) / sizeof( pArgs[0] ), nullptr, 0, runtimeIncludeSM6, &shaderBlob );

    HRESULT hrCompilation;
    shaderBlob->GetStatus( &hrCompilation );

    if ( FAILED( hrCompilation ) ) {
        IDxcBlobEncoding* printBlob;
        shaderBlob->GetErrorBuffer( &printBlob );

        DUSK_LOG_ERROR( "'%hs' : Compilation Failed!\n%hs\n", shaderName, printBlob->GetBufferPointer() );

        if ( DumpFailedShaders ) {
            dkString_t dumpFile = DUSK_STRING( "GameData/failed_shaders/" ) + StringToWideString( shaderName ) + DUSK_STRING( ".sm6.hlsl" );
            FileSystemObject* dumpStream = virtualFileSystem->openFile( dumpFile.c_str(), eFileOpenMode::FILE_OPEN_MODE_WRITE | eFileOpenMode::FILE_OPEN_MODE_BINARY );
            if ( dumpStream->isGood() ) {
                dumpStream->writeString( "/***********\n" );
                dumpStream->write( static_cast< u8* >( printBlob->GetBufferPointer() ), printBlob->GetBufferSize() - 1 );
                dumpStream->writeString( "***********/\n\n" );
                dumpStream->writeString( sourceCode, sourceCodeLength );
                dumpStream->close();
            }
        }

        if ( printBlob != nullptr ) {
            printBlob->Release();
        }

        if ( shaderBlob != nullptr ) {
            shaderBlob->Release();
        }

        return GeneratedBytecode( memoryAllocator, nullptr, 0ull );
    }

    DUSK_LOG_DEBUG( "'%hs' : Compilation Suceeded!\n", shaderName );

    IDxcBlob* result = nullptr;
    shaderBlob->GetResult( &result );

    size_t bytecodeLength = result->GetBufferSize();
    u8* bytecodeArray = dk::core::allocateArray<u8>( memoryAllocator, bytecodeLength );
    memcpy( bytecodeArray, result->GetBufferPointer(), bytecodeLength );

    result->Release();
    shaderBlob->Release();

    return GeneratedBytecode( memoryAllocator, bytecodeArray, bytecodeLength );
}
#endif
