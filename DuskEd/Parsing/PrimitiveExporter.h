/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <Parsing/GeometryParser.h>

class FileSystemObject;

namespace dk
{
    namespace editor
    {
        // Export a parsed mesh (e.g. an already parsed .fbx or .obj) to a Dusk Primitive Geometry (.dpg).
        static void ExportToDuskPrimitive( FileSystemObject* fileSystemObject, const ParsedMesh& parsedMesh );
    }
}
