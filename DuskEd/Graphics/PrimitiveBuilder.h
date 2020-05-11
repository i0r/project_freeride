/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Parsing/GeometryParser.h>

// This is for testing purpose; so don't be surprised about this retarded dependency
#include <Graphics/RenderModules/PrimitiveLightingTest.h>

class RenderDevice;
struct Buffer;

namespace dk
{
    namespace graphics
    {
        BuiltPrimitive    BuildPrimitive( RenderDevice* renderDevice, const ParsedMesh& parsedMesh );
    }
}