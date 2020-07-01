/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "RenderPassGenerator.h"

#include "Parser.h"
#include "HLSLSemanticLuts.h"

#include <Core/StringHelpers.h>
#include <Core/Hashing/MurmurHash3.h>
#include <Core/Hashing/Helpers.h>

DUSK_INLINE void StorePassStage( std::string* stageShaderNames, const i32 stageIdx, const Token::StreamRef& paramValue )
{
    stageShaderNames[stageIdx].append( paramValue.StreamPointer, paramValue.Length );
}

DUSK_INLINE static bool IsReadOnlyResourceType( const TypeAST::ePrimitiveType type )
{
    return type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROBUFFER
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROSBUFFER
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE1D
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE2D
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE3D
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGECUBE
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGECUBE_ARRAY
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE1D_ARRAY
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE2D_ARRAY
		|| type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_SAMPLER
		|| type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_RAWBUFFER;
}

struct CbufferEntry {
    size_t                  SizeInBytes;
    const TypeAST*          Entry;
    const Token::StreamRef* Name;
    const Token::StreamRef* Value;

    CbufferEntry()
        : SizeInBytes( 0 )
        , Entry( nullptr )
        , Name( nullptr )
        , Value( nullptr )
    {

    }
};

struct CBufferLine {
    size_t                     SpaceLeftInBytes;
    std::list<CbufferEntry>    Entries;

    CBufferLine()
        : SpaceLeftInBytes( 16 )
    {

    }

    DUSK_INLINE void addEntry( CbufferEntry& entry )
    {
        SpaceLeftInBytes -= entry.SizeInBytes;
        Entries.push_back( entry );
    }
};

DUSK_INLINE static bool SortCbufferEntries( const CbufferEntry& l, const CbufferEntry& r )
{
    return ( l.SizeInBytes > r.SizeInBytes );
}

// Return true if the data has been added into a cbuffer line; false if the data doesn't fit in a cbuffer line
// and require a new cbuffer line
DUSK_INLINE static bool AddDataInCBufferLine( std::list<CBufferLine>& cbufferLines, CbufferEntry& entry )
{
    const size_t typeSize = entry.SizeInBytes;
    // Check if the primitive fits in one of the cbuffer line
    for ( CBufferLine& line : cbufferLines ) {
        if ( line.SpaceLeftInBytes >= typeSize ) {
            line.addEntry( entry );
            return true;
        }
    }

    return false;
}

static DUSK_INLINE void ParseRenderPassProperties( const TypeAST& renderPassBlock, TypeAST* propertiesNode, RenderLibraryGenerator::RenderPassInfos& passInfos )
{
    for ( u32 j = 0; j < renderPassBlock.Names.size(); ++j ) {
        const Token::StreamRef& passParam = renderPassBlock.Names[j];
        const Token::StreamRef& paramValue = renderPassBlock.Values[j];
        const TypeAST* paramType = renderPassBlock.Types[j];

        if ( dk::core::ExpectKeyword( passParam.StreamPointer, 7, "compute" ) ) {
            // Special case: if the pass has a Compute Shader, assume the pipeline state will be Compute
            passInfos.PipelineStateType = PipelineStateDesc::COMPUTE;
            StorePassStage( passInfos.StageShaderNames, 4, paramValue );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 6, "vertex" ) ) {
            StorePassStage( passInfos.StageShaderNames, 0, paramValue );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 5, "pixel" ) ) {
            StorePassStage( passInfos.StageShaderNames, 3, paramValue );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 9, "tsControl" ) ) {
            StorePassStage( passInfos.StageShaderNames, 1, paramValue );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 6, "tsEval" ) ) {
            StorePassStage( passInfos.StageShaderNames, 2, paramValue );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 13, "rendertargets" ) ) {
            std::string rtTok( paramValue.StreamPointer, paramValue.Length );
            
            std::size_t currentOffset = rtTok.find_first_of( '{' );
            std::size_t endOffset = rtTok.find_first_of( ',' );
            while ( endOffset != std::string::npos ) {
                std::string renderTarget = rtTok.substr( currentOffset + 1, endOffset - 1 );
                dk::core::TrimString( renderTarget );
                passInfos.ColorRenderTargets.push_back( renderTarget );

                currentOffset = endOffset + 1;
                endOffset = rtTok.find_first_of( ',', currentOffset );
            }

            endOffset = rtTok.find_first_of( '}' );
            std::string renderTarget = rtTok.substr( currentOffset + 1, endOffset - 1 );
            dk::core::TrimString( renderTarget );
            passInfos.ColorRenderTargets.push_back( renderTarget );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 12, "depthStencil" ) ) {
            passInfos.DepthStencilBuffer = std::string( paramValue.StreamPointer, paramValue.Length );
            dk::core::TrimString( passInfos.DepthStencilBuffer );
        } else if ( dk::core::ExpectKeyword( passParam.StreamPointer, 8, "dispatch" ) ) {
            std::string dispatchTok( paramValue.StreamPointer, paramValue.Length );
            dk::core::TrimString( dispatchTok );

            std::size_t offsetX = dispatchTok.find_first_of( ',' ),
                offsetY = dispatchTok.find_first_of( ',', offsetX + 1 ),
                offsetZ = dispatchTok.find_first_of( '}' );

            std::string dispatchX = dispatchTok.substr( 1, offsetX - 1 );
            std::string dispatchY = dispatchTok.substr( offsetX + 1, offsetY - offsetX - 1 );
            std::string dispatchZ = dispatchTok.substr( offsetY + 1, offsetZ - offsetY - 1 );

            passInfos.DispatchX = std::stoi( dispatchX );
            passInfos.DispatchY = std::stoi( dispatchY );
            passInfos.DispatchZ = std::stoi( dispatchZ );
        } else if ( propertiesNode != nullptr ) {
            // If the name does not match an identifier, assume it is a property override
            for ( u32 i = 0; i < propertiesNode->Values.size(); i++ ) {
                if ( dk::core::ExpectKeyword( passParam.StreamPointer, passParam.Length, propertiesNode->Names[i].StreamPointer ) ) {
                    propertiesNode->Values[i] = paramValue;
                    break;
                }
            }
        }
    }
}

