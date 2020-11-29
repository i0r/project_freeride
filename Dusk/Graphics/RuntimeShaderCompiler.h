/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class VirtualFileSystem;
class RuntimeIncludeSM5;
class RuntimeIncludeSM6;
 
#include <Rendering/RenderDevice.h>

// TODO We may want to make those flags modulable.
#ifdef DUSK_WIN
// Shader Model 5 is only supported on Windows (fxc isn't portable to unix + no point using sm5 on Unix).
#define DUSK_SUPPORT_SM5_COMPILATION 1
#endif

// For now we'll use dxc to compile from hlsl to spirv/dx bytecode (sm6). Might worth checking spirv-cross parser/compiler if
// unix support is bad.
#ifdef DUSK_USE_DIRECTX_COMPILER
#define DUSK_SUPPORT_SM6_COMPILATION 1
#define DUSK_SUPPORT_SPIRV_COMPILATION 1
#endif

#ifdef DUSK_USE_DIRECTX_COMPILER
#if DUSK_WIN
#include <d3dcompiler.h>
#endif

#include <ThirdParty/dxc/include/dxc/dxcapi.use.h>

struct IDxcLibrary;
struct IDxcCompiler;
#endif

class RuntimeShaderCompiler {
public:
    // RAI object holding shader bytecode (for a given shader model).
    struct GeneratedBytecode 
    {
        GeneratedBytecode( BaseAllocator* blobAllocator, u8* bytecode, const size_t bytecodeLength )
            : memoryAllocator( blobAllocator )
            , Bytecode( bytecode )
            , Length( bytecodeLength )
        {

        }
        
        ~GeneratedBytecode()
        {
            if ( Bytecode != nullptr ) {
                dk::core::freeArray( memoryAllocator, Bytecode );
            }

            Bytecode = nullptr;
            memoryAllocator = nullptr;
        }

        // Pointer to the generated bytecode buffer (its format depends on the shading model requested).
        u8* Bytecode;

        // Length of Bytecode (in bytes).
        const size_t Length;
    
    private:
        // The allocator owning the Bytecode array.
        BaseAllocator* memoryAllocator;
    };

public:
    static constexpr const dkChar_t* SHADER_DUMP_EXT_SPIRV = DUSK_STRING( ".spirv.hlsl" );
    static constexpr const dkChar_t* SHADER_DUMP_EXT_SM5 = DUSK_STRING( ".sm5.hlsl" );
    static constexpr const dkChar_t* SHADER_DUMP_EXT_SM6 = DUSK_STRING( ".sm6.hlsl" );

public:
                            RuntimeShaderCompiler( BaseAllocator* allocator, VirtualFileSystem* vfs );
                            ~RuntimeShaderCompiler();


    // Helper function for saving shader binaries to the hdd. 'shaderFolder' is the path to the folder which will contain shader binaries; 'compiledShader' is the generated bytecode; 'shaderHashcode' the parsed shader code/metadata.
    static void             SaveToDisk( VirtualFileSystem* virtualFileSystem, const dkString_t& shaderFolder, const RuntimeShaderCompiler::GeneratedBytecode& compiledShader, const std::string& shaderFilename );

    // Remove shader dump from previously failed shader compilation. Should be called if the latest compilation result is successful (the previous compilation result does not matter).
    static void             ClearShaderDump( VirtualFileSystem* virtualFileSystem, FileSystem* fileSystem, const char* shaderName, const dkChar_t* extension );

#ifdef DUSK_SUPPORT_SM5_COMPILATION
    // Compile HLSL code to a blob with SM5.0 bytecode.
    // If the compilation failed, the Bytecode pointer will be null with a Length of 0.
    // ShaderName is an extra parameter used for logging/debugging (can be duplicated/null if you don't need shader dump).
    GeneratedBytecode       compileShaderModel5( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName );
#endif

#ifdef DUSK_USE_DIRECTX_COMPILER
    // Compile HLSL code to a blob with SM6.0 bytecode (D3D12 bytecode).
    // If the compilation failed, the Bytecode pointer will be null with a Length of 0.
    // ShaderName is an extra parameter used for logging/debugging (can be duplicated/null if you don't need shader dump).
    RuntimeShaderCompiler::GeneratedBytecode    compileShaderModel6( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName );

    // Compile HLSL code to a blob with SM6.0 bytecode (Spirv bytecode).
    // If the compilation failed, the Bytecode pointer will be null with a Length of 0.
    // ShaderName is an extra parameter used for logging/debugging (can be duplicated/null if you don't need shader dump).
    RuntimeShaderCompiler::GeneratedBytecode    compileShaderModel6Spirv( const eShaderStage shaderStage, const char* sourceCode, const size_t sourceCodeLength, const char* shaderName );
#endif

private:
    // The memory allocator owning this instance.
    BaseAllocator*          memoryAllocator;

#if DUSK_SUPPORT_SM5_COMPILATION
    // Helper to resolve HLSL include directives (when using D3DCompile).
    RuntimeIncludeSM5*         runtimeInclude;
#endif

    // A pointer to the active Virtual File System instance.
    VirtualFileSystem*      virtualFileSystem;

#if DUSK_USE_DIRECTX_COMPILER
    // Class helper to load DXC library (used for dxbytecode generation).
    dxc::DxcDllSupport          dxcHelper;

    // DXC library instance.
    IDxcLibrary*                dxcLibrary;

    // DXC compiler instance.
    IDxcCompiler*               dxcCompiler;

    // Helper to resolve HLSL include directives (when using DXC).
    RuntimeIncludeSM6*          runtimeIncludeSM6;
#endif
};
