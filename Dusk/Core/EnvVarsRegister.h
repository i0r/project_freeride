/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <map>
#include <functional>

#include <Dusk/FileSystem/FileSystemObject.h>

using dkToStringEnvType_t = std::function<const char*( void* )>;
using dkFromStringEnvType_t = std::function<void*( const dkString_t& )>;

struct Variable
{
    std::string      Name;
    void*            Value;
    dkStringHash_t  Type;
    bool             ShouldBeSerialized;
};

struct CustomVariableType
{
    dkToStringEnvType_t     ToString;
    dkFromStringEnvType_t   FromString;
};

class EnvironmentVariables
{
public:
    using Hashmap = std::map<dkStringHash_t, Variable>;

public:
    template<typename T> static T&                  registerVariable( const std::string& name, const dkStringHash_t typeHashcode, T* value, const T defaultValue = static_cast<T>( 0 ), const bool serialize = true );
    static i32                                      registerCustomType( const dkStringHash_t typeHashcode, dkToStringEnvType_t toStringFunc, dkFromStringEnvType_t fromStringFunc );
    template<typename T> static T*                  getVariable( dkStringHash_t hashcode );
    static void                                     serialize( FileSystemObject* file );
    static void                                     deserialize( FileSystemObject* file );
    static void                                     readValue( Variable& variable, const dkString_t& inputValue );
    static EnvironmentVariables::Hashmap&           getEnvironmentVariableMap();
};

#include "EnvVarsRegister.inl"

#define DUSK_ENV_OPTION_ENUM( optionName ) optionName,
#define DUSK_ENV_OPTION_STRING( optionName ) static_cast<const char* const>( #optionName ),
#define DUSK_ENV_OPTION_CASE( optionName ) case DUSK_STRING_HASH( #optionName ): return reinterpret_cast<void*>( optionName );

// Generates and registers a custom option type to the EnvironmentVariables instance
// The registered type will be serializable/deserializable
// Option list must be a macro defining the different options available
// Options enum is named: eACustomOptionList
// Options string is name: sACustomOptionList
//
// e.g. 
// #define OPTION_LIST( option ) option( OPTION_1 ) option( OPTION_2 )
// DUSK_ENV_OPTION_LIST( ACustomOptionList, OPTION_LIST )
// (...)
// DUSK_ENV_VAR( d_TestVar, "TestVariable", OPTION_1, eACustomOptionList )
#define DUSK_ENV_OPTION_LIST( name, list )\
enum e##name : int32_t\
{\
    list( DUSK_ENV_OPTION_ENUM )\
    e##name##_COUNT\
};\
static constexpr const char* s##name[e##name##_COUNT] =\
{\
    list( DUSK_ENV_OPTION_STRING )\
};\
static i32 __DUMMY##name = EnvironmentVariables::registerCustomType(\
DUSK_STRING_HASH( "e"#name ),\
[]( void* typelessVar )\
{\
    return s##name[*static_cast<e##name*>( typelessVar )]; \
},\
[]( const dkString_t& str )\
{\
    const dkStringHash_t hashcode = dk::core::CRC32( str );\
    switch ( hashcode ) {\
        list( DUSK_ENV_OPTION_CASE )\
        default: return static_cast<void*>( nullptr );\
    };\
} );

#define DUSK_ENV_VAR( name, defaultValue, type ) static type name = EnvironmentVariables::registerVariable<type>( #name, DUSK_STRING_HASH( #type ), &name, defaultValue );

#if DUSK_DEVBUILD
#define DUSK_DEV_VAR_PERSISTENT( name, defaultValue, type ) static type name = EnvironmentVariables::registerVariable<type>( #name, DUSK_STRING_HASH( #type ), &name, defaultValue );
#define DUSK_DEV_VAR( name, desc, defaultValue, type ) static type name = EnvironmentVariables::registerVariable<type>( #name, DUSK_STRING_HASH( #type ), &name, defaultValue, false );
#else
#define DUSK_DEV_VAR_PERSISTENT( name, desc, defaultValue, type ) static type name = defaultValue;
#define DUSK_DEV_VAR( name, desc, defaultValue, type ) static type name = defaultValue;
#endif
