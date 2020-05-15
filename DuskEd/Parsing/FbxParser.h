/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Parsing/GeometryParser.h>

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

    // Return a pointer to the last model parsed. Return null if no model has been parsed/the parsing failed.
    ParsedModel*            getParsedModel() const;

public:
    // FBX SDK Manager.
    FbxManager*             fbxManager;
    
    // FBX SDK Scene (a virtual scene in which you are supposed to import fbx resources).
    FbxScene*               fbxScene;

    // FBX SDK Importer.
    FbxImporter*            importer;

    // The memory allocator used for triangle allocation.
    BaseAllocator*          memoryAllocator;

    // The model parsed/being parsed.
    ParsedModel*            parsedModel;
};
#endif
