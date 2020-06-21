/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Parsing/GeometryParser.h>

class BaseAllocator;
class RenderDevice;
class Model;

namespace dk
{
    namespace graphics
    {
        // Create a Model instance based on a ParsedModel (from a .fbx, .obj, etc.). This should be used in an editor 
        // context only. Note that this function does not allocate the builtModel instance (it should be allocated
        // by an external source; e.g. the RenderWorld).
        void BuildParsedModel( Model* builtModel, BaseAllocator* memoryAllocator, RenderDevice* renderDevice, ParsedModel& parsedModel );
    }
}