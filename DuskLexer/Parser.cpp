/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Parser.h"

#include "Lexer.h"

#include <Io/TextStreamHelpers.h>
#include <Core/StringHelpers.h>
#include <string.h>


Parser::Parser( const char* text )
    : lexer( text )
    , currentToken()
    , typesCount( TypeAST::ePrimitiveType::PRIMITIVE_TYPE_COUNT )
{
    for ( u32 i = 0; i < typesCount; ++i ) {
        TypeAST& primType = types[i];
        primType.Type = TypeAST::PRIMITIVE;
        primType.Name.Length = static_cast<u32>( strlen_c( PRIMITIVE_TYPES[i] ) );
        primType.Name.StreamPointer = PRIMITIVE_TYPES[i];
        primType.PrimitiveType = static_cast<TypeAST::ePrimitiveType>( i );
    }
}

Parser::~Parser()
{

}

void Parser::generateAST()
{
    bool parsing = true;

    while ( parsing ) {
        lexer.nextToken( currentToken );

        switch ( currentToken.type ) {
        case Token::IDENTIFIER: {
            parseIdentifier( currentToken );
            break;
        }
        case Token::END_OF_STREAM: {
            parsing = false;
            break;
        }
        default:
            break;
        }
    }
}

void Parser::parseRenderPass( TypeAST& parentType )
{
    Token token;
    if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
        return;
    }

    Token::StreamRef name = token.streamReference;

    if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
        return;
    }

    TypeAST& type = types[typesCount++];
    type.Name = name;
    type.Type = TypeAST::PASS;
    type.Exportable = true;

    Token param;
    while ( !lexer.equalToken( Token::CLOSE_BRACE, param ) ) {
        if ( param.type != Token::IDENTIFIER ) {
            continue;
        }

        Token::StreamRef varTypeSr = param.streamReference;
        Token::StreamRef name = param.streamReference;

        Token::StreamRef value;
        value.StreamPointer = nullptr;
        value.Length = 0;

        lexer.nextToken( param );
        // Take in account type for property override (e.g. float myVar = 1;)
        if ( param.type == Token::IDENTIFIER ) {
             name = param.streamReference;
             lexer.nextToken( param );
        }

        // Variable has an initializer value
        if ( param.type == Token::EQUALS ) {
            lexer.nextToken( param );

            value.StreamPointer = param.streamReference.StreamPointer;

            while ( !lexer.equalToken( Token::SEMICOLON, param ) );

            value.Length = static_cast<u32>(  param.streamReference.StreamPointer - value.StreamPointer );
        }

        TypeAST* varType = getType( varTypeSr );

        type.Names.emplace_back( name );
        type.Types.emplace_back( varType );
        type.Values.emplace_back( value );
    }

    Token::StreamRef value;
    value.StreamPointer = nullptr;
    value.Length = 0;

    TypeAST* varType = getType( type.Name );
    parentType.Types.emplace_back( varType );
    parentType.Names.emplace_back( name );
    parentType.Values.emplace_back( value );
}

void Parser::parseShaderBlock(  const TypeAST::eTypes blockType, TypeAST& parentType )
{
    Token token;
    lexer.nextToken( token );

    Token::StreamRef name;
    name.StreamPointer = nullptr;
    name.Length = 0;

    if ( token.type ==  Token::IDENTIFIER ) {
         name = token.streamReference;

         if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
             return;
         }
    } else if ( token.type != Token::OPEN_BRACE ) {
        return;
    }

    TypeAST& _type = types[typesCount++];
    _type.Name = name;
    _type.Type = blockType;
    _type.Exportable = true;

    // Shared is a litteral block (contains shader shared code between the stages of the current library)
    // Since the shared code might have one or several permutation (depending on the properties of a pass)
    // we only apply the preprocessor and the compile flags when we build the renderpass itself
    lexer.nextToken( token );

    Token::StreamRef value;
    value.StreamPointer = token.streamReference.StreamPointer;
    value.Length = 0;

    i32 shaderBraceLvl = 1;
    while ( shaderBraceLvl > 0 ) {
        lexer.nextToken( token );

        if ( token.type == Token::CLOSE_BRACE ) {
            shaderBraceLvl--;
        } else if ( token.type == Token::OPEN_BRACE ) {
            shaderBraceLvl++;
        }
    }

    // NOTE Don't take the closing brace in account (hence the minus one)
    value.Length = static_cast<u32>(  token.streamReference.StreamPointer - value.StreamPointer ) - 1u;

    _type.Values.push_back( value );

    parentType.Names.push_back( value );
    parentType.Types.push_back( &_type );
}

