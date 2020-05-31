/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct TypeAST;

#include <vector>
#include <unordered_map>

// We need this retarded include to get eShaderStage definition...
#include <Rendering/RenderDevice.h>
#include "Lexer.h"

class RenderLibraryGenerator 
{
public:
    struct GeneratedShader {
        // The name of the generated shader (a 128bits hashcode digest).
        std::string     ShaderName;

        // The HLSL source code of this shader (ready to compile).
        std::string     GeneratedSource;

        // The pipeline stage of this shader.
        eShaderStage    ShaderStage;

        GeneratedShader( const eShaderStage stage = eShaderStage::SHADER_STAGE_VERTEX, const char* name = "" )
            : ShaderName( name )
            , GeneratedSource()
            , ShaderStage( stage )
        {
        }
    };

    struct RenderPassInfos {
        std::string                 RenderPassName;
        std::string                 StageShaderNames[eShaderStage::SHADER_STAGE_COUNT]; // Vertex, TEval, TComp, Pixel, Compute
        PipelineStateDesc::Type     PipelineStateType; // Compute or Graphics PSO

        i32 DispatchX;
        i32 DispatchY;
        i32 DispatchZ;

        std::vector<std::string>    ColorRenderTargets;
        std::string                 DepthStencilBuffer;

        RenderPassInfos()
            : RenderPassName( "" )
            , PipelineStateType( PipelineStateDesc::GRAPHICS )
            , DispatchX( 0 )
            , DispatchY( 0 )
            , DispatchZ( 0 )
        {

        }
    };

public:
    DUSK_INLINE const std::string& getGeneratedLibraryName() const { return generatedLibraryName; }
    DUSK_INLINE const std::string& getGeneratedMetadata() const { return generatedMetadata; }
    DUSK_INLINE const std::string& getGeneratedReflection() const { return generatedReflection; }
    DUSK_INLINE const std::vector<GeneratedShader>& getGeneratedShaders() const { return generatedShaders; }
    DUSK_INLINE const std::vector<RenderPassInfos>& getGeneratedRenderPasses() const { return generatedRenderPasses; }

public:
                    RenderLibraryGenerator();
                    ~RenderLibraryGenerator();

    // Generate code from a previously parsed asset. Root is the root node of the parsed asset (the lib node for RenderLibraries).
    // If generateMetadata is true, the generator will write metadata to generatedMetadata.
    // If generateReflection is true, the generator will write reflection infos to generatedReflection.
    void            generateFromAST( const TypeAST& root, const bool generateMetadata = true, const bool generateReflection = false );

private:
    // Name of the RenderLibrary generated.
    std::string                     generatedLibraryName;

    // Generated header for RenderPass binding on the engine side. Holds resources hashcode, shader bindings, etc.
    std::string                     generatedMetadata;

    // Generated header for RenderPass reflection data/runtime edition.
    std::string                     generatedReflection;

    // Array holding the shaders generated from a RenderPass library. A single library can contain several renderpasses
    // so we can't know in advance how many shaders sources we'll have to store.
    std::vector<GeneratedShader>    generatedShaders;
    
    // Array holding the generated renderpasses from a RenderPass Library.
    std::vector<RenderPassInfos>    generatedRenderPasses;

    // Pointer to the AST node holding the library properties. Careful, this pointer is only valid when
    // a generation is in progress.
    TypeAST*                        propertiesNode;

    // Pointer to the AST node holding the library resource list. Careful, this pointer is only valid when
    // a generation is in progress.
    const TypeAST*                  resourceListNode;

    // Non-processed shared HLSL code. Careful, this resource is only valid when a generation is in progress.
    std::string                     sharedShaderBody;

    // Hashmap holding pointers to non-processed shader HLSL code. Key is the shader name hashcode; value is a pointer
    // to shader source code. Careful, this resource is only valid when a generation is in progress.
    std::unordered_map<dkStringHash_t, const Token::StreamRef*> shadersSource;

private:
    // Process a RenderPass node. AstName is the stream reference of this node, astType is the reference to this node content.
    void            processRenderPassNode( const Token::StreamRef& astName, const TypeAST& astType );

    // Generate resource metadata for a given RenderPass.
    void            generateResourceMetadata( RenderPassInfos& passInfos, const std::string& scopedPassName, const std::string& shaderBindingDecl );

    // Process a shader stage for a given RenderPass.
    void            processShaderStage( const i32 stageIndex, const std::string scopedPassName, RenderPassInfos& passInfos, std::string& shaderBindingDecl );

    // Clear string builders and previously generated stuff.
    void            resetGeneratedContent();

    // Append shared code to the input stringbuilder (shared cbuffers, constants, etc.).
    void            appendSharedShaderHeader( std::string& hlslSource ) const;
};