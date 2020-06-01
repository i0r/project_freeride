/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Material.h"

#include <Io/TextStreamHelpers.h>
#include <DuskLexer/Parser.h>

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
                    }
                } else {
                    const TypeAST& type = *typeAST.Types[nodeIdx];

                    switch ( type.Type ) {
                    case TypeAST::RENDER_SCENARIO: {
                        if ( dk::core::ExpectKeyword( name.StreamPointer, 7, "Default" ) ) {
                            ParseScenario( defaultScenario, type );
                        }
                    } break;
                    }
                }
            }
        } break;
        }
    }
}

void Material::bindForScenario( const RenderScenario scenario )
{

}

const char* Material::getName() const
{
    return name.c_str();
}

bool Material::isParameterMutable( const dkStringHash_t parameterHashcode ) const
{
    return ( mutableParameters.find( parameterHashcode ) != mutableParameters.end() );
}
