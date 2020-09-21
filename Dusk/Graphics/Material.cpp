/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Material.h"

#include <Io/TextStreamHelpers.h>
#include <Core/Parser.h>
#include <Rendering/CommandList.h>
#include <Graphics/GraphicsAssetCache.h>

void ParseScenario( Material::RenderScenarioBinding& binding, const TypeAST& node )
{
    u32 subNodeCount = static_cast< u32 >( node.Names.size() );
    for ( u32 i = 0; i < subNodeCount; i++ ) {
        const Token::StreamRef& name = node.Names[i];
        std::string value = std::string( node.Values[i].StreamPointer, node.Values[i].Length );
        const TypeAST& sType = *node.Types[i];

        switch ( sType.Type ) {
        case TypeAST::SHADER_PERMUTATION:
        {
            if ( dk::core::ExpectKeyword( name.StreamPointer, 6, "vertex" ) ) {
                binding.VertexStage = StringToWideString( value );
            } else if ( dk::core::ExpectKeyword( name.StreamPointer, 5, "pixel" ) ) {
                binding.PixelStage = StringToWideString( value );
            }
        } break;
        }
    }

    // Update scenario ShaderBinding.
    binding.PsoShaderBinding.VertexShader = binding.VertexStage.c_str();
    binding.PsoShaderBinding.TesselationControlShader = nullptr;
    binding.PsoShaderBinding.TesselationEvaluationShader = nullptr;
    binding.PsoShaderBinding.PixelShader = binding.PixelStage.c_str();
    binding.PsoShaderBinding.ComputeShader = nullptr;
}

Material::Material( BaseAllocator* allocator )
    : name( "" )
    , isAlphaBlended( false )
    , isDoubleFace( false )
    , enableAlphaToCoverage( false )
    , isAlphaTested( false )
    , isWireframe( false )
    , invalidateCachedStates( false )
    , isShadeless( false )
{

}

Material::~Material()
{

}

void Material::deserialize( FileSystemObject* object )
{
    // Load asset content, parse and tokenize it.
    std::string assetStr;
    dk::io::LoadTextFile( object, assetStr );

    Parser parser( assetStr.c_str() );
    parser.generateAST();

    // Iterate over each node of the AST.
    const u32 typeCount = parser.getTypeCount();
    for ( u32 t = 0; t < typeCount; t++ ) {
        const TypeAST& typeAST = parser.getType( t );
        switch ( typeAST.Type ) {
        case TypeAST::eTypes::MATERIAL: {
            name = std::string( typeAST.Name.StreamPointer, typeAST.Name.Length );

            u32 nodeCount = static_cast<u32>( typeAST.Names.size() );
            for ( u32 nodeIdx = 0; nodeIdx < nodeCount; nodeIdx++ ) {
                const Token::StreamRef& name = typeAST.Names[nodeIdx];

                // If the node is typeless, assume it is a flag.
                if ( typeAST.Types[nodeIdx] == nullptr ) {
                    std::string value = std::string( typeAST.Values[nodeIdx].StreamPointer, typeAST.Values[nodeIdx].Length );

                    if ( dk::core::ExpectKeyword( name.StreamPointer, 7, "version" ) ) {
                        // Will be used in the future.
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 14, "isAlphaBlended" ) ) {
                        isAlphaBlended = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 12, "isDoubleFace" ) ) {
                        isDoubleFace = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 21, "enableAlphaToCoverage" ) ) {
                        enableAlphaToCoverage = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 13, "isAlphaTested" ) ) {
                        isAlphaTested = dk::core::StringToBool( value );
					} else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "isWireframe" ) ) {
                        isWireframe = dk::core::StringToBool( value );
                    } else if ( dk::core::ExpectKeyword( name.StreamPointer, 11, "isShadeless" ) ) {
                        isShadeless = dk::core::StringToBool( value );
                    }
                } else {
                    const TypeAST& type = *typeAST.Types[nodeIdx];

                    switch ( type.Type ) {
                    case TypeAST::RENDER_SCENARIO: {
                        if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Editor_Default_Picking" ) ) {
                            ParseScenario( defaultPickingEditorScenario, type );
                        }

                        if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Default_Picking" ) ) {
                            ParseScenario( defaultPickingScenario, type );
                        }

                        if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Editor_Default" ) ) {
                            ParseScenario( defaultEditorScenario, type );
                        } 

                        if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Default" ) ) {
                            ParseScenario( defaultScenario, type );
						}

						if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Depth_Only" ) ) {
							ParseScenario( depthOnlyScenario, type );
						}

						if ( dk::core::ExpectKeyword( name.StreamPointer, name.Length, "Editor_Depth_Only" ) ) {
							ParseScenario( depthOnlyEditorScenario, type );
						}
                    } break;
                    case TypeAST::MATERIAL_PARAMETER: {
                        const u32 parameterCount = static_cast<u32>( type.Names.size() );

                        for ( u32 i = 0; i < parameterCount; i++ ) {
                            dkStringHash_t key = dk::core::CRC32( type.Names[i].StreamPointer, type.Names[i].Length );
                            std::string value( type.Values[i].StreamPointer, type.Values[i].Length );

                            MutableParameter param;
                            if ( value.front() == '{' && value.back() == '}' ) {
                                param.Type = MutableParameter::ParamType::Float3;
                                param.Float3Value = dk::io::StringTo3DVector( value );
                            } else {
                                param.Type = MutableParameter::ParamType::Texture2D;
                                param.Value = value;
                            }

                            mutableParameters.insert( std::make_pair( key, param ) );
                        }
                    } break;
                    }
                }
            }
        } break;
        }
    }
}