static bool IsCompileTimeConstant( const TypeAST::ePrimitiveType primitiveType )
{
    return primitiveType == TypeAST::PRIMITIVE_TYPE_CFLAG || primitiveType == TypeAST::PRIMITIVE_TYPE_CINT;
}

// Write a cbuffer declaration using a parser AST node as an input
// Compile time parameters will be output in the cflagMap
static void WriteConstantBufferDecl( const TypeAST* bufferData, const char* bufferName, const i32 bufferIndex, std::unordered_map<dkStringHash_t, const Token::StreamRef*>& compileTimeConstants, std::string& hlslSourceOutput )
{
    if ( bufferData == nullptr ) {
        return;
    }

    std::list<CBufferLine> cbufferLines; // PerPass cbuffer layout (with a 16bytes alignment)
    for ( u32 i = 0; i < bufferData->Values.size(); i++ ) {
        const TypeAST::ePrimitiveType primType = bufferData->Types[i]->PrimitiveType;

        // Compile time constants should be used to cull dead code at compile time (so do not write those)
        if ( IsCompileTimeConstant( primType ) ) {
            std::string flagName( bufferData->Names[i].StreamPointer, bufferData->Names[i].Length );

            compileTimeConstants.insert( std::make_pair( dk::core::CRC32( flagName ), &bufferData->Values[i] ) );
        } else {
            const size_t typeSize = PRIMITIVE_TYPE_SIZE[primType];
            if ( typeSize == 0 ) {
                const std::string typeString( bufferData->Types[i]->Name.StreamPointer, bufferData->Types[i]->Name.Length );
                DUSK_LOG_WARN( "Unknown/Invalid parameter type '%s' found while writing cbuffer '%hs'!", typeString.c_str(), bufferName );
                continue;
            }

            CbufferEntry entry;
            entry.SizeInBytes = typeSize;
            entry.Entry = bufferData->Types[i];
            entry.Name = &bufferData->Names[i];
            entry.Value = &bufferData->Values[i];

            bool hasDataBeenAdded = AddDataInCBufferLine( cbufferLines, entry );

            // No suitable cbuffer line found; create a new one
            if ( !hasDataBeenAdded ) {
                CBufferLine line;
                line.addEntry( entry );

                cbufferLines.push_back( line );
            }
        }
    }

    hlslSourceOutput.append( "cbuffer " );
    hlslSourceOutput.append( bufferName );
    hlslSourceOutput.append( " : register( b" );
    hlslSourceOutput.append( std::to_string( bufferIndex ) );
    hlslSourceOutput.append( " )\n{ \n" );

    for ( CBufferLine& line : cbufferLines ) {
        // Sort the line (this way data should perfectly fit)
        line.Entries.sort( SortCbufferEntries );

        for ( CbufferEntry& entry : line.Entries ) {
            hlslSourceOutput.append( "\t" );
            hlslSourceOutput.append( PRIMITIVE_TYPES[entry.Entry->PrimitiveType] );
            hlslSourceOutput.append( " " );
            hlslSourceOutput.append( entry.Name->StreamPointer, entry.Name->Length );

            // Ignore initializer values (for now).
            if ( entry.Value != nullptr && entry.Value->StreamPointer != nullptr ) {
                if ( entry.Value->StreamPointer[0] == '[' ) {
                    hlslSourceOutput.append( entry.Value->StreamPointer, entry.Value->Length );
                }
            }

            hlslSourceOutput.append( ";\n" );
        }
    }

    hlslSourceOutput.append( "};\n\n" );
}

static DUSK_INLINE void WriteResourceList( const TypeAST* resourceList, const TypeAST* properties, const RenderLibraryGenerator::RenderPassInfos& renderPassInfos, std::string& hlslSource )
{
    if ( resourceList != nullptr ) {
        u32 samplerRegisterIdx = 0;
        u32 srvRegisterIdx = 0;
        u32 uavRegisterIdx = 0;

        for ( u32 i = 0; i < resourceList->Names.size(); i++ ) {
            std::string swizzle;
            std::string registerAddr;

            const TypeAST::ePrimitiveType primType = resourceList->Types[i]->PrimitiveType;
            const std::string resName( resourceList->Names[i].StreamPointer, resourceList->Names[i].Length );
            const bool isReadOnlyRes = IsReadOnlyResourceType( primType );
            bool isMultisampled = false;
            
            for ( u32 j = 0; j < resourceList->Types[i]->Names.size(); j++ ) {
                // TODO This is crappy; there is probably a better way to do this!
                // TODO Hash resource parameters?
                std::string pName( resourceList->Types[i]->Names[j].StreamPointer, resourceList->Types[i]->Names[j].Length );
                std::string pValue( resourceList->Types[i]->Values[j].StreamPointer, resourceList->Types[i]->Values[j].Length );
                if ( pName == "swizzle" ) {
                    swizzle.append( "<" );
                    swizzle.append( pValue );
                    swizzle.append( ">" );
                } else if ( pName == "format" ) {
                    dk::core::TrimString( pValue );
                } else if ( pName == "isMultisampled" ) {
                    std::string pValueToLower = pValue;
                    dk::core::StringToLower( pValueToLower );

                    if ( pValueToLower == "0" || pValueToLower == "1" || pValueToLower == "true" || pValueToLower == "false" ) {
                        isMultisampled = true;
                    } else {
                        // Iterate the properties list and check if there is a property matching (declared as a cflag with the same name)
                        for ( u32 i = 0; i < properties->Values.size(); i++ ) {
                            if ( dk::core::ExpectKeyword( pValue.c_str(), pValue.size(), properties->Names[i].StreamPointer )
                              && properties->Types[i]->PrimitiveType == TypeAST::PRIMITIVE_TYPE_CFLAG ) {
                                isMultisampled = dk::core::StringToBool( std::string( properties->Values[i].StreamPointer, properties->Values[i].Length ) );
                                break;
                            }
                        }
                    }
                }
            }

            // Build register index
            // We need to figure out the virtual register space (based on the primitive type)
            // and then inc. the current index within the register space
            // TODO Support SM6.0 shader/register spaces (maybe try to create two hlsl sources with different
            // feature level?)
            if ( primType == TypeAST::PRIMITIVE_TYPE_SAMPLER ) {
                registerAddr.append( "s" );
                registerAddr.append( std::to_string( samplerRegisterIdx++ ) );
            } else {
                if ( isReadOnlyRes ) {
                    registerAddr.append( "t" );
                    registerAddr.append( std::to_string( srvRegisterIdx++ ) );
                } else {
                    // UAV bindings on the Graphics pipeline share the same register space as the framebuffer render targets
                    // which is why we need to offset the register index with the number of rendertarget.
                    const u32 uavRegisterOffset = ( renderPassInfos.PipelineStateType == PipelineStateDesc::GRAPHICS ) ? static_cast<u32>( renderPassInfos.ColorRenderTargets.size() ) : 0;

                    registerAddr.append( "u" );
                    registerAddr.append( std::to_string( uavRegisterOffset + uavRegisterIdx++ ) );
                }
            }

            std::string resourceEntry;
            resourceEntry.append( PRIMITIVE_TYPES[resourceList->Types[i]->PrimitiveType] );
            if ( isMultisampled ) {
                resourceEntry.append( "MS" );
            }

            resourceEntry.append( swizzle );
            resourceEntry.append( " " );
            resourceEntry.append( resourceList->Names[i].StreamPointer, resourceList->Names[i].Length );
            resourceEntry.append( " : register( " );
            resourceEntry.append( registerAddr );
            resourceEntry.append( " );\n" );

            hlslSource.append( resourceEntry );
        }
    }
}

