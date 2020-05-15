/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "PrimitiveExporter.h"

#include <Parsing/DuskGeometry.h>

void dk::editor::ExportToDuskPrimitive( FileSystemObject* fileSystemObject, const ParsedMesh& parsedMesh )
{
    fileSystemObject->write<u32>( DuskGeometryMagic );
    fileSystemObject->write<DuskGeometryVersion>( DuskGeometryVersion::LatestVersion );
}