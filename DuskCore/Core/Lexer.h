/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct Token {
    struct StreamRef {
        const char*   StreamPointer;
        u32      Length;

        StreamRef()
            : StreamPointer( nullptr )
            , Length( 0 )
        {

        }
    };

    enum Type {
        UNKNOWN,

        OPEN_PAREN,
        CLOSE_PAREN,
        COLON,
        SEMICOLON,
        COMMA,
        DOT,
        EQUALS,
        ASTERISK,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        OPEN_BRACE,
        CLOSE_BRACE,
        OPEN_ANGLE_BRACKET,
        CLOSE_ANGLE_BRACKET,
        DOLLAR,
        PLUS,
        MINUS,
        SLASH,
        SHARP,

        STRING,
        IDENTIFIER,
        NUMBER,

        END_OF_STREAM,
    };

    Type             type;
    StreamRef   streamReference;
    u32               streamLine;
};

class Lexer
{
public:
    Lexer( const char* text );
    Lexer( Lexer& ) = delete;
    Lexer& operator = ( Lexer& ) = delete;
    ~Lexer();

    void    nextToken( Token& token );
    bool    expectToken( const Token::Type expectedType, Token& token );
    bool    equalToken( const Token::Type expectedType, Token& token );

private:
    const char*         streamPosition;
    u32                 streamLine;
    u32                 streamColumn;

    bool                hasError;
    u32                 errorLine;

private:
    void                skipWhitespace();
};