static void PreprocessShaderSource( const std::string& bodySource, const i32 stageIdx, const std::unordered_map<dkStringHash_t, const Token::StreamRef*>& constantMap, std::string& processedSource, std::unordered_map<dkStringHash_t, std::string>& semanticInput, std::unordered_map<dkStringHash_t, std::string>& semanticOutput )
{
    Lexer srcCodeLexer( bodySource.c_str() );
    i32 inputCount = 0;

    bool skipSpace = true;
    Token token;
    std::string srcCodeLine;
    while ( !srcCodeLexer.equalToken( Token::END_OF_STREAM, token ) ) {
        switch ( token.type ) {
        case Token::DOLLAR:
        {
            if ( srcCodeLexer.expectToken( Token::IDENTIFIER, token ) ) {
                std::string semanticName( token.streamReference.StreamPointer, token.streamReference.Length );
                dkStringHash_t semanticHash = dk::core::CRC32( semanticName );

                auto flagIt = constantMap.find( semanticHash );
                if ( flagIt != constantMap.end() ) {
                    srcCodeLine.append( flagIt->second->StreamPointer, flagIt->second->Length );
                } else {
                    dk::core::StringToLower( semanticName );
                    semanticHash = dk::core::CRC32( semanticName );

                    std::string dataName = "SystemValue_" + std::to_string( inputCount );

                    for ( i32 i = 0; i < SEMANTIC_COUNT; i++ ) {
                        if ( SEMANTIC_HASHTABLE[i] == semanticHash ) {
                            if ( SEMANTIC_ACCESS[i] >> stageIdx & 1 ) {
                                auto semanticIt = semanticOutput.find( semanticHash );
                                if ( semanticIt == semanticOutput.end() ) {
                                    semanticOutput.insert( std::make_pair( semanticHash, dataName ) );
                                    inputCount++;
                                } else {
                                    dataName = semanticIt->second;
                                }

                                srcCodeLine.append( "output." );
                                srcCodeLine.append( dataName );
                            } else {
                                auto semanticIt = semanticInput.find( semanticHash );
                                if ( semanticIt == semanticInput.end() ) {
                                    semanticInput.insert( std::make_pair( semanticHash, dataName ) );
                                    inputCount++;
                                } else {
                                    dataName = semanticIt->second;
                                }

                                srcCodeLine.append( "input." );
                                srcCodeLine.append( dataName );
                            }
                        }
                    }
                }
            }
        } break;
        case Token::SHARP:
        {
            srcCodeLine.append( token.streamReference.StreamPointer, token.streamReference.Length );

            // include, if, pragma, ...
            srcCodeLexer.nextToken( token );
            if ( token.type != Token::IDENTIFIER ) {
                break;
            }

            // TODO Support other directives (if; pragma; etc.)
            std::string tokenValue( token.streamReference.StreamPointer, token.streamReference.Length );
            if ( tokenValue == "include" ) {
                srcCodeLine.append( tokenValue );

                srcCodeLexer.nextToken( token );
                if ( token.type == Token::OPEN_BRACKET ) {
                    // Include (system scope)
                    do {
                        srcCodeLine.append( token.streamReference.StreamPointer, token.streamReference.Length );
                        srcCodeLexer.nextToken( token );
                    } while ( token.type != Token::CLOSE_BRACKET );
                    srcCodeLine.append( ">\n" );
                } else if ( token.type == Token::STRING ) {
                    // Include (local scope)
                    srcCodeLine.append( " \"" );
                    srcCodeLine.append( token.streamReference.StreamPointer, token.streamReference.Length );
                    srcCodeLine.append( "\"" );
                }
            } else if ( tokenValue == "endif" ) {
                srcCodeLine.append( "endif\n" );
            } else if ( tokenValue == "define" ) {
                srcCodeLine.append( "define " );
            } else if ( tokenValue == "else" ) {
                srcCodeLine.append( "else\n" );
            } else if ( tokenValue == "ifdef" || tokenValue == "ifndef" ) {
                srcCodeLine.append( tokenValue );
                srcCodeLine.append( " " );

                srcCodeLexer.nextToken( token );
                if ( token.type == Token::DOLLAR ) {
                    srcCodeLexer.nextToken( token );

                    if ( token.type == Token::IDENTIFIER ) {
                        std::string semanticName( token.streamReference.StreamPointer, token.streamReference.Length );
                        dkStringHash_t semanticHash = dk::core::CRC32( semanticName );

                        auto flagIt = constantMap.find( semanticHash );
                        if ( flagIt != constantMap.end() ) {
                            std::string flagValue( flagIt->second->StreamPointer, flagIt->second->Length );

                            // TODO ATROCIOUS GARBAGE THAT NEEDS A REFACTORING AS SOON AS POSSIBLE
                            static i32 ProxyCountFlag = 0;

                            std::string defineProxyName = "PROXY_" + std::to_string( ProxyCountFlag++ );

                            if ( flagValue == "true" ) {
                                processedSource.append( "#define " );
                                processedSource.append( defineProxyName );
                                processedSource.append( "\n" );
                            }

                            srcCodeLine.append( defineProxyName );
                        } else {
                            DUSK_LOG_WARN( "Unknown cflag specified by preprocessor guards: '%hs'\n", semanticName.c_str() );
                        }
                    }

                    srcCodeLine.append( "\n" );
                }
            } else if ( tokenValue == "if" || tokenValue == "elif" ) {
                srcCodeLine.append( tokenValue );
                srcCodeLine.append( " " );

                srcCodeLexer.nextToken( token );
                if ( token.type == Token::DOLLAR ) {
                    srcCodeLexer.nextToken( token );

                    if ( token.type == Token::IDENTIFIER ) {
                        std::string semanticName( token.streamReference.StreamPointer, token.streamReference.Length );
                        dkStringHash_t semanticHash = dk::core::CRC32( semanticName );

                        auto flagIt = constantMap.find( semanticHash );
                        if ( flagIt != constantMap.end() ) {
                            std::string flagValue( flagIt->second->StreamPointer, flagIt->second->Length );

                         /*   static i32 ProxyCount = 0;
                            
                            std::string defineProxyName = "PROXY_" + std::to_string( ProxyCount++ );

                            processedSource.append( "#define " );
                            processedSource.append( defineProxyName );
                            processedSource.append( " " );
                            processedSource.append( flagValue );
                            processedSource.append( "\n" );*/

                            srcCodeLine.append( flagValue );
                        } else {
                            DUSK_LOG_WARN( "Unknown cflag specified by preprocessor guards: '%hs'\n", semanticName.c_str() );
                        }

                        do {
                            srcCodeLexer.nextToken( token );
                        } while ( token.type == Token::EQUALS || token.type == Token::OPEN_BRACKET || token.type == Token::CLOSE_BRACKET );

                        if ( token.type == Token::NUMBER ) {
                            srcCodeLine.append( " == " );
                            srcCodeLine.append( token.streamReference.StreamPointer, token.streamReference.Length );
                        }
                    }

                    srcCodeLine.append( "\n" );
                }
            }

            processedSource.append( srcCodeLine );
            srcCodeLine.clear();
        } break;
        case Token::OPEN_BRACE:
        {
            srcCodeLine.append( " {\n" );
            processedSource.append( srcCodeLine );
            srcCodeLine.clear();
        } break;
        case Token::CLOSE_BRACE:
        {
            srcCodeLine.append( "}\n" );
            processedSource.append( srcCodeLine );
            srcCodeLine.clear();
        } break;
        case Token::SEMICOLON:
        {
            srcCodeLine.append( ";\n" );
            processedSource.append( srcCodeLine );
            srcCodeLine.clear();
        } break;
        default:
        {
            if ( token.type == Token::IDENTIFIER && !skipSpace ) {
                srcCodeLine.append( " " );
            }

            srcCodeLine.append( token.streamReference.StreamPointer, token.streamReference.Length );
        } break;
        }

        skipSpace = ( token.type == Token::DOT
                      || token.type == Token::OPEN_PAREN
                      || token.type == Token::OPEN_BRACE
                      || token.type == Token::CLOSE_BRACE
                      || token.type == Token::PLUS
                      || token.type == Token::MINUS
                      || token.type == Token::ASTERISK
                      || token.type == Token::SLASH
                      || token.type == Token::EQUALS
                      || token.type == Token::NUMBER
                      || token.type == Token::COMMA
                      || token.type == Token::SEMICOLON );
    }
}

