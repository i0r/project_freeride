/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "CodeGenerator.h"

#include <Core/Parser.h>
#include <Core/StringHelpers.h>
#include <Core/Hashing/MurmurHash3.h>
#include <Core/Hashing/Helpers.h>

#include <Rendering/RenderDevice.h>

#include <unordered_map>

extern LinearAllocator* g_GlobalAllocator;

void dk::baker::WriteEnum( const TypeAST& type )
{
    std::string values;
    std::string value_names;
    std::string generatedHeader;

    if ( type.Names.size() > 1 ) {
        const size_t maxValues = type.Names.size() - 1;
        for ( u32 v = 0; v < maxValues; ++v ) {
            values.append( type.Names[v].StreamPointer, type.Names[v].Length );
            values.append( ", " );
            value_names.append( type.Names[v].StreamPointer, type.Names[v].Length );
            value_names.append( "\", \"" );
        }

        values.append( type.Names[maxValues].StreamPointer, type.Names[maxValues].Length );

        value_names.append( type.Names[maxValues].StreamPointer, type.Names[maxValues].Length );
        value_names.append( "\"" );
    } else {
        values.append( type.Names[0].StreamPointer, type.Names[0].Length );
        value_names.append( type.Names[0].StreamPointer, type.Names[0].Length );
        value_names.append( "\"" );
    }

    values.append( ", Count" );
    value_names.append( ", \"Count\"" );

    generatedHeader.append( "namespace " );
    generatedHeader.append( type.Name.StreamPointer, type.Name.Length );
    generatedHeader.append( "  {\n" );

    generatedHeader.append( "\tenum Enum {\n" );
    generatedHeader.append( "\t\t" );
    generatedHeader.append( values.c_str() );
    generatedHeader.append( "\n" );
    generatedHeader.append( "\t};\n" );
    generatedHeader.append( "\n\tstatic const char* s_value_names[] = {\n" );
    generatedHeader.append( "\t\t\"" );
    generatedHeader.append( value_names.c_str() );
    generatedHeader.append( "\n" );

    generatedHeader.append( "\t};\n" );

    generatedHeader.append( "\n\tstatic const char* ToString( Enum e ) {\n" );
    generatedHeader.append( "\t\treturn s_value_names[(int)e];\n" );
    generatedHeader.append( "\t}\n" );

    generatedHeader.append( "}" );

    DUSK_LOG_INFO( "%hs\n", generatedHeader.c_str() );
}

void dk::baker::WriteStruct( const TypeAST& type, const bool useReflection )
{
    std::string reflectionCode;
    std::string generatedHeader;

    if ( useReflection ) {
        reflectionCode.append( "\n\tvoid reflectMembers() {\n" );
    }

    generatedHeader.append( "struct " );
    generatedHeader.append( type.Name.StreamPointer, type.Name.Length );
    generatedHeader.append( " {\n\n" );

    for ( u32 i = 0; i < type.Types.size(); ++i ) {
        const TypeAST& astType = *type.Types[i];
        const Token::StreamRef& astName = type.Names[i];

        switch ( astType.Type ) {
        case TypeAST::PRIMITIVE:
        {
            generatedHeader.append( "\t" );
            generatedHeader.append( PRIMITIVE_TYPES[astType.PrimitiveType] );
            generatedHeader.append( " " );
            generatedHeader.append( astName.StreamPointer, astName.Length );
            generatedHeader.append( ";\n" );
            break;
        }

        case TypeAST::STRUCT:
        {
            generatedHeader.append( "\t" );
            generatedHeader.append( astType.Name.StreamPointer, astType.Name.Length );
            generatedHeader.append( " " );
            generatedHeader.append( astName.StreamPointer, astName.Length );
            generatedHeader.append( ";\n" );
            break;
        }

        case TypeAST::ENUM:
        {
            generatedHeader.append( "\t" );
            generatedHeader.append( astType.Name.StreamPointer, astType.Name.Length );
            generatedHeader.append( "::Enum " );
            generatedHeader.append( astName.StreamPointer, astName.Length );
            generatedHeader.append( ";\n" );
            break;
        }

        default:
        {
            break;
        }
        }
    }

    generatedHeader.append( "}" );

    DUSK_LOG_INFO( "%hs\n", generatedHeader.c_str() );
}

DUSK_INLINE void StorePassStage( std::string* stageShaderNames, const i32 stageIdx, const Token::StreamRef& paramValue )
{
    //stageShaderNames[stageIdx].append( astName.StreamPointer, astName.Length );
    //stageShaderNames[stageIdx].append( "/" );
    stageShaderNames[stageIdx].append( paramValue.StreamPointer, paramValue.Length );
}

