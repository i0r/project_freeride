/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "Material.h"

Material::Material( BaseAllocator* allocator, const dkChar_t* materialName )
    : name( materialName )
{

}

Material::~Material()
{

}

void Material::deserialize( FileSystemObject* object )
{

}

void Material::bindForScenario( const RenderScenario scenario )
{

}

const dkString_t& Material::getName() const
{
    return name;
}

bool Material::isParameterMutable( const dkStringHash_t parameterHashcode ) const
{
    return ( mutableParameters.find( parameterHashcode ) != mutableParameters.end() );
}