static void WritePassInOutData( const std::unordered_map<dkStringHash_t, std::string>& data, const std::string& dataName, std::string& hlslSource )
{
    if ( data.empty() ) {
        return;
    }

    hlslSource.append( "\nstruct " );
    hlslSource.append( dataName );
    hlslSource.append( " {\n" );

    for ( const std::pair<dkStringHash_t, std::string>& semanticPair : data ) {
        for ( i32 i = 0; i < SEMANTIC_COUNT; i++ ) {
            if ( SEMANTIC_HASHTABLE[i] == semanticPair.first ) {
                hlslSource.append( "\t" );
                hlslSource.append( SEMANTIC_SWIZZLE[i] );
                hlslSource.append( " " );
                hlslSource.append( semanticPair.second );
                hlslSource.append( " : " );
                hlslSource.append( SEMANTIC_NAME[i] );
                hlslSource.append( ";\n" );
                break;
            }
        }
    }
    hlslSource.append( "};\n\n" );
}

constexpr eShaderStage SHADER_STAGE_LUT[eShaderStage::SHADER_STAGE_COUNT] = {
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_TESSELATION_CONTROL,
    SHADER_STAGE_TESSELATION_EVALUATION,
    SHADER_STAGE_PIXEL,
    SHADER_STAGE_COMPUTE
};

constexpr const char* SHADER_STAGE_NAME_LUT[eShaderStage::SHADER_STAGE_COUNT] = {
    "vertex",
    "tesselationControl",
    "tesselationEvaluation",
    "pixel",
    "compute"
};