DUSK_INLINE static bool IsReadOnlyResourceType( const TypeAST::ePrimitiveType type )
{
    return type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROBUFFER
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROSBUFFER
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE1D
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE2D
        || type == TypeAST::ePrimitiveType::PRIMITIVE_TYPE_ROIMAGE3D;
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

struct RenderPassInfos 
{
    std::string                 StageShaderNames[eShaderStage::SHADER_STAGE_COUNT]; // Vertex, TEval, TComp, Pixel, Compute
    PipelineStateDesc::Type     PipelineStateType; // Compute or Graphics PSO

    i32 DispatchX;
    i32 DispatchY;
    i32 DispatchZ;

    std::vector<std::string>    ColorRenderTargets;
    std::string                 DepthStencilBuffer;

    RenderPassInfos()
        : PipelineStateType( PipelineStateDesc::GRAPHICS )
        , DispatchX( 0 )
        , DispatchY( 0 )
        , DispatchZ( 0 )
    {

    }
};

static DUSK_INLINE void ParseRenderPassProperties( const TypeAST& renderPassBlock, TypeAST* properties, RenderPassInfos& passInfos )
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
        } else if ( paramType != nullptr && properties != nullptr ) {
            // If the name does not match an identifier, assume it is a property override
            for ( u32 i = 0; i < properties->Values.size(); i++ ) {
                if ( dk::core::ExpectKeyword( passParam.StreamPointer, passParam.Length, properties->Names[i].StreamPointer )
                     && properties->Types[i]->PrimitiveType == paramType->PrimitiveType ) {
                    properties->Values[i] = paramValue;
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
            hlslSourceOutput.append( ";\n" );
        }
    }

    hlslSourceOutput.append( "};\n\n" );
}

static DUSK_INLINE void WriteResourceList( const TypeAST* resourceList, const TypeAST* properties, std::string& hlslSource )
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
                    registerAddr.append( "u" );
                    registerAddr.append( std::to_string( uavRegisterIdx++ ) );
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

static constexpr size_t SEMANTIC_COUNT = 120;

