/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "EnvVarsRegister.h"
#include <Io/TextStreamHelpers.h>

#include <Core/StringHelpers.h>
#include <Core/Hashing/CRC32.h>

#include <Maths/Vector.h>

// Not really nice, but this way we can control the initialization of global vars
static DUSK_INLINE std::map<dkStringHash_t, CustomVariableType>& ENV_VAR_TYPES()
{
    static std::map<dkStringHash_t, CustomVariableType> internalMap = {};
    return internalMap;
}

EnvironmentVariables::Hashmap& EnvironmentVariables::getEnvironmentVariableMap()
{
    static std::map<dkStringHash_t, Variable> internalMap = {};
    return internalMap;
}

void EnvironmentVariables::readValue( Variable& variable, const dkString_t& inputValue )
{
    if ( !variable.ShouldBeSerialized ) {
        return;
    }

    switch ( variable.Type ) {
    case DUSK_STRING_HASH( "double" ):
        *static_cast<double*>( variable.Value ) = std::stod( inputValue );
        break;
    case DUSK_STRING_HASH( "f32" ):
        *static_cast<f32*>( variable.Value ) = std::stof( inputValue );
        break;
    case DUSK_STRING_HASH( "long long" ):
    case DUSK_STRING_HASH( "int64_t" ):
        *static_cast<int64_t*>( variable.Value ) = std::stoll( inputValue );
        break;
    case DUSK_STRING_HASH( "unsigned long long" ):
    case DUSK_STRING_HASH( "uint64_t" ):
        *static_cast<uint64_t*>( variable.Value ) = std::stoull( inputValue );
        break;
    case DUSK_STRING_HASH( "i32" ):
    case DUSK_STRING_HASH( "int32_t" ):
        *static_cast<int32_t*>( variable.Value ) = std::stoi( inputValue );
        break;
    case DUSK_STRING_HASH( "u32" ):
    case DUSK_STRING_HASH( "uint32_t" ):
        *static_cast<u32*>( variable.Value ) = static_cast<u32>( std::stoi( inputValue ) );
        break;
        // TODO Not safe
    case DUSK_STRING_HASH( "short" ):
    case DUSK_STRING_HASH( "int16_t" ):
        *static_cast<int16_t*>( variable.Value ) = static_cast<int16_t>( std::stoi( inputValue ) );
        break;
    case DUSK_STRING_HASH( "unsigned short" ):
    case DUSK_STRING_HASH( "uint16_t" ):
        *static_cast<uint16_t*>( variable.Value ) = static_cast<uint16_t>( std::stoi( inputValue ) );
        break;
    case DUSK_STRING_HASH( "bool" ):
        *static_cast<bool*>( variable.Value ) = dk::io::StringToBoolean( inputValue );
        break;
    case DUSK_STRING_HASH( "dkVec2u" ):
        *static_cast<dkVec2u*>( variable.Value ) = dk::io::StringTo2DVectorU( inputValue );
        break;
    default: {
        auto it = ENV_VAR_TYPES().find( variable.Type );

        // This shouldn't cause any trouble as long as the generated enum type matches the type used here
        if ( it != ENV_VAR_TYPES().end() ) {
            *static_cast<int64_t*>( variable.Value ) = reinterpret_cast<int64_t>( it->second.FromString( inputValue ) );
        }
    } break;
    }
}

i32 EnvironmentVariables::registerCustomType( const dkStringHash_t typeHashcode, dkToStringEnvType_t toStringFunc, dkFromStringEnvType_t fromStringFunc )
{
    CustomVariableType varType = { toStringFunc, fromStringFunc };
    ENV_VAR_TYPES().emplace( typeHashcode, varType );

    // NOTE Dummy value so that the compiler doesn't think we're trying to redeclare the function outside the class within a macro call
    return 0;
}