RenderLibraryGenerator::RenderLibraryGenerator()
    : propertiesNode( nullptr )
    , resourceListNode( nullptr )
{

}

RenderLibraryGenerator::~RenderLibraryGenerator()
{

}

void RenderLibraryGenerator::generateFromAST( const TypeAST& root, const bool generateMetadata /*= true*/, const bool generateReflection /*= false */ )
{
    resetGeneratedContent();

    // Create library name.
    generatedLibraryName = "NoName_Library";
    if ( root.Name.StreamPointer == nullptr ) {
        DUSK_LOG_WARN( "RenderLibrary: missing name identifier! The library will use a generic name instead\n" );
    } else {
        generatedLibraryName = std::string( root.Name.StreamPointer, root.Name.Length );
    }

    if ( generateMetadata ) {
        generatedMetadata.append( "// This file has been automatically generated by Dusk Baker\n" );
        generatedMetadata.append( "#pragma once\n\n" );
        generatedMetadata.append( "#include <Graphics/FrameGraph.h>\n\n" );
		generatedMetadata.append( "#include <Graphics/PipelineStateCache.h>\n\n" );
		generatedMetadata.append( "#include <Graphics/RenderPasses/Headers/Light.h>\n\n" );
        generatedMetadata.append( "namespace " );
        generatedMetadata.append( generatedLibraryName );
        generatedMetadata.append( "\n{\n" );
    }

    if ( generateReflection ) {
        generatedReflection.append( "// This file has been automatically generated by Dusk Baker\n" );
        generatedReflection.append( "#pragma once\n\n#if DUSK_USE_IMGUI\n" );
        generatedReflection.append( "#include <ThirdParty/imgui/imgui.h>\n" );
        generatedReflection.append( "#include <Graphics/RenderModules/Generated/" );
        generatedReflection.append( generatedLibraryName );
        generatedReflection.append( ".generated.h>\n\n" );
        generatedReflection.append( "namespace " );
        generatedReflection.append( generatedLibraryName );
        generatedReflection.append( "\n{\n" );
    }

    // Iterate over each AST node.
    for ( u32 i = 0; i < root.Types.size(); ++i ) {
        const TypeAST& astType = *root.Types[i];
        const Token::StreamRef& astName = root.Names[i];

        switch ( astType.Type ) {
        case TypeAST::PROPERTIES:
        {
            // HACK We need to un-const the property list in order to do properties override in place
            propertiesNode = const_cast< TypeAST* >( root.Types[i] );
        } break;

        case TypeAST::RESOURCES:
        {
            resourceListNode = root.Types[i];
        } break;

        case TypeAST::SHADER:
        {
            dkStringHash_t shaderHashcode = dk::core::CRC32( astType.Name.StreamPointer, astType.Name.Length );
            shadersSource[shaderHashcode] = &astType.Values[0];
        } break;

        case TypeAST::SHARED_CONTENT:
        {
            for ( size_t i = 0; i < astType.Values.size(); i++ ) {
                sharedShaderBody.append( astType.Values[i].StreamPointer, astType.Values[i].Length );
            }
        } break;

        case TypeAST::PASS:
        {
            processRenderPassNode( astName, astType );
        } break;
        };
    }

    generatedMetadata.append( "}\n" );

    generatedReflection.append( "}\n" );
    generatedReflection.append( "#endif\n" );
}