static constexpr dkStringHash_t SEMANTIC_HASHTABLE[SEMANTIC_COUNT] = {
    DUSK_STRING_HASH( "sv_clipdistance" ),
    DUSK_STRING_HASH( "sv_culldistance" ),
    DUSK_STRING_HASH( "sv_coverage" ),
    DUSK_STRING_HASH( "sv_depth" ),
    DUSK_STRING_HASH( "sv_dispatchthreadid" ),
    DUSK_STRING_HASH( "sv_domainlocation" ),
    DUSK_STRING_HASH( "sv_groupid" ),
    DUSK_STRING_HASH( "sv_groupindex" ),
    DUSK_STRING_HASH( "sv_groupthreadid" ),
    DUSK_STRING_HASH( "sv_gsinstanceid" ),
    DUSK_STRING_HASH( "sv_insidetessfactor" ),
    DUSK_STRING_HASH( "sv_instanceid" ),
    DUSK_STRING_HASH( "sv_isfrontface" ),
    DUSK_STRING_HASH( "sv_outputcontrolpointid" ),
    DUSK_STRING_HASH( "sv_position" ),
    DUSK_STRING_HASH( "sv_primitiveid" ),
    DUSK_STRING_HASH( "sv_rendertargetarrayindex" ),
    DUSK_STRING_HASH( "sv_sampleindex" ),
    DUSK_STRING_HASH( "sv_stencilref" ),
    DUSK_STRING_HASH( "sv_target0" ),
    DUSK_STRING_HASH( "sv_target1" ),
    DUSK_STRING_HASH( "sv_target2" ),
    DUSK_STRING_HASH( "sv_target3" ),
    DUSK_STRING_HASH( "sv_target4" ),
    DUSK_STRING_HASH( "sv_target5" ),
    DUSK_STRING_HASH( "sv_target6" ),
    DUSK_STRING_HASH( "sv_target7" ),
    DUSK_STRING_HASH( "sv_tessfactor" ),
    DUSK_STRING_HASH( "sv_vertexid" ),
    DUSK_STRING_HASH( "sv_viewportarrayindex" ),

    DUSK_STRING_HASH( "position" ),
    DUSK_STRING_HASH( "position0" ),
    DUSK_STRING_HASH( "position1" ),
    DUSK_STRING_HASH( "position2" ),
    DUSK_STRING_HASH( "position3" ),
    DUSK_STRING_HASH( "position4" ),
    DUSK_STRING_HASH( "position5" ),
    DUSK_STRING_HASH( "position6" ),
    DUSK_STRING_HASH( "position7" ),

    DUSK_STRING_HASH( "normal" ),
    DUSK_STRING_HASH( "normal0" ),
    DUSK_STRING_HASH( "normal1" ),
    DUSK_STRING_HASH( "normal2" ),
    DUSK_STRING_HASH( "normal3" ),
    DUSK_STRING_HASH( "normal4" ),
    DUSK_STRING_HASH( "normal5" ),
    DUSK_STRING_HASH( "normal6" ),
    DUSK_STRING_HASH( "normal7" ),

    DUSK_STRING_HASH( "tangent" ),
    DUSK_STRING_HASH( "tangent0" ),
    DUSK_STRING_HASH( "tangent1" ),
    DUSK_STRING_HASH( "tangent2" ),
    DUSK_STRING_HASH( "tangent3" ),
    DUSK_STRING_HASH( "tangent4" ),
    DUSK_STRING_HASH( "tangent5" ),
    DUSK_STRING_HASH( "tangent6" ),
    DUSK_STRING_HASH( "tangent7" ),

    DUSK_STRING_HASH( "texcoord" ),
    DUSK_STRING_HASH( "texcoord0" ),
    DUSK_STRING_HASH( "texcoord1" ),
    DUSK_STRING_HASH( "texcoord2" ),
    DUSK_STRING_HASH( "texcoord3" ),
    DUSK_STRING_HASH( "texcoord4" ),
    DUSK_STRING_HASH( "texcoord5" ),
    DUSK_STRING_HASH( "texcoord6" ),
    DUSK_STRING_HASH( "texcoord7" ),

    DUSK_STRING_HASH( "color" ),
    DUSK_STRING_HASH( "color0" ),
    DUSK_STRING_HASH( "color1" ),
    DUSK_STRING_HASH( "color2" ),
    DUSK_STRING_HASH( "color3" ),
    DUSK_STRING_HASH( "color4" ),
    DUSK_STRING_HASH( "color5" ),
    DUSK_STRING_HASH( "color6" ),
    DUSK_STRING_HASH( "color7" ),

    DUSK_STRING_HASH( "depth" ),
    DUSK_STRING_HASH( "depth0" ),
    DUSK_STRING_HASH( "depth1" ),
    DUSK_STRING_HASH( "depth2" ),
    DUSK_STRING_HASH( "depth3" ),
    DUSK_STRING_HASH( "depth4" ),
    DUSK_STRING_HASH( "depth5" ),
    DUSK_STRING_HASH( "depth6" ),
    DUSK_STRING_HASH( "depth7" ),

    DUSK_STRING_HASH( "binormal" ),
    DUSK_STRING_HASH( "binormal0" ),
    DUSK_STRING_HASH( "binormal1" ),
    DUSK_STRING_HASH( "binormal2" ),
    DUSK_STRING_HASH( "binormal3" ),
    DUSK_STRING_HASH( "binormal4" ),
    DUSK_STRING_HASH( "binormal5" ),
    DUSK_STRING_HASH( "binormal6" ),
    DUSK_STRING_HASH( "binormal7" ),

    DUSK_STRING_HASH( "blendindices" ),
    DUSK_STRING_HASH( "blendindices0" ),
    DUSK_STRING_HASH( "blendindices1" ),
    DUSK_STRING_HASH( "blendindices2" ),
    DUSK_STRING_HASH( "blendindices3" ),
    DUSK_STRING_HASH( "blendindices4" ),
    DUSK_STRING_HASH( "blendindices5" ),
    DUSK_STRING_HASH( "blendindices6" ),
    DUSK_STRING_HASH( "blendindices7" ),

    DUSK_STRING_HASH( "blendweight" ),
    DUSK_STRING_HASH( "blendweight0" ),
    DUSK_STRING_HASH( "blendweight1" ),
    DUSK_STRING_HASH( "blendweight2" ),
    DUSK_STRING_HASH( "blendweight3" ),
    DUSK_STRING_HASH( "blendweight4" ),
    DUSK_STRING_HASH( "blendweight5" ),
    DUSK_STRING_HASH( "blendweight6" ),
    DUSK_STRING_HASH( "blendweight7" ),

    DUSK_STRING_HASH( "psize" ),
    DUSK_STRING_HASH( "psize0" ),
    DUSK_STRING_HASH( "psize1" ),
    DUSK_STRING_HASH( "psize2" ),
    DUSK_STRING_HASH( "psize3" ),
    DUSK_STRING_HASH( "psize4" ),
    DUSK_STRING_HASH( "psize5" ),
    DUSK_STRING_HASH( "psize6" ),
    DUSK_STRING_HASH( "psize7" ),
};

