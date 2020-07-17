/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "Lexer.h"
#include <vector>

class FileSystemObject;

struct TypeAST {
    enum ePrimitiveType {
        PRIMITIVE_TYPE_ROIMAGECUBE,
        PRIMITIVE_TYPE_ROIMAGECUBE_ARRAY,
        PRIMITIVE_TYPE_ROIMAGE1D_ARRAY,
        PRIMITIVE_TYPE_ROIMAGE2D_ARRAY,
        PRIMITIVE_TYPE_I8,
        PRIMITIVE_TYPE_U8,
        PRIMITIVE_TYPE_I16,
        PRIMITIVE_TYPE_U16,
        PRIMITIVE_TYPE_I32,
        PRIMITIVE_TYPE_U32,
        PRIMITIVE_TYPE_I64,
        PRIMITIVE_TYPE_U64,
        PRIMITIVE_TYPE_F32,
        PRIMITIVE_TYPE_F64,
        PRIMITIVE_TYPE_BOOL,
        PRIMITIVE_TYPE_CFLAG,
        PRIMITIVE_TYPE_FLOAT2,
        PRIMITIVE_TYPE_FLOAT3,
        PRIMITIVE_TYPE_FLOAT4,
        PRIMITIVE_TYPE_ROIMAGE1D,
        PRIMITIVE_TYPE_ROIMAGE2D,
        PRIMITIVE_TYPE_ROIMAGE3D,
        PRIMITIVE_TYPE_RWIMAGE1D,
        PRIMITIVE_TYPE_RWIMAGE2D,
        PRIMITIVE_TYPE_RWIMAGE3D,
        PRIMITIVE_TYPE_ROBUFFER,
        PRIMITIVE_TYPE_RWBUFFER,
        PRIMITIVE_TYPE_ROSBUFFER,
        PRIMITIVE_TYPE_RWSBUFFER,
		PRIMITIVE_TYPE_SAMPLER,
		PRIMITIVE_TYPE_SAMPLER_COMPARISON,
        PRIMITIVE_TYPE_STRING,
        PRIMITIVE_TYPE_FLOAT4X4,
		PRIMITIVE_TYPE_CINT,
		PRIMITIVE_TYPE_RAWBUFFER,
		PRIMITIVE_TYPE_RWRAWBUFFER,
		PRIMITIVE_TYPE_ASBUFFER,
		PRIMITIVE_TYPE_CSBUFFER,
		PRIMITIVE_TYPE_UINT2,
		PRIMITIVE_TYPE_UINT3,
		PRIMITIVE_TYPE_UINT4,
        // TODO Anything below this line is temporary and should be DATA DRIVEN
        // since it has nothing todo with the parser itself.
        PRIMITIVE_TYPE_LINEINFO,
        PRIMITIVE_TYPE_ATMOSPHERE_PARAMS,
        PRIMITIVE_TYPE_NONE,
        PRIMITIVE_TYPE_COUNT
    };

    enum eTypes {
        NONE,
        PRIMITIVE,
        STRUCT,
        ENUM,
        SHADER,
        SHARED_CONTENT,
        RESOURCES,
        RESOURCE_ENTRY,
        PROPERTIES,
        PASS,
        LIBRARY,
        FONT,
        MATERIAL,
        RENDER_SCENARIO,
        SHADER_PERMUTATION,
        MATERIAL_PARAMETER,
    };

    eTypes                       Type;
    ePrimitiveType          PrimitiveType;
    Token::StreamRef     Name;

    std::vector<Token::StreamRef>   Names;
    std::vector<const TypeAST*>       Types;
    std::vector<Token::StreamRef>   Values;
    bool                                               Exportable;

    TypeAST()
        : Type( NONE )
        , PrimitiveType( PRIMITIVE_TYPE_NONE )
        , Name()
        , Exportable( true )
    {

    }
};

static constexpr const char* PRIMITIVE_TYPES[TypeAST::ePrimitiveType::PRIMITIVE_TYPE_COUNT] = {
    "TextureCube",
    "TextureCubeArray",
    "Texture1DArray",
    "Texture2DArray",
    "i8",
    "u8",
    "i16",
    "u16",
    "int",
    "uint",
    "i64",
    "u64",
    "float",
    "double",
    "bool",
    "cflag",
    "float2",
    "float3",
    "float4",
    "Texture1D",
    "Texture2D",
    "Texture3D",
    "RWTexture1D",
    "RWTexture2D",
    "RWTexture3D",
    "Buffer",
    "RWBuffer",
    "StructuredBuffer",
    "RWStructuredBuffer",
	"sampler",
	"SamplerComparisonState",
    "string",
    "float4x4",
    "cint",
    "ByteAddressBuffer",
    "RWByteAddressBuffer",
    "AppendStructuredBuffer",
    "ConsumeStructuredBuffer",
    "uint2",
    "uint3",
    "uint4",
    "LineInfos",
    "AtmosphereParameters",
    ""
};

// Size of ePrimitiveType (in bytes)
static constexpr size_t PRIMITIVE_TYPE_SIZE[TypeAST::ePrimitiveType::PRIMITIVE_TYPE_COUNT] =
{
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    2,
    4,
    4,
    8,
    8,
    4,
    8,
    1,
    0,
    8,
    12,
    16,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
	0,
	0,
    0,
    64,
    4,
    0,
    0,
    0,
    0,
    8,
    12,
    16,
    48,
    16,
    0,
};

class Parser
{
public:
    DUSK_INLINE u32         getTypeCount() const                { return typesCount; }
    DUSK_INLINE const TypeAST&    getType( const u32 typeIdx ) const  { return types[typeIdx]; }

public:
                    Parser( const char* text );
                    Parser( Parser& ) = delete;
                    Parser& operator = ( Parser& ) = delete;
                    ~Parser();

    void            generateAST();

private:
    static constexpr u64 MAX_TYPE_COUNT = 96;

private:
    Lexer           lexer;
    Token           currentToken;
    u32             typesCount;
    TypeAST         types[MAX_TYPE_COUNT];

private:
    void            parseLibrary();
    void            parseIdentifier( const Token& token );
    void            parseRenderPass( TypeAST& parentType );
    void            parseShaderBlock( const TypeAST::eTypes blockType,  TypeAST& parentType );
    void            parseStruct();
    void            parseEnum();
    void            parseResources( TypeAST& parentType );
    void            parseVariable( const Token::StreamRef& typeName, TypeAST& type, const bool isTypeless = false );
    void            parseFont();
    void            parseMaterial();

    void            parseShaderPermutation( Token& token, TypeAST& scenarioType );

    TypeAST*        getType( const Token::StreamRef& typeName );
};