void RenderLibraryGenerator::processRenderPassNode( const Token::StreamRef& astName, const TypeAST& astType )
{
    // Parse RenderPass properties (override library properties if needed)
    RenderPassInfos passInfos;
    ParseRenderPassProperties( astType, propertiesNode, passInfos );

    // Name of this renderpass (without its library name).
    const std::string scopedPassName( astName.StreamPointer, astName.Length );
    passInfos.RenderPassName = scopedPassName;

    generatedRenderPasses.push_back( passInfos );

    // ShaderBinding content for this RenderPass.
    std::string shaderBindingContent;

    // Generate Shader Stage Permutation for the current RenderPass
    for ( i32 i = 0; i < eShaderStage::SHADER_STAGE_COUNT; i++ ) {
        processShaderStage( i, scopedPassName, passInfos, shaderBindingContent );
    }

    // Create ShaderBinding declaration.
    std::string shaderBindingDecl;
    shaderBindingDecl.append( "\tstatic constexpr PipelineStateCache::ShaderBinding " );
    shaderBindingDecl.append( scopedPassName );
    shaderBindingDecl.append( "_ShaderBinding = PipelineStateCache::ShaderBinding( " );
    shaderBindingDecl.append( shaderBindingContent );
    shaderBindingDecl.append( " );\n" );

    generateResourceMetadata( passInfos, scopedPassName, shaderBindingDecl );

    // Reflection API
    if ( propertiesNode != nullptr ) {
        std::string propertyDecl;
        propertyDecl.append( "\n\tstatic struct " );
        propertyDecl.append( scopedPassName );
        propertyDecl.append( "RuntimeProperties\n\t{\n" );

        generatedReflection.append( "\tstatic void Reflect" );
        generatedReflection.append( scopedPassName );
        generatedReflection.append( "( " );
        generatedReflection.append( scopedPassName );
        generatedReflection.append( "RuntimeProperties& properties )\n\t{ \n" );

        generatedReflection.append( "\t\tImGui::LabelText( \"" );
        generatedReflection.append( scopedPassName );
        generatedReflection.append( "\", \"" );
        generatedReflection.append( scopedPassName );
        generatedReflection.append( "\" );\n" );

        size_t cbufferTotalSize = 0;
        std::list<CBufferLine> perPassBufferLines; // PerPass cbuffer layout (with a 16bytes alignment)
        for ( u32 i = 0; i < propertiesNode->Values.size(); i++ ) {
            const TypeAST::ePrimitiveType primType = propertiesNode->Types[i]->PrimitiveType;

            // Compile time constants should be used to cull dead code at compile time (so do not write those)
            if ( IsCompileTimeConstant( primType ) ) {
                continue;
            }

            const size_t typeSize = PRIMITIVE_TYPE_SIZE[primType];
            if ( typeSize == 0 ) {
                continue;
            }

            cbufferTotalSize += typeSize;

            CbufferEntry entry;
            entry.SizeInBytes = typeSize;
            entry.Entry = propertiesNode->Types[i];
            entry.Name = &propertiesNode->Names[i];
            entry.Value = ( propertiesNode->Values[i].StreamPointer != nullptr ) ? &propertiesNode->Values[i] : nullptr;

            bool hasDataBeenAdded = AddDataInCBufferLine( perPassBufferLines, entry );

            // No suitable cbuffer line found; create a new one
            if ( !hasDataBeenAdded ) {
                CBufferLine line;
                line.addEntry( entry );

                perPassBufferLines.push_back( line );
            }
        }

        for ( CBufferLine& line : perPassBufferLines ) {
            // Sort the line (this way data should perfectly fit)
            line.Entries.sort( SortCbufferEntries );

            for ( CbufferEntry& entry : line.Entries ) {
                TypeAST::ePrimitiveType primType = entry.Entry->PrimitiveType;
                std::string propertyName( entry.Name->StreamPointer, entry.Name->Length );

                propertyDecl.append( "\t\t" );
                switch ( primType ) {
                case TypeAST::PRIMITIVE_TYPE_I8:
                    propertyDecl.append( "i8" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", -128, +127 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_U8:
                    propertyDecl.append( "u8" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", 0, +255 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_I16:
                    propertyDecl.append( "i16" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", -32768, +32767 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_U16:
                    propertyDecl.append( "u16" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", 0, +65535 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_I32:
                    propertyDecl.append( "i32" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", -2147483648, +2147483647 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_U32:
                    propertyDecl.append( "u32" );

                    generatedReflection.append( "\t\tImGui::InputInt( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", (int*)&properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ", 0, +4294967295 );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_F32:
                    propertyDecl.append( "f32" );

                    generatedReflection.append( "\t\tImGui::DragFloat( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", &properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( " );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_BOOL:
                    propertyDecl.append( "bool" );

                    generatedReflection.append( "\t\tImGui::Checkbox( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", &properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( " );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_FLOAT2:
                    propertyDecl.append( "dkVec2f" );

                    generatedReflection.append( "\t\tImGui::DragFloat2( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", &properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ".x );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_FLOAT3:
                    propertyDecl.append( "dkVec3f" );

                    generatedReflection.append( "\t\tImGui::DragFloat3( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", &properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ".x );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_FLOAT4:
                    propertyDecl.append( "dkVec4f" );

                    generatedReflection.append( "\t\tImGui::DragFloat4( \"" );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( "\", &properties." );
                    generatedReflection.append( propertyName );
                    generatedReflection.append( ".x );\n" );
                    break;
                case TypeAST::PRIMITIVE_TYPE_FLOAT4X4:
                    propertyDecl.append( "dkMat4x4f" );
					break;
				default:
					propertyDecl.append( PRIMITIVE_TYPES[primType] );
					break;
                };

                propertyDecl.append( " " );
                propertyDecl.append( propertyName );
                if ( entry.Value != nullptr ) {
					if ( entry.Value->StreamPointer[0] != '[' ) {
						propertyDecl.append( " = " );
                    }

                    propertyDecl.append( entry.Value->StreamPointer, entry.Value->Length );
                }
                propertyDecl.append( ";\n" );
            }
        }

        // Pad Cbuffer if needed (respect 16bytes alignment restriction).
        if ( cbufferTotalSize % 16 != 0 ) {
            propertyDecl.append( "\t\tu8 __PADDING__[" );
            propertyDecl.append( std::to_string( 16 - ( cbufferTotalSize % 16 ) ) );
            propertyDecl.append( "];\n" );
        }

        propertyDecl.append( "\t} " );
        propertyDecl.append( scopedPassName );
        propertyDecl.append( "Properties;\n" );

        generatedReflection.append( "\t}\n" );

        generatedMetadata.append( propertyDecl );
    }
}

void RenderLibraryGenerator::generateResourceMetadata( RenderPassInfos& passInfos, const std::string& scopedPassName, const std::string& shaderBindingDecl )
{
    std::vector<u32> outputResIndexes;
    for ( u32 i = 0; i < resourceListNode->Names.size(); i++ ) {
        const TypeAST::ePrimitiveType primType = resourceListNode->Types[i]->PrimitiveType;
        const std::string resName( resourceListNode->Names[i].StreamPointer, resourceListNode->Names[i].Length );

        bool isReadOnlyRes = IsReadOnlyResourceType( primType );

        // Check if the texture is bound to the framebuffer (by name comparison)
        // The resource should be read only if that's not the case (unless it uses a UAV)
        if ( primType == TypeAST::PRIMITIVE_TYPE_ROIMAGE1D
             || primType == TypeAST::PRIMITIVE_TYPE_ROIMAGE2D
             || primType == TypeAST::PRIMITIVE_TYPE_ROIMAGE3D ) {
            if ( passInfos.DepthStencilBuffer == resName ) {
                isReadOnlyRes = false;
            } else {
                for ( std::string& fboColorAttachment : passInfos.ColorRenderTargets ) {
                    if ( fboColorAttachment == resName ) {
                        isReadOnlyRes = false;
                        break;
                    }
                }
            }
        }

        if ( !isReadOnlyRes ) {
            outputResIndexes.push_back( i );
        }

        generatedMetadata.append( "\tstatic constexpr dkStringHash_t " );
        generatedMetadata.append( scopedPassName );
        generatedMetadata.append( "_" );
        generatedMetadata.append( resName );
        generatedMetadata.append( "_Hashcode = DUSK_STRING_HASH( \"" );
        generatedMetadata.append( resName );
        generatedMetadata.append( "\" );\n" );
    }

    bool hasMultipleOutput = ( outputResIndexes.size() > 1 );
    if ( hasMultipleOutput ) {
        generatedMetadata.append( "\tstruct " );
        generatedMetadata.append( scopedPassName );
        generatedMetadata.append( "_Output {\n" );

        for ( u32 resIdx : outputResIndexes ) {
            if ( resourceListNode->Types[resIdx]->PrimitiveType == TypeAST::PRIMITIVE_TYPE_SAMPLER ) {
                continue;
            }

            generatedMetadata.append( "\t\tResHandle_t\t" );
            generatedMetadata.append( resourceListNode->Names[resIdx].StreamPointer, resourceListNode->Names[resIdx].Length );
            generatedMetadata.append( ";\n" );
        }

        generatedMetadata.append( "\t};\n\n" );
    }

    if ( passInfos.PipelineStateType == PipelineStateDesc::COMPUTE ) {
        generatedMetadata.append( "\tstatic constexpr u32 " );
        generatedMetadata.append( scopedPassName );
        generatedMetadata.append( "_DispatchX = " );
        generatedMetadata.append( std::to_string( passInfos.DispatchX ) );
        generatedMetadata.append( "u;\n" );

        generatedMetadata.append( "\tstatic constexpr u32 " );
        generatedMetadata.append( scopedPassName );
        generatedMetadata.append( "_DispatchY = " );
        generatedMetadata.append( std::to_string( passInfos.DispatchY ) );
        generatedMetadata.append( "u;\n" );

        generatedMetadata.append( "\tstatic constexpr u32 " );
        generatedMetadata.append( scopedPassName );
        generatedMetadata.append( "_DispatchZ = " );
        generatedMetadata.append( std::to_string( passInfos.DispatchZ ) );
        generatedMetadata.append( "u;\n" );
    }

    generatedMetadata.append( "\tstatic constexpr const char* " );
    generatedMetadata.append( scopedPassName );
    generatedMetadata.append( "_Name = \"" );
    generatedMetadata.append( generatedLibraryName );
    generatedMetadata.append( "::" );
    generatedMetadata.append( scopedPassName );
    generatedMetadata.append( "\";\n" );

    generatedMetadata.append( "\tstatic constexpr const dkChar_t* " );
    generatedMetadata.append( scopedPassName );
    generatedMetadata.append( "_EventName = DUSK_STRING( \"" );
    generatedMetadata.append( generatedLibraryName );
    generatedMetadata.append( "::" );
    generatedMetadata.append( scopedPassName );
    generatedMetadata.append( "\" );\n" );

    generatedMetadata.append( shaderBindingDecl );
}

void RenderLibraryGenerator::processShaderStage( const i32 stageIndex, const std::string scopedPassName, RenderPassInfos& passInfos, std::string& shaderBindingDecl )
{
    // Skip unused shader stage.
    if ( passInfos.StageShaderNames[stageIndex].empty() ) {
        shaderBindingDecl.append( "nullptr" );

        if ( stageIndex != ( eShaderStage::SHADER_STAGE_COUNT - 1 ) ) {
            shaderBindingDecl.append( ", " );
        }
        return;
    }

    // Check if the shader has been declared prior to the renderpass.
    const dkStringHash_t shaderHashcode = dk::core::CRC32( passInfos.StageShaderNames[stageIndex] );
    auto it = shadersSource.find( shaderHashcode );
    DUSK_RAISE_FATAL_ERROR( it != shadersSource.end(), "Unknown shader stage '%s'!", passInfos.StageShaderNames[stageIndex].c_str() );

    // Append shader stage name to the shader name (this way we can have shaders with the same name but different
    // stage type).
    const std::string baseShaderName = passInfos.StageShaderNames[stageIndex];
    passInfos.StageShaderNames[stageIndex].append( passInfos.RenderPassName );
    passInfos.StageShaderNames[stageIndex].append( SHADER_STAGE_NAME_LUT[stageIndex] );

    // Hash the final name.
    Hash128 permutationHashcode;
    // TODO Keep the hashing key somewhere shared (atm it's hardcoded in multiple files...).
    MurmurHash3_x64_128( passInfos.StageShaderNames[stageIndex].c_str(), static_cast< int >( passInfos.StageShaderNames[stageIndex].size() ), 19081996, &permutationHashcode );
    dkString_t filenameWithExtension = dk::core::GetHashcodeDigest128( permutationHashcode );

    // Create our generated shader output.
    GeneratedShader generatedShader( SHADER_STAGE_LUT[stageIndex], WideStringToString( filenameWithExtension ).c_str(), baseShaderName.c_str(), passInfos.RenderPassName.c_str() );

    appendSharedShaderHeader( generatedShader.GeneratedSource );

    // Shader stage compile-time constant map (key is the constant hashcode; value its stream reference).
    std::unordered_map<dkStringHash_t, const Token::StreamRef*> constantMap;

    // Shader input semantics (from either vertex buffers or previous shader stage).
    std::unordered_map<dkStringHash_t, std::string> semanticInput;

    // Shader output semantices (to the ROP, UAV, or next shader stage).
    std::unordered_map<dkStringHash_t, std::string> semanticOutput;

    // Write per renderpass/view cbuffers and parse compile-time flags
    WriteConstantBufferDecl( propertiesNode, "PerPassBuffer", 1, constantMap, generatedShader.GeneratedSource );

    // Create resource list
    WriteResourceList( resourceListNode, propertiesNode, passInfos, generatedShader.GeneratedSource );

    // NOTE We want the shared header first in the hlsl source (since it might add includes to the file
    // used by the resource list)
    PreprocessShaderSource( sharedShaderBody, stageIndex, constantMap, generatedShader.GeneratedSource, semanticInput, semanticOutput );

    // Preprocess shader source code
    std::string processedSource;
    std::string bodySource( it->second->StreamPointer, it->second->Length );
    PreprocessShaderSource( bodySource, stageIndex, constantMap, processedSource, semanticInput, semanticOutput );

    // Write Stage input (based on system value semantics used in the source code)
    WritePassInOutData( semanticInput, STAGE_INPUT_NAME_LUT[stageIndex], generatedShader.GeneratedSource );
    WritePassInOutData( semanticOutput, STAGE_OUTPUT_NAME_LUT[stageIndex], generatedShader.GeneratedSource );

    if ( stageIndex == 4 ) {
        generatedShader.GeneratedSource.append( "[numthreads( " );
        generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchX ) );
        generatedShader.GeneratedSource.append( ", " );
        generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchY ) );
        generatedShader.GeneratedSource.append( ", " );
        generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchZ ) );
        generatedShader.GeneratedSource.append( " )]\n" );
    }

    generatedShader.GeneratedSource.append( STAGE_OUTPUT_NAME_LUT[stageIndex] );
    generatedShader.GeneratedSource.append( " EntryPoint( " );

    if ( !semanticInput.empty() ) {
        generatedShader.GeneratedSource.append( STAGE_INPUT_NAME_LUT[stageIndex] );
        generatedShader.GeneratedSource.append( " input " );
    }

    generatedShader.GeneratedSource.append( ")\n{" );

    std::string stageNameVarName = scopedPassName + "_" + STAGE_NAME_LUT[stageIndex] + "ShaderHashcode";
    generatedMetadata.append( "\tstatic constexpr const dkChar_t* " );
    generatedMetadata.append( stageNameVarName );
    generatedMetadata.append( " = DUSK_STRING( \"" );
    generatedMetadata.append( WideStringToString( filenameWithExtension ) );
    generatedMetadata.append( "\" );\n" );

    // Append stage name to the ShaderBinding variable
    shaderBindingDecl.append( stageNameVarName );
    if ( stageIndex != ( eShaderStage::SHADER_STAGE_COUNT - 1 ) ) {
        shaderBindingDecl.append( ", " );
    }

    if ( !semanticOutput.empty() ) {
        generatedShader.GeneratedSource.append( "\n" );
        generatedShader.GeneratedSource.append( STAGE_OUTPUT_NAME_LUT[stageIndex] );
        generatedShader.GeneratedSource.append( " output;\n" );
        generatedShader.GeneratedSource.append( processedSource );
        generatedShader.GeneratedSource.append( "return output;\n" );
    } else {
        generatedShader.GeneratedSource.append( processedSource );
    }
    generatedShader.GeneratedSource.append( "}" );

    generatedShaders.push_back( generatedShader );
}

void RenderLibraryGenerator::resetGeneratedContent()
{
    generatedLibraryName.clear();
    generatedMetadata.clear();
    generatedReflection.clear();
    generatedShaders.clear();
    generatedRenderPasses.clear();

    propertiesNode = nullptr;
    resourceListNode = nullptr;
    sharedShaderBody.clear();
    shadersSource.clear();
}

void RenderLibraryGenerator::appendSharedShaderHeader( std::string& hlslSource ) const
{
    // TODO Find a way to conditionally add this (e.g. not required for screenspace stuff).
    hlslSource.append( "#include <AutoExposure/Shared.hlsli>\n\n" );

    // PerViewBuffer
    hlslSource.append( "cbuffer PerViewBuffer : register( b0 )\n{\n" );
    hlslSource.append( "\tfloat4x4         g_ViewProjectionMatrix;\n" );
    hlslSource.append( "\tfloat4x4         g_InverseViewProjectionMatrix;\n" );
    hlslSource.append( "\tfloat4x4         g_PreviousViewProjectionMatrix;\n" );
    hlslSource.append( "\tfloat4x4         g_OrthoProjectionMatrix;\n" );
    hlslSource.append( "\tfloat2           g_ScreenSize;\n" );
    hlslSource.append( "\tfloat2           g_InverseScreenSize;\n" );
    hlslSource.append( "\tfloat3           g_WorldPosition;\n" );
    hlslSource.append( "\tint              g_FrameIndex;\n" );
    hlslSource.append( "\tfloat3           g_ViewDirection;\n" );
	hlslSource.append( "\tfloat            g_ImageQuality;\n" );
	hlslSource.append( "\tfloat2           g_CameraJitteringOffset;\n" );
	hlslSource.append( "\tuint2            g_CursorPosition;\n" );
	hlslSource.append( "\tfloat3           g_UpVector;\n" );
	hlslSource.append( "\tfloat            g_Fov;\n" );
	hlslSource.append( "\tfloat3           g_RightVector;\n" );
	hlslSource.append( "\tfloat            g_AspectRatio;\n" );
    hlslSource.append( "};\n" );

    // PerWorldBuffer
    // TODO Find a way to conditionally add this (e.g. not required for screenspace stuff).
    hlslSource.append( "#include <Light.h>\n\n" );

    hlslSource.append( "cbuffer PerWorldBuffer : register( b2 )\n{\n" );
    hlslSource.append( "\tDirectionalLightGPU   g_DirectionalLight;\n" );
    hlslSource.append( "\tfloat3	            g_ClustersScale;\n" );
    hlslSource.append( "\tfloat	                g_SceneAABBMinX;\n" );

    hlslSource.append( "\tfloat3	            g_ClustersInverseScale;\n" );
    hlslSource.append( "\tfloat	                g_SceneAABBMinY;\n" );

    hlslSource.append( "\tfloat3	            g_ClustersBias;\n" );
    hlslSource.append( "\tfloat	                g_SceneAABBMinZ;\n" );

    hlslSource.append( "\tfloat3	            g_SceneAABBMax;\n" );
    //hlslSource.append( "\tuint	            PADDING;\n" );

    //hlslSource.append( "\tPointLightGPU       g_PointLights[MAX_POINT_LIGHT_COUNT];\n" );
    //hlslSource.append( "\tIBLProbeGPU         g_IBLProbes[MAX_IBL_PROBE_COUNT];\n" );
    hlslSource.append( "};\n" );
}