static constexpr const char* SEMANTIC_NAME[SEMANTIC_COUNT] = {
    "SV_ClipDistance",
    "SV_CullDistance",
    "SV_Coverage",
    "SV_Depth",
    "SV_DispatchThreadID",
    "SV_DomainLocation",
    "SV_GroupID",
    "SV_GroupIndex",
    "SV_GroupThreadID",
    "SV_GSInstanceID",
    "SV_InsideTessFactor",
    "SV_InstanceID",
    "SV_IsFrontFace",
    "SV_OutputControlPointID",
    "SV_Position",
    "SV_PrimitiveID",
    "SV_RenderTargetArrayIndex",
    "SV_SampleIndex",
    "SV_StencilRef",
    "SV_Target0",
    "SV_Target1",
    "SV_Target2",
    "SV_Target3",
    "SV_Target4",
    "SV_Target5",
    "SV_Target6",
    "SV_Target7",
    "SV_TessFactor",
    "SV_VertexID",
    "SV_ViewportArrayIndex",

    "POSITION",
    "POSITION0",
    "POSITION1",
    "POSITION2",
    "POSITION3",
    "POSITION4",
    "POSITION5",
    "POSITION6",
    "POSITION7",

    "NORMAL",
    "NORMAL0",
    "NORMAL1",
    "NORMAL2",
    "NORMAL3",
    "NORMAL4",
    "NORMAL5",
    "NORMAL6",
    "NORMAL7",

    "TANGENT",
    "TANGENT0",
    "TANGENT1",
    "TANGENT2",
    "TANGENT3",
    "TANGENT4",
    "TANGENT5",
    "TANGENT6",
    "TANGENT7",

    "TEXCOORD",
    "TEXCOORD0",
    "TEXCOORD1",
    "TEXCOORD2",
    "TEXCOORD3",
    "TEXCOORD4",
    "TEXCOORD5",
    "TEXCOORD6",
    "TEXCOORD7",

    "COLOR",
    "COLOR0",
    "COLOR1",
    "COLOR2",
    "COLOR3",
    "COLOR4",
    "COLOR5",
    "COLOR6",
    "COLOR7",

    "DEPTH",
    "DEPTH0",
    "DEPTH1",
    "DEPTH2",
    "DEPTH3",
    "DEPTH4",
    "DEPTH5",
    "DEPTH6",
    "DEPTH7",

    "BINORMAL",
    "BINORMAL0",
    "BINORMAL1",
    "BINORMAL2",
    "BINORMAL3",
    "BINORMAL4",
    "BINORMAL5",
    "BINORMAL6",
    "BINORMAL7",

    "BLENDINDICES",
    "BLENDINDICES0",
    "BLENDINDICES1",
    "BLENDINDICES2",
    "BLENDINDICES3",
    "BLENDINDICES4",
    "BLENDINDICES5",
    "BLENDINDICES6",
    "BLENDINDICES7",

    "BLENDWEIGHT",
    "BLENDWEIGHT0",
    "BLENDWEIGHT1",
    "BLENDWEIGHT2",
    "BLENDWEIGHT3",
    "BLENDWEIGHT4",
    "BLENDWEIGHT5",
    "BLENDWEIGHT6",
    "BLENDWEIGHT7",

    "PSIZE",
    "PSIZE0",
    "PSIZE1",
    "PSIZE2",
    "PSIZE3",
    "PSIZE4",
    "PSIZE5",
    "PSIZE6",
    "PSIZE7",
};

static constexpr const char* SEMANTIC_SWIZZLE[SEMANTIC_COUNT] = {
    "float",
    "float",
    "uint",
    "float",
    "uint3",
    "float3",
    "uint3",
    "uint",
    "uint3",
    "uint",
    "float[2]",
    "uint",
    "bool",
    "uint",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "uint",
    "uint",
    "uint",
    "uint",
    "float4",
    "float4",
    "uint",
    "uint",

    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",

    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",

    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",

    "float2",
    "float2",
    "float2",
    "float2",
    "float2",
    "float2",
    "float2",
    "float2",
    "float2",

    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",

    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",

    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",
    "float4",

    "uint",
    "uint",
    "uint",
    "uint",
    "uint",
    "uint",
    "uint",
    "uint",
    "uint",

    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",

    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
    "float",
};

#define DECLARE_WRITE_ACCESS( vs, te, tc, px, cmp ) ( ( vs & 1 )\
| ( te & 1 ) << 1\
| ( tc & 1 ) << 2\
| ( px & 1 ) << 3\
| ( cmp & 1 ) << 4 )

static constexpr i32 SEMANTIC_ACCESS[SEMANTIC_COUNT] = {
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, true, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, true, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),
    DECLARE_WRITE_ACCESS( false, false, false, true, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),

    DECLARE_WRITE_ACCESS( false, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
    DECLARE_WRITE_ACCESS( true, false, false, false, false ),
};

#undef DECLARE_WRITE_ACCESS

// Pass Input Struct (data received from vbo or previous stage binded in the pipeline)
static constexpr const char* STAGE_INPUT_NAME_LUT[5] = {
    "VertexInput",
    "TesselationControlInput",
    "TesselationEvalInput",
    "PixelInput",
    "ComputeInput"
};

static constexpr const char* STAGE_NAME_LUT[5] = {
    "Vertex",
    "TesselationControl",
    "TesselationEval",
    "Pixel",
    "Compute"
};

