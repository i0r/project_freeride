/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "Lexer.h"

#include <Core/StringHelpers.h>

Lexer::Lexer( const char* text )
    : streamPosition( text )
    , streamLine( 0 )
    , streamColumn( 0 )
    , hasError( false )
    , errorLine( 0 )
{

}

Lexer::~Lexer()
{

}

void Lexer::nextToken( Token& token )
{
    skipWhitespace();

    token.type = Token::UNKNOWN;
    token.streamReference.StreamPointer = streamPosition;
    token.streamReference.Length = 1;
    token.streamLine =  streamLine;

    const char c  = *streamPosition;
    streamPosition++;

    switch ( c ) {
    case '\0':
        token.type = Token::END_OF_STREAM;
        break;
    case '(':
        token.type = Token::OPEN_PAREN;
        break;
    case ')':
        token.type = Token::CLOSE_PAREN;
        break;
    case ':':
        token.type = Token::COLON;
        break;
    case '.':
        token.type = Token::DOT;
        break;
    case ';':
        token.type = Token::SEMICOLON;
        break;
    case ',':
        token.type = Token::COMMA;
        break;
    case '=':
        token.type = Token::EQUALS;
        break;
    case '*':
        token.type = Token::ASTERISK;
        break;
    case '<':
        token.type = Token::OPEN_BRACKET;
        break;
    case '>':
        token.type = Token::CLOSE_BRACKET;
        break;
    case '[':
        token.type = Token::OPEN_ANGLE_BRACKET;
        break;
    case ']':
        token.type = Token::CLOSE_ANGLE_BRACKET;
        break;
    case '{':
        token.type = Token::OPEN_BRACE;
        break;
    case '}':
        token.type = Token::CLOSE_BRACE;
        break;
    case '$':
        token.type = Token::DOLLAR;
        break;
    case '+':
        token.type = Token::PLUS;
        break;
    case '-':
        token.type = Token::MINUS;
        break;
    case '/':
        token.type = Token::SLASH;
        break;
    case '#':
        token.type = Token::SHARP;
        break;
    case '"': {
        token.type = Token::STRING;
        token.streamReference.StreamPointer = streamPosition;

        while ( *streamPosition && *streamPosition != '"' ) {
            if ( *streamPosition == '\\' && streamPosition[1] ) {
                ++streamPosition;
            }

            ++streamPosition;
        }

        token.streamReference.Length = static_cast<u32>( streamPosition - token.streamReference.StreamPointer );

        if ( *streamPosition == '"' ) {
            ++streamPosition;
        }
    } break;
    default: {
        if ( dk::core::IsAlpha( c ) ) {
            token.type = Token::IDENTIFIER;

            while ( dk::core::IsAlpha( *streamPosition )
                    || dk::core::IsNumber( *streamPosition )
                    || *streamPosition == '_' ) {
                ++streamPosition;
            }

            token.streamReference.Length = static_cast<u32>( streamPosition - token.streamReference.StreamPointer );
        } else if ( dk::core::IsNumber( c ) ) {
            token.type = Token::NUMBER;
        } else {
            token.type = Token::UNKNOWN;
        }
    } break;
    }
}

bool Lexer::expectToken( const Token::Type expectedType, Token& token )
{
    if ( hasError ) {
        return true;
    }

    nextToken( token );

    const bool error = ( token.type != expectedType );
    hasError = error;

    if ( error ) {
        errorLine = streamLine;
    }

    return !error;
}

bool Lexer::equalToken( const Token::Type expectedType, Token& token )
{
    nextToken( token );

    return ( token.type == expectedType );
}

void Lexer::skipWhitespace()
{
    for ( ; ; ) {
        if ( dk::core::IsWhitespace( *streamPosition ) ) {
            if ( dk::core::IsEndOfLine( *streamPosition ) ) {
                ++streamLine;
            }

            ++streamPosition;
        } else if ( streamPosition[0] == '/' && streamPosition[1] == '/' ) {
            streamPosition += 2;
            while ( *streamPosition && !dk::core::IsEndOfLine( *streamPosition ) ) {
                ++streamPosition;
            }
        } else if ( streamPosition[0] == '/' && streamPosition[1] == '*' ) {
            streamPosition += 2;

            while ( !( streamPosition[0] == '*' && streamPosition[1] == '/' ) ) {
                if ( dk::core::IsEndOfLine( *streamPosition ) ) {
                    ++streamLine;
                }

                ++streamPosition;
            }

            if ( *streamPosition == '*' ) {
                streamPosition += 2;
            }
        } else {
            break;
        }
    }
}