void Material::bindForScenario( const RenderScenario scenario, CommandList* cmdList, PipelineStateCache* psoCache, const u32 samplerCount )
{
    PipelineState* scenarioPso = nullptr;

    switch ( scenario ) {
    case RenderScenario::Default:
    case RenderScenario::Default_Editor:
    case RenderScenario::Default_Picking:
    case RenderScenario::Default_Picking_Editor:
    {
        // TODO Cache the descriptors so that we don't have to recreate those each frame?
        PipelineStateDesc DefaultPipelineState( PipelineStateDesc::GRAPHICS );
        DefaultPipelineState.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        DefaultPipelineState.DepthStencilState.EnableDepthWrite = false;
        DefaultPipelineState.DepthStencilState.EnableDepthTest = true;
        DefaultPipelineState.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_EQUAL;

		DefaultPipelineState.RasterizerState.UseTriangleCCW = true;
        DefaultPipelineState.RasterizerState.CullMode = ( isDoubleFace ) ? eCullMode::CULL_MODE_NONE : eCullMode::CULL_MODE_FRONT;
        DefaultPipelineState.RasterizerState.FillMode = ( isWireframe ) ? eFillMode::FILL_MODE_WIREFRAME : eFillMode::FILL_MODE_SOLID;

        DefaultPipelineState.FramebufferLayout.declareRTV( 0, VIEW_FORMAT_R16G16B16A16_FLOAT );
        DefaultPipelineState.FramebufferLayout.declareDSV( VIEW_FORMAT_D32_FLOAT );

        DefaultPipelineState.samplerCount = samplerCount;
        DefaultPipelineState.InputLayout.Entry[0] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 0, 0, false, "POSITION" };
        DefaultPipelineState.InputLayout.Entry[1] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 1, 0, true, "NORMAL" };
        DefaultPipelineState.InputLayout.Entry[2] = { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 2, 0, true, "TEXCOORD" };
        DefaultPipelineState.depthClearValue = 0.0f;

		DefaultPipelineState.addStaticSampler( RenderingHelpers::S_BilinearWrap );
		DefaultPipelineState.addStaticSampler( RenderingHelpers::S_TrilinearComparisonClampEdge );

        // Retrieve the appropriate shader binding for the given scenario.
        const PipelineStateCache::ShaderBinding& shaderBinding = getScenarioShaderBinding( scenario );

		scenarioPso = psoCache->getOrCreatePipelineState( DefaultPipelineState, shaderBinding, invalidateCachedStates );
    } break;
    case RenderScenario::Depth_Only:
    {
        // TODO Cache the descriptors so that we don't have to recreate those each frame?
        PipelineStateDesc DefaultPipelineState( PipelineStateDesc::GRAPHICS );
        DefaultPipelineState.PrimitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        DefaultPipelineState.DepthStencilState.EnableDepthWrite = true;
        DefaultPipelineState.DepthStencilState.EnableDepthTest = true;
        DefaultPipelineState.DepthStencilState.DepthComparisonFunc = eComparisonFunction::COMPARISON_FUNCTION_GREATER;

        DefaultPipelineState.RasterizerState.UseTriangleCCW = true;
        DefaultPipelineState.RasterizerState.CullMode = ( isDoubleFace ) ? eCullMode::CULL_MODE_NONE : eCullMode::CULL_MODE_FRONT;
        DefaultPipelineState.RasterizerState.FillMode = ( isWireframe ) ? eFillMode::FILL_MODE_WIREFRAME : eFillMode::FILL_MODE_SOLID;

		DefaultPipelineState.FramebufferLayout.declareRTV( 0, VIEW_FORMAT_R16G16B16A16_FLOAT );
		DefaultPipelineState.FramebufferLayout.declareRTV( 1, VIEW_FORMAT_R16G16_FLOAT );
        DefaultPipelineState.FramebufferLayout.declareDSV( VIEW_FORMAT_D32_FLOAT );

        DefaultPipelineState.samplerCount = samplerCount;
		DefaultPipelineState.InputLayout.Entry[0] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 0, 0, false, "POSITION" };
		DefaultPipelineState.InputLayout.Entry[1] = { 0, VIEW_FORMAT_R32G32B32_FLOAT, 0, 1, 0, true, "NORMAL" };
		DefaultPipelineState.InputLayout.Entry[2] = { 0, VIEW_FORMAT_R32G32_FLOAT, 0, 2, 0, true, "TEXCOORD" };
        DefaultPipelineState.depthClearValue = 0.0f;

        // Retrieve the appropriate shader binding for the given scenario.
        const PipelineStateCache::ShaderBinding& shaderBinding = getScenarioShaderBinding( scenario );

        scenarioPso = psoCache->getOrCreatePipelineState( DefaultPipelineState, shaderBinding, invalidateCachedStates );
    } break; 
    default:
        break;
    }

    // Reset flags.
    invalidateCachedStates = false;

    // Set pipeline states.
    cmdList->bindPipelineState( scenarioPso );

    // Lazily bind resources to the pipeline.
    // TODO Check which resource should be binded (e.g. don't bind the basecolor map during a depth only pass...).
    for ( auto& mutableParam : mutableParameters ) {
        if ( mutableParam.second.Type == MutableParameter::ParamType::Texture2D ) {
            cmdList->bindImage( mutableParam.first, mutableParam.second.CachedImageAsset );
        }
    }
}

