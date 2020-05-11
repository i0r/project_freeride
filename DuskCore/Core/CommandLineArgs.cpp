/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "CommandLineArgs.h"

#include <Core/Hashing/CRC32.h>
#include <Core/EnvVarsRegister.h>

#include <Core/Logger.h>
#include <Core/StringHelpers.h>

#include <string.h>

void dk::core::ReadCommandLineArgs( const char* cmdLineArgs )
{
    DUSK_LOG_INFO( "Parsing command line arguments ('%s')\n", StringToWideString( cmdLineArgs ).c_str() );

    char* arg = strtok( const_cast<char*>( cmdLineArgs ), " " );

	while ( arg != nullptr ) {
        dkStringHash_t argHash = CRC32( arg );
        EnvironmentVariables::Hashmap& envVarsMap = EnvironmentVariables::getEnvironmentVariableMap();
        auto variablesIterator = envVarsMap.find( argHash );

        if ( variablesIterator != envVarsMap.end() ) {
            Variable& variableInstance = envVarsMap.at( argHash );

            char* varState = strtok( nullptr, " " );
            if ( varState == nullptr ) {
                continue;
            }

            dkString_t varValue = dkString_t( varState, varState + std::string::traits_type::length( varState ) );
            EnvironmentVariables::readValue( variableInstance, varValue );
        } else {
            DUSK_LOG_ERROR( "Unknown environment variable '%s' (either spelling mistake or deprecated)\n", arg );
        }

		arg = strtok( nullptr, " " );
	}
}