// Pass Output Struct (data sent to the stage binded in the pipeline)
static constexpr const char* STAGE_OUTPUT_NAME_LUT[5] = {
    "VertexOuput",
    "TesselationControlOutput",
    "TesselationEvalOutput",
    "PixelOutput",
    "void" // Compute kernels have no return type!
};

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
            } else if ( tokenValue == "else" ) {
                srcCodeLine.append( "else\n" );
            } else if ( tokenValue == "ifdef" ) {
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
                            srcCodeLine.append( flagIt->second->StreamPointer, flagIt->second->Length );
                        } else {
                            DUSK_LOG_WARN( "Unknown cflag specified by preprocessor guards: '%s'\n", semanticName.c_str() );
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
                            srcCodeLine.append( flagIt->second->StreamPointer, flagIt->second->Length );
                        } else {
                            DUSK_LOG_WARN( "Unknown cflag specified by preprocessor guards: '%s'\n", semanticName.c_str() );
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

void dk::baker::WriteRenderLibrary( const TypeAST& type, std::string& libraryName, std::string& generatedHeader, std::string& reflectionHeader, std::vector<GeneratedShader>& generatedShaders )
{
    std::string sharedShaderBody;
    std::unordered_map<dkStringHash_t, const Token::StreamRef*> shadersSource;
    TypeAST* properties = nullptr;
    const TypeAST* resourceList = nullptr;

    libraryName = std::string( type.Name.StreamPointer, type.Name.Length );

    generatedHeader.append( "// This file has been automatically generated by Dusk Baker\n" );
    generatedHeader.append( "#pragma once\n\n" );
    generatedHeader.append( "#include <Graphics/FrameGraph.h>\n\n" );
    generatedHeader.append( "#include <Graphics/PipelineStateCache.h>\n\n" );
    generatedHeader.append( "namespace " );
    generatedHeader.append( type.Name.StreamPointer, type.Name.Length );
    generatedHeader.append( "\n{\n" );


    reflectionHeader.append( "// This file has been automatically generated by Dusk Baker\n" );
    reflectionHeader.append( "#pragma once\n\n#if DUSK_USE_IMGUI\n" );
    reflectionHeader.append( "#include <ThirdParty/imgui/imgui.h>\n" );
    reflectionHeader.append( "#include <Graphics/RenderModules/" );
    reflectionHeader.append( libraryName );
    reflectionHeader.append( ".generated.h>\n\n" );
    reflectionHeader.append( "namespace " );
    reflectionHeader.append( type.Name.StreamPointer, type.Name.Length );
    reflectionHeader.append( "\n{\n" );

    for ( u32 i = 0; i < type.Types.size(); ++i ) {
        const TypeAST& astType = *type.Types[i];
        const Token::StreamRef& astName = type.Names[i];

        switch ( astType.Type ) {
        case TypeAST::PROPERTIES:
        {
            // HACK We need to un-const the property list in order to do properties override in place
            properties = const_cast< TypeAST* >( type.Types[i] );
        } break;

        case TypeAST::RESOURCES:
        {
            resourceList = type.Types[i];
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
            // Parse RenderPass properties (override library properties if needed)
            RenderPassInfos passInfos;
            ParseRenderPassProperties( astType, properties, passInfos );

            std::string psoDescDecl; // PSO Descriptor (stages to retrieve, pipeline setup, etc.)
            std::string shaderBindingDecl;

            std::string scopedPassName( astName.StreamPointer, astName.Length );

            shaderBindingDecl.append( "\tstatic constexpr PipelineStateCache::ShaderBinding " );
            shaderBindingDecl.append( scopedPassName );
            shaderBindingDecl.append( "_ShaderBinding = PipelineStateCache::ShaderBinding( " );
            // Generate Shader Stage Permutation for the current RenderPass
            for ( i32 i = 0; i < eShaderStage::SHADER_STAGE_COUNT; i++ ) {
                if ( passInfos.StageShaderNames[i].empty() ) {
                    shaderBindingDecl.append( "nullptr" );

                    if ( i != ( eShaderStage::SHADER_STAGE_COUNT - 1 ) ) {
                        shaderBindingDecl.append( ", " );
                    }
                    continue;
                }

                const dkStringHash_t shaderHashcode = dk::core::CRC32( passInfos.StageShaderNames[i] );
                auto it = shadersSource.find( shaderHashcode );
                DUSK_RAISE_FATAL_ERROR( it != shadersSource.end(), "Unknown shader stage!" );

                static constexpr eShaderStage SHADER_STAGE_LUT[eShaderStage::SHADER_STAGE_COUNT] = {
                    SHADER_STAGE_VERTEX,
                    SHADER_STAGE_TESSELATION_CONTROL,
                    SHADER_STAGE_TESSELATION_EVALUATION,
                    SHADER_STAGE_PIXEL,
                    SHADER_STAGE_COMPUTE
                };

                static constexpr const char* SHADER_STAGE_NAME_LUT[eShaderStage::SHADER_STAGE_COUNT] = {
                    "vertex",
                    "tesselationControl",
                    "tesselationEvaluation",
                    "pixel",
                    "compute"
                };

                passInfos.StageShaderNames[i].append( SHADER_STAGE_NAME_LUT[i] );

                Hash128 permutationHashcode;
                MurmurHash3_x64_128( passInfos.StageShaderNames[i].c_str(), static_cast< int >( passInfos.StageShaderNames[i].size() ), 19081996, &permutationHashcode );
                dkString_t filenameWithExtension = dk::core::GetHashcodeDigest128( permutationHashcode );

                GeneratedShader generatedShader;
                generatedShader.ShaderName = WideStringToString( filenameWithExtension );
                generatedShader.ShaderStage = SHADER_STAGE_LUT[i];
                generatedShader.GeneratedSource.clear();

                generatedShader.GeneratedSource.append( "cbuffer PerViewBuffer : register( b0 )\n{\n" );
                generatedShader.GeneratedSource.append( "\tfloat4x4         g_ViewProjectionMatrix;\n" );
                generatedShader.GeneratedSource.append( "\tfloat4x4         g_InverseViewProjectionMatrix;\n" );
                generatedShader.GeneratedSource.append( "\tfloat2           g_ScreenSize;\n" );
                generatedShader.GeneratedSource.append( "\tfloat2           g_InverseScreenSize;\n" );
                generatedShader.GeneratedSource.append( "\tfloat3           g_WorldPosition;\n" );
                generatedShader.GeneratedSource.append( "\tint              g_FrameIndex;\n" );
                generatedShader.GeneratedSource.append( "};\n" );

                generatedShader.GeneratedSource.append( "#include <Light.h>\n\n" );

                generatedShader.GeneratedSource.append( "cbuffer PerWorldBuffer : register( b2 )\n{\n" );
                generatedShader.GeneratedSource.append( "\tDirectionalLightGPU g_DirectionalLight;\n" );
                generatedShader.GeneratedSource.append( "\tfloat3	        g_ClustersScale;\n" );
                generatedShader.GeneratedSource.append( "\tfloat	        g_SceneAABBMinX;\n" );

                generatedShader.GeneratedSource.append( "\tfloat3	        g_ClustersInverseScale;\n" );
                generatedShader.GeneratedSource.append( "\tfloat	        g_SceneAABBMinY;\n" );

                generatedShader.GeneratedSource.append( "\tfloat3	        g_ClustersBias;\n" );
                generatedShader.GeneratedSource.append( "\tfloat	        g_SceneAABBMinZ;\n" );

                generatedShader.GeneratedSource.append( "\tfloat3	        g_SceneAABBMax;\n" );
                //generatedShader.GeneratedSource.append( "\tuint	            PADDING;\n" );

                //generatedShader.GeneratedSource.append( "\tPointLightGPU       g_PointLights[MAX_POINT_LIGHT_COUNT];\n" );
                //generatedShader.GeneratedSource.append( "\tIBLProbeGPU         g_IBLProbes[MAX_IBL_PROBE_COUNT];\n" );
                generatedShader.GeneratedSource.append( "};\n" );

                std::unordered_map<dkStringHash_t, const Token::StreamRef*> constantMap;
                std::unordered_map<dkStringHash_t, std::string> semanticInput;
                std::unordered_map<dkStringHash_t, std::string> semanticOutput;

                // Write per renderpass/view cbuffers and parse compile-time flags
                WriteConstantBufferDecl( properties, "PerPassBuffer", 1, constantMap, generatedShader.GeneratedSource );

                // NOTE We want the shared header first in the hlsl source (since it might add includes to the file
                // used by the resource list)
                PreprocessShaderSource( sharedShaderBody, i, constantMap, generatedShader.GeneratedSource, semanticInput, semanticOutput );

                // Create resource list
                WriteResourceList( resourceList, properties, generatedShader.GeneratedSource );

                // Preprocess shader source code
                std::string processedSource;
                std::string bodySource( it->second->StreamPointer, it->second->Length );
                PreprocessShaderSource( bodySource, i, constantMap, processedSource, semanticInput, semanticOutput );

                // Write Stage input (based on system value semantics used in the source code)
                WritePassInOutData( semanticInput, STAGE_INPUT_NAME_LUT[i], generatedShader.GeneratedSource );
                WritePassInOutData( semanticOutput, STAGE_OUTPUT_NAME_LUT[i], generatedShader.GeneratedSource );

                if ( i == 4 ) {
                    generatedShader.GeneratedSource.append( "[numthreads( " );
                    generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchX ) );
                    generatedShader.GeneratedSource.append( ", " );
                    generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchY ) );
                    generatedShader.GeneratedSource.append( ", " );
                    generatedShader.GeneratedSource.append( std::to_string( passInfos.DispatchZ ) );
                    generatedShader.GeneratedSource.append( " )]\n" );
                }

                generatedShader.GeneratedSource.append( STAGE_OUTPUT_NAME_LUT[i] );
                generatedShader.GeneratedSource.append( " EntryPoint( " );

                if ( !semanticInput.empty() ) {
                    generatedShader.GeneratedSource.append( STAGE_INPUT_NAME_LUT[i] );
                    generatedShader.GeneratedSource.append( " input " );
                }

                generatedShader.GeneratedSource.append( ")\n{" );

                std::string stageNameVarName = scopedPassName + "_" + STAGE_NAME_LUT[i] + "ShaderHashcode";
                psoDescDecl.append( "\tstatic constexpr const dkChar_t* " );
                psoDescDecl.append( stageNameVarName );
                psoDescDecl.append( " = DUSK_STRING( \"" );
                psoDescDecl.append( WideStringToString( filenameWithExtension ) );
                psoDescDecl.append( "\" );\n" );

                // Append stage name to the ShaderBinding variable
                shaderBindingDecl.append( stageNameVarName );
                if ( i != ( eShaderStage::SHADER_STAGE_COUNT - 1 ) ) {
                    shaderBindingDecl.append( ", " );
                }

                if ( !semanticOutput.empty() ) {
                    generatedShader.GeneratedSource.append( "\n" );
                    generatedShader.GeneratedSource.append( STAGE_OUTPUT_NAME_LUT[i] );
                    generatedShader.GeneratedSource.append( " output;\n" );
                    generatedShader.GeneratedSource.append( processedSource );
                    generatedShader.GeneratedSource.append( "return output;\n" );
                } else {
                    generatedShader.GeneratedSource.append( processedSource );
                }
                generatedShader.GeneratedSource.append( "}" );

                generatedShaders.push_back( generatedShader );
            }

            shaderBindingDecl.append( " );\n" );

            std::string passInputResDecl; // Resources consumed by the pass
            std::vector<u32> outputResIndexes;
            std::string resourceListDecl; // PassData resources
            std::string reflectedResourceBinding; // Reflected hashes for each binded resource
            for ( u32 i = 0; i < resourceList->Names.size(); i++ ) {
                const TypeAST::ePrimitiveType primType = resourceList->Types[i]->PrimitiveType;
                const std::string resName( resourceList->Names[i].StreamPointer, resourceList->Names[i].Length );
                
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

                if ( isReadOnlyRes ) {
                    passInputResDecl.append( ", ResHandle_t " );
                    passInputResDecl.append( resName );
                } else {
                    if ( primType == TypeAST::PRIMITIVE_TYPE_SAMPLER ) {
                        continue;
                    }

                    outputResIndexes.push_back( i );
                }

                resourceListDecl.append( "\t\tResHandle_t\t" );
                resourceListDecl.append( resName );
                resourceListDecl.append( ";\n" );

                reflectedResourceBinding.append( "\tstatic constexpr dkStringHash_t " );
                reflectedResourceBinding.append( scopedPassName );
                reflectedResourceBinding.append( "_" );
                reflectedResourceBinding.append( resName );
                reflectedResourceBinding.append( "_Hashcode = DUSK_STRING_HASH( \"" );
                reflectedResourceBinding.append( resName );
                reflectedResourceBinding.append( "\" );\n" );
            }

            bool hasMultipleOutput = ( outputResIndexes.size() > 1 );
            if ( hasMultipleOutput ) {
                generatedHeader.append( "\tstruct " );
                generatedHeader.append( scopedPassName );
                generatedHeader.append( "_Output {\n" );

                for ( u32 resIdx : outputResIndexes ) {
                    if ( resourceList->Types[resIdx]->PrimitiveType == TypeAST::PRIMITIVE_TYPE_SAMPLER ) {
                        continue;
                    }

                    generatedHeader.append( "\t\tResHandle_t\t" );
                    generatedHeader.append( resourceList->Names[resIdx].StreamPointer, resourceList->Names[resIdx].Length );
                    generatedHeader.append( ";\n" );
                }

                generatedHeader.append( "\t};\n\n" );
            }

            //// Resource List
            //generatedHeader.append( "\tstruct " );
            //generatedHeader.append( scopedPassName );
            //generatedHeader.append( "_PassData\n\t{\n" );
            //generatedHeader.append( resourceListDecl );
            //generatedHeader.append( "\t};\n\n" );

            generatedHeader.append( reflectedResourceBinding );
            generatedHeader.append( psoDescDecl );

            if ( passInfos.PipelineStateType == PipelineStateDesc::COMPUTE ) {
                generatedHeader.append( "\tstatic constexpr u32 " );
                generatedHeader.append( scopedPassName );
                generatedHeader.append( "_DispatchX = " );
                generatedHeader.append( std::to_string( passInfos.DispatchX ) );
                generatedHeader.append( "u;\n" );

                generatedHeader.append( "\tstatic constexpr u32 " );
                generatedHeader.append( scopedPassName );
                generatedHeader.append( "_DispatchY = " );
                generatedHeader.append( std::to_string( passInfos.DispatchY ) );
                generatedHeader.append( "u;\n" );

                generatedHeader.append( "\tstatic constexpr u32 " );
                generatedHeader.append( scopedPassName );
                generatedHeader.append( "_DispatchZ = " );
                generatedHeader.append( std::to_string( passInfos.DispatchZ ) );
                generatedHeader.append( "u;\n" );
            }

            generatedHeader.append( "\tstatic constexpr const char* " );
            generatedHeader.append( scopedPassName );
            generatedHeader.append( "_Name = \"" );
            generatedHeader.append( type.Name.StreamPointer, type.Name.Length );
            generatedHeader.append( "::" );
            generatedHeader.append( astName.StreamPointer, astName.Length );
            generatedHeader.append( "\";\n" );

            generatedHeader.append( "\tstatic constexpr const dkChar_t* " );
            generatedHeader.append( scopedPassName );
            generatedHeader.append( "_EventName = DUSK_STRING( \"" );
            generatedHeader.append( type.Name.StreamPointer, type.Name.Length );
            generatedHeader.append( "::" );
            generatedHeader.append( astName.StreamPointer, astName.Length );
            generatedHeader.append( "\" );\n" );

            generatedHeader.append( shaderBindingDecl );

            // Reflection API
            if ( properties != nullptr ) {
                std::string propertyDecl;
                propertyDecl.append( "\n\tstatic struct " );
                propertyDecl.append( scopedPassName );
                propertyDecl.append( "RuntimeProperties\n\t{\n" );
                    
                std::string reflectionApiDecl;
                reflectionApiDecl.append( "\tstatic void Reflect" );
                reflectionApiDecl.append( scopedPassName );
                reflectionApiDecl.append( "( " );
                reflectionApiDecl.append( scopedPassName );
                reflectionApiDecl.append( "RuntimeProperties& properties )\n\t{ \n" );

                reflectionApiDecl.append( "\t\tImGui::LabelText( \"" );
                reflectionApiDecl.append( scopedPassName );
                reflectionApiDecl.append( "\", \"" );
                reflectionApiDecl.append( scopedPassName );
                reflectionApiDecl.append( "\" );\n" );

                size_t cbufferTotalSize = 0;
                std::list<CBufferLine> perPassBufferLines; // PerPass cbuffer layout (with a 16bytes alignment)
                for ( u32 i = 0; i < properties->Values.size(); i++ ) {
                    const TypeAST::ePrimitiveType primType = properties->Types[i]->PrimitiveType;

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
                    entry.Entry = properties->Types[i];
                    entry.Name = &properties->Names[i];
                    entry.Value = ( properties->Values[i].StreamPointer != nullptr ) ? &properties->Values[i] : nullptr;

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

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", -128, +127 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_U8:
                            propertyDecl.append( "u8" );

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", 0, +255 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_I16:
                            propertyDecl.append( "i16" );

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", -32768, +32767 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_U16:
                            propertyDecl.append( "u16" );

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", 0, +65535 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_I32:
                            propertyDecl.append( "i32" );

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", -2147483648, +2147483647 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_U32:
                            propertyDecl.append( "u32" );

                            reflectionApiDecl.append( "\t\tImGui::InputInt( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", (int*)&properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ", 0, +4294967295 );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_F32:
                            propertyDecl.append( "f32" );

                            reflectionApiDecl.append( "\t\tImGui::DragFloat( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", &properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( " );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_BOOL:
                            propertyDecl.append( "bool" );

                            reflectionApiDecl.append( "\t\tImGui::Checkbox( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", &properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( " );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_FLOAT2:
                            propertyDecl.append( "dkVec2f" );

                            reflectionApiDecl.append( "\t\tImGui::DragFloat2( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", &properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ".x );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_FLOAT3:
                            propertyDecl.append( "dkVec3f" );

                            reflectionApiDecl.append( "\t\tImGui::DragFloat3( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", &properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ".x );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_FLOAT4:
                            propertyDecl.append( "dkVec4f" );

                            reflectionApiDecl.append( "\t\tImGui::DragFloat4( \"" );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( "\", &properties." );
                            reflectionApiDecl.append( propertyName );
                            reflectionApiDecl.append( ".x );\n" );
                            break;
                        case TypeAST::PRIMITIVE_TYPE_FLOAT4X4:
                            propertyDecl.append( "dkMat4x4f" );
                            break;
                        default:
                            break;
                        };

                        propertyDecl.append( " " );
                        propertyDecl.append( propertyName );
                        if ( entry.Value != nullptr ) {
                            propertyDecl.append( " = " );
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

                reflectionApiDecl.append( "\t}\n" );

                generatedHeader.append( propertyDecl );
                reflectionHeader.append( reflectionApiDecl );
            }

            break;
        }
        }
    }

    generatedHeader.append( "}\n" );

    reflectionHeader.append( "}\n" );
    reflectionHeader.append( "#endif\n" );
}
