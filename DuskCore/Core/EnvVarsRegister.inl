/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/

template<typename T> 
T& EnvironmentVariables::registerVariable( const std::string& name, const dkStringHash_t typeHashcode, T* value, const T defaultValue, const bool serialize )
{
    Variable var = { name, value, typeHashcode, serialize };

    getEnvironmentVariableMap().emplace( DUSK_STRING_HASH( name.c_str() ), var );
    *value = defaultValue;

    return *value;
}

template<typename T>
T* EnvironmentVariables::getVariable( dkStringHash_t hashcode )
{
    auto iterator = getEnvironmentVariableMap().find( hashcode );

    return iterator != getEnvironmentVariableMap().end() ? ( T* )getEnvironmentVariableMap()[hashcode].Value : nullptr;
}