void Parser::parseLibrary()
{
    Token token;
    if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
        return;
    }

    Token::StreamRef name = token.streamReference;

    if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
        return;
    }

    TypeAST& type = types[typesCount++];
    type.Name = name;
    type.Type = TypeAST::LIBRARY;
    type.Exportable = true;

    while ( !lexer.equalToken( Token::CLOSE_BRACE, token ) ) {
        if ( token.type == Token::IDENTIFIER ) {           
            if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 6, "shader" ) ) {               
                parseShaderBlock( TypeAST::SHADER, type );
            } else  if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 6, "shared" ) ) {
                parseShaderBlock( TypeAST::SHARED_CONTENT, type );
            } else if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 4, "pass" ) ) {
                parseRenderPass( type );
            } else if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 10, "properties" ) ) {
                Token::StreamRef name = token.streamReference;
                if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
                    continue;
                }

                TypeAST& _type = types[typesCount++];
                _type.Name = name;
                _type.Type = TypeAST::PROPERTIES;
                _type.Exportable = true;

                while ( !lexer.equalToken( Token::CLOSE_BRACE, token ) ) {
                    if ( token.type == Token::IDENTIFIER ) {
                        parseVariable( token.streamReference, _type );
                    }
                }

                type.Names.push_back( name );
                type.Types.push_back( &_type );
            } else if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 9, "resources" ) ) {
                parseResources( type );
            }
        }
    }
}

void Parser::parseIdentifier( const Token& token )
{
    for ( u32 i = 0; i < token.streamReference.Length; ++i ) {
        const char c = *( token.streamReference.StreamPointer + i );

        switch ( c ) {
            case 's':  {
                if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 6, "struct" ) ) {
                    parseStruct();
                    return;
                }
                break;
            }

            case 'e': {
                if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 4, "enum" ) ) {
                    parseEnum();
                    return;
                }

                break;
            }
                    
            case 'f': {
                if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 4, "font" ) ) {
                    parseFont();
                    return;
                }

                break;
            }

            case 'l': {
                if ( dk::core::ExpectKeyword( token.streamReference.StreamPointer, 3, "lib" ) ) {
                    parseLibrary();
                    return;
                }

                break;
            }
        }
    }
}

void Parser::parseStruct()
{
    Token token;
    if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
        return;
    }

    Token::StreamRef name = token.streamReference;

    if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
        return;
    }

    TypeAST& type = types[typesCount++];
    type.Name = name;
    type.Type = TypeAST::STRUCT;
    type.Exportable = true;

    while ( !lexer.equalToken( Token::CLOSE_BRACE, token ) ) {
        if ( token.type == Token::IDENTIFIER ) {
            parseVariable( token.streamReference, type );
        }
    }
}

void Parser::parseEnum()
{
    Token token;
    if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
        return;
    }

    Token::StreamRef name = token.streamReference;

    if ( !lexer.expectToken( Token::OPEN_BRACE, token ) ) {
        return;
    }

    TypeAST& type = types[typesCount++];
    type.Name = name;
    type.Type = TypeAST::ENUM;
    type.Exportable = true;

    while ( !lexer.equalToken( Token::CLOSE_BRACE, token ) ) {
        if ( token.type == Token::IDENTIFIER ) {
            type.Names.emplace_back( token.streamReference );
        }
    }
}

