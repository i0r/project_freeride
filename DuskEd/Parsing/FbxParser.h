/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include "GeometryParser.h"

#if DUSK_USE_FBXSDK
#include <fbxsdk.h>
#include <vector>

class BaseAllocator;

class FbxParser 
{
public:
                            FbxParser();
                            FbxParser( FbxParser& ) = delete;
                            FbxParser& operator = ( FbxParser& ) = delete;
                            ~FbxParser();

    // Create the instance of the parser (initialize FBX SDK internal states).
    void                    create( BaseAllocator* allocator );

    // Load a resource located at the given path (can be outside the virtual file system).
    // This function should only be used in an editor context.
    void                    load( const char* filePath );

    // Return the number of mesh parsed.
    i32                     getMeshCount() const;
    
    // Return the mesh at the specified index (meshIndex must be between 0 and getMeshCount() - 1).
    const ParsedMesh*       getMesh( const i32 meshIndex ) const;

public:
    // FBX SDK Manager.
    FbxManager*             fbxManager;
    
    // FBX SDK Scene (a virtual scene in which you are supposed to import fbx resources).
    FbxScene*               fbxScene;

    // FBX SDK Importer.
    FbxImporter*            importer;

    // The memory allocator used for triangle allocation.
    BaseAllocator*          memoryAllocator;

    // Parsed mesh array.
    // TODO In the future, it could use a linear allocator which would be flushed everytime the load function is called.
    // Right now; a crappy vector relying on the standard lib is not that great...
    std::vector<ParsedMesh> parsedMeshes;
};
#endif
