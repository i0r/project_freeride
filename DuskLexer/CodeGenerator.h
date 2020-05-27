/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

struct TypeAST;

#include <vector>
#include <Rendering/RenderDevice.h>

struct GeneratedShader 
{
    std::string     ShaderName;
    std::string     GeneratedSource;
    eShaderStage    ShaderStage;
};

namespace dk
{
    namespace baker
    {
        void WriteEnum( const TypeAST& type );
        void WriteStruct( const TypeAST& type, const bool useReflection = true );
        void WriteRenderLibrary( const TypeAST& type, std::string& libraryName, std::string& generatedHeader, std::string& reflectionHeader, std::vector<GeneratedShader>& generatedShaders );
    }
}
