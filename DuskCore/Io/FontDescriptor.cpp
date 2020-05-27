/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "FontDescriptor.h"

#include <FileSystem/FileSystemObject.h>
#include "TextStreamHelpers.h"
#include "Core/StringHelpers.h"

void dk::io::LoadFontFile( FileSystemObject* file, FontDescriptor& data )
{
    dkString_t streamLine, lineToken;

    while ( file->isGood() ) {
        dk::io::ReadString( file, streamLine );

        const auto keyValueSeparator = streamLine.find_first_of( ' ' );

        // Check if this is a key value line
        if ( keyValueSeparator != dkString_t::npos ) {
            lineToken = streamLine.substr( 0, keyValueSeparator );

            // Trim both key and values (useful if a file has inconsistent spacing, ...)
            dk::core::TrimString( lineToken );

            auto keyHashcode = dk::core::CRC32( lineToken.c_str() );

            switch ( keyHashcode ) {
            case DUSK_STRING_HASH( "page" ):
            {
                auto commonVariablesLine = streamLine.substr( keyValueSeparator + 1 );

                auto commonVariableOffset = commonVariablesLine.find( ' ' );
                auto previousCommonVariableOffset = 0;
                while ( commonVariableOffset != dkString_t::npos ) {
                    auto variable = commonVariablesLine.substr( previousCommonVariableOffset, commonVariableOffset - previousCommonVariableOffset );

                    const auto keyValueSeparator = variable.find_first_of( '=' );
                    if ( keyValueSeparator == dkString_t::npos ) {
                        continue;
                    }

                    auto key = variable.substr( 0, keyValueSeparator );
                    auto value = variable.substr( keyValueSeparator + 1 );

                    auto tokenHashcode = dk::core::CRC32( key.c_str() );
                    switch ( tokenHashcode ) {
                    case DUSK_STRING_HASH( "file" ):
                        data.Name = WrappedStringToString( value );
                        break;
                    default:
                        break;
                    }

                    // TODO Shouldn't be needed
                    if ( previousCommonVariableOffset > commonVariableOffset ) {
                        break;
                    }

                    // Look for the next var
                    previousCommonVariableOffset = static_cast< int >( commonVariableOffset ) + 1;
                    commonVariableOffset = commonVariablesLine.find( ' ', commonVariableOffset );
                }
            } break;


            case DUSK_STRING_HASH( "common" ):
            {
                auto commonVariablesLine = streamLine.substr( keyValueSeparator + 1 );

                auto commonVariableOffset = commonVariablesLine.find( ' ', keyValueSeparator + 1 );
                auto previousCommonVariableOffset = 0;
                while ( commonVariableOffset != dkString_t::npos ) {
                    auto variable = commonVariablesLine.substr( previousCommonVariableOffset, commonVariableOffset - previousCommonVariableOffset );

                    const auto keyValueSeparator = variable.find_first_of( '=' );
                    if ( keyValueSeparator == dkString_t::npos ) {
                        continue;
                    }

                    auto key = variable.substr( 0, keyValueSeparator );
                    auto value = variable.substr( keyValueSeparator + 1 );

                    auto tokenHashcode = dk::core::CRC32( key.c_str() );
                    switch ( tokenHashcode ) {
                    case DUSK_STRING_HASH( "scaleW" ):
                        data.AtlasWidth = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "scaleH" ):
                        data.AtlasHeight = std::stoi( value );
                        break;
                    default:
                        break;
                    }

                    // Look for the next var
                    previousCommonVariableOffset = static_cast< int >( commonVariableOffset ) + 1;
                    commonVariableOffset = commonVariablesLine.find( ' ', previousCommonVariableOffset );
                }
            } break;

            case DUSK_STRING_HASH( "char" ):
            {
                auto commonVariablesLine = streamLine.substr( keyValueSeparator + 1 );

                FontDescriptor::Glyph* glyph = nullptr;

                auto commonVariableOffset = commonVariablesLine.find( ' ', keyValueSeparator + 1 );
                auto previousCommonVariableOffset = 0;
                while ( commonVariableOffset != dkString_t::npos ) {
                    auto variable = commonVariablesLine.substr( previousCommonVariableOffset, commonVariableOffset - previousCommonVariableOffset );

                    if ( variable.empty() ) {
                        previousCommonVariableOffset++;
                        commonVariableOffset = commonVariablesLine.find( ' ', previousCommonVariableOffset );
                        continue;
                    }

                    const auto keyValueSeparator = variable.find_first_of( '=' );
                    if ( keyValueSeparator == dkString_t::npos ) {
                        continue;
                    }

                    auto key = variable.substr( 0, keyValueSeparator );
                    auto value = variable.substr( keyValueSeparator + 1 );

                    auto tokenHashcode = dk::core::CRC32( key.c_str() );
                    switch ( tokenHashcode ) {
                    case DUSK_STRING_HASH( "id" ):
                    {
                        auto glyphIndex = std::stoi( value );
                        auto requiredAtlasCapacity = ( glyphIndex + 1 );

                        if ( requiredAtlasCapacity > data.Glyphes.size() ) {
                            data.Glyphes.resize( requiredAtlasCapacity );
                        }

                        glyph = &data.Glyphes[glyphIndex];
                    } break;
                    case DUSK_STRING_HASH( "x" ):
                        glyph->PositionX = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "y" ):
                        glyph->PositionY = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "width" ):
                        glyph->Width = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "height" ):
                        glyph->Height = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "xoffset" ):
                        glyph->OffsetX = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "yoffset" ):
                        glyph->OffsetY = std::stoi( value );
                        break;
                    case DUSK_STRING_HASH( "xadvance" ):
                        glyph->AdvanceX = std::stoi( value );
                        break;
                    default:
                        break;
                    }

                    // Look for the next var
                    previousCommonVariableOffset = static_cast< int >( commonVariableOffset ) + 1;
                    commonVariableOffset = commonVariablesLine.find( ' ', previousCommonVariableOffset );
                }
            } break;


            default:
                continue;
            }
        }
    }

    file->close();
}