const PipelineStateCache::ShaderBinding& Material::getScenarioShaderBinding( const RenderScenario scenario ) const
{
    switch ( scenario ) {
    case RenderScenario::Default_Editor:
        return defaultEditorScenario.PsoShaderBinding;
    case RenderScenario::Default:
        return defaultScenario.PsoShaderBinding;
    case RenderScenario::Default_Picking:
        return defaultPickingScenario.PsoShaderBinding;
    case RenderScenario::Default_Picking_Editor:
        return defaultPickingEditorScenario.PsoShaderBinding;
	case RenderScenario::Depth_Only:
		return depthOnlyScenario.PsoShaderBinding;
    default:
        return defaultScenario.PsoShaderBinding;
    }
}

const char* Material::getName() const
{
    return name.c_str();
}

bool Material::isParameterMutable( const dkStringHash_t parameterHashcode ) const
{
    return ( mutableParameters.find( parameterHashcode ) != mutableParameters.end() );
}

void Material::invalidateCache()
{
    invalidateCachedStates = true;
}

void Material::updateResourceStreaming( GraphicsAssetCache* graphicsAssetCache )
{
    for ( auto& mutableParam : mutableParameters ) {
        if ( mutableParam.second.Type == MutableParameter::ParamType::Texture2D ) {
            mutableParam.second.CachedImageAsset = graphicsAssetCache->getImage( StringToWideString( mutableParam.second.Value ).c_str() );
        }
    }
}

void Material::setParameterAsTexture2D( const dkStringHash_t parameterHashcode, const std::string& imagePath )
{
    MutableParameter& parameter = mutableParameters[parameterHashcode];

    parameter.Type = MutableParameter::ParamType::Texture2D;
    parameter.Value = imagePath;
}

bool Material::skipLighting() const
{
    return isShadeless;
}

bool Material::castShadow() const
{
    return true;
}