void EnvironmentVariables::serialize( FileSystemObject* file )
{
    for ( auto& var : getEnvironmentVariableMap() ) {
        const Variable& varInfos = var.second;

        if ( !varInfos.ShouldBeSerialized ) {
            continue;
        }

        std::string varLine = varInfos.Name;
        varLine.append( ": " );

        switch ( varInfos.Type ) {
        case DUSK_STRING_HASH( "double" ):
            varLine.append( std::to_string( *( double* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "f32" ):
            varLine.append( std::to_string( *( f32* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "long long" ):
        case DUSK_STRING_HASH( "int64_t" ):
            varLine.append( std::to_string( *( int64_t* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "unsigned long long" ):
        case DUSK_STRING_HASH( "uint64_t" ):
            varLine.append( std::to_string( *( uint64_t* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "i32" ):
        case DUSK_STRING_HASH( "int32_t" ):
            varLine.append( std::to_string( *( int32_t* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "u32" ):
        case DUSK_STRING_HASH( "uint32_t" ):
            varLine.append( std::to_string( *( u32* )varInfos.Value ) );
            break;
            // TODO Not safe
        case DUSK_STRING_HASH( "short" ):
        case DUSK_STRING_HASH( "int16_t" ):
            varLine.append( std::to_string( *( int16_t* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "unsigned short" ):
        case DUSK_STRING_HASH( "uint16_t" ):
            varLine.append( std::to_string( *( uint16_t* )varInfos.Value ) );
            break;
        case DUSK_STRING_HASH( "bool" ):
            varLine.append( *( bool* )varInfos.Value ? "True" : "False" );
            break;
        case DUSK_STRING_HASH( "dkVec2u" ): {
            const dkVec2u& vec = *( dkVec2u* )varInfos.Value;
            varLine.append( "{" + std::to_string( vec.x ) + "," + std::to_string( vec.y ) + "}" );
        } break;
        default:
            auto it = ENV_VAR_TYPES().find( varInfos.Type );

            if ( it != ENV_VAR_TYPES().end() ) {
                varLine.append( it->second.ToString( varInfos.Value ) );
            } else {
                DUSK_LOG_WARN( "Found unknown EnvVar type! (with hashcode: 0x%x)\n", varInfos.Type );
                continue;
            }
            break;
        }

        varLine.append( "\n" );
        file->writeString( varLine );
    }
}

void EnvironmentVariables::deserialize( FileSystemObject* file )
{
    dkString_t streamLine, dictionaryKey, dictionaryValue;

    DUSK_LOG_INFO( "EnvVars deserialization started...\n" );

    while ( file->isGood() ) {
        dk::io::ReadString( file, streamLine );

        // Find seperator character offset in the line (if any)
        const size_t commentSeparator = streamLine.find_first_of( DUSK_STRING( "#" ) );

        // Remove user comments before reading the keypair value
        if ( commentSeparator != dkString_t::npos ) {
            streamLine.erase( streamLine.begin() + commentSeparator, streamLine.end() );
        }

        // Skip commented out and empty lines
        if ( streamLine.empty() ) {
            continue;
        }

        const size_t keyValueSeparator = streamLine.find_first_of( ':' );

        // Check if this is a key value line
        if ( keyValueSeparator != dkString_t::npos ) {
            dictionaryKey = streamLine.substr( 0, keyValueSeparator );
            dictionaryValue = streamLine.substr( keyValueSeparator + 1 );

            // Trim both key and values (useful if a file has inconsistent spacing, ...)
            dk::core::TrimString( dictionaryKey );
            dk::core::TrimString( dictionaryValue );

            // Do the check after triming, since the value might be a space or a tab character
            if ( !dictionaryValue.empty() ) {
                dkStringHash_t keyHashcode = dk::core::CRC32( dictionaryKey.c_str() );
                auto variablesIterator = getEnvironmentVariableMap().find( keyHashcode );

                if ( variablesIterator != getEnvironmentVariableMap().end() ) {
                    Variable& variableInstance = getEnvironmentVariableMap().at( keyHashcode );

                    readValue( variableInstance, dictionaryValue );
                } else {
                    DUSK_LOG_ERROR( "Unknown environment variable '%s' (either spelling mistake or deprecated)\n", dictionaryKey.c_str() );
                }
            }
        }
    }
}