void Parser::parseVariable( const Token::StreamRef& typeName, TypeAST& type )
{
    TypeAST* varType = getType( typeName );

    Token token;
    if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
        return;
    }

    Token::StreamRef name = token.streamReference;

    Token::StreamRef value;
    value.StreamPointer = nullptr;
    value.Length = 0;

    lexer.nextToken( token );

    // Variable has an initializer value
    if ( token.type == Token::EQUALS ) {
        lexer.nextToken( token );
        value = token.streamReference;
    }

    while ( token.type != Token::SEMICOLON ) {
        lexer.nextToken( token );
    }

    value.Length = static_cast<u32>(  token.streamReference.StreamPointer - value.StreamPointer );

    type.Types.emplace_back( varType );
    type.Names.emplace_back( name );
    type.Values.emplace_back( value );
}

void Parser::parseFont()
{
    // TODO
}

void Parser::parseResources( TypeAST& parentType )
{
    Token token;
    if ( !lexer.equalToken( Token::OPEN_BRACE, token ) ) {
        return;
    }

    TypeAST& type = types[typesCount++];
    type.Name.StreamPointer = nullptr;
    type.Name.Length = 0;
    type.Type = TypeAST::RESOURCES;
    type.Exportable = true;

    i32 shaderBraceLvl = 1;
    while ( shaderBraceLvl > 0 ) {
        lexer.nextToken( token );

        if ( token.type == Token::CLOSE_BRACE ) {
            shaderBraceLvl--;
        } else if ( token.type == Token::OPEN_BRACE ) {
            shaderBraceLvl++;
        } else if ( token.type == Token::IDENTIFIER ) {
            TypeAST* varType = getType( token.streamReference );

            Token token;
            if ( !lexer.expectToken( Token::IDENTIFIER, token ) ) {
                continue;
            }

            Token::StreamRef name = token.streamReference;

            TypeAST& _type = types[typesCount++];
            _type.Name = name;
            _type.Type = TypeAST::RESOURCE_ENTRY;
            _type.Exportable = true;
            _type.PrimitiveType = ( varType == nullptr ) ? TypeAST::ePrimitiveType::PRIMITIVE_TYPE_NONE : varType->PrimitiveType;

            Token param;
            while ( !lexer.equalToken( Token::CLOSE_BRACE, param ) ) {
                if ( param.type != Token::IDENTIFIER ) {
                    continue;
                }

                Token::StreamRef name = param.streamReference;

                Token::StreamRef value;
                value.StreamPointer = nullptr;
                value.Length = 0;

                lexer.nextToken( param );
                // Take in account type for property override (e.g. float myVar = 1;)
                if ( param.type == Token::IDENTIFIER ) {
                    name = param.streamReference;
                    lexer.nextToken( param );
                }

                // Variable has an initializer value
                if ( param.type == Token::EQUALS ) {
                    lexer.nextToken( param );

                    value.StreamPointer = param.streamReference.StreamPointer;

                    while ( !lexer.equalToken( Token::SEMICOLON, param ) );

                    value.Length = static_cast< u32 >( param.streamReference.StreamPointer - value.StreamPointer );
                }

                _type.Names.emplace_back( name );
                 _type.Values.emplace_back( value );
            }

            type.Names.emplace_back( name );
            type.Types.emplace_back( & _type );
        }
    }

    parentType.Names.push_back( type.Name );
    parentType.Types.push_back( &type );
}

TypeAST* Parser::getType( const Token::StreamRef& typeName )
{
    for ( u32 i = 0; i < typesCount; i++ ) {
        const TypeAST& type = types[i];

        if ( type.Name.Length != typeName.Length ) {
            continue;
        }

        if ( strncmp( type.Name.StreamPointer, typeName.StreamPointer, typeName.Length ) == 0 ) {
            return &types[i];
        }
    }

    return nullptr;
}
