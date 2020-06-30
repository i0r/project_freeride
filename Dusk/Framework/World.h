/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class DrawCommandBuilder;
class EntityDatabase;
class EntityNameRegister;
class TransformDatabase;
class StaticGeometryDatabase;

#include <list>
#include "Entity.h"

class World
{
public:
	static constexpr u32    GRID_SIZE_X = 8;
	static constexpr u32    GRID_SIZE_Y = 2;
	static constexpr u32    GRID_SIZE_Z = 8;

public:
public:
                            World( BaseAllocator* allocator );
                            ~World();

    void                    create();

    // Iterate over the streamed entities in the World and collect any
    // entity that is renderable (e.g. static geometry; lights; etc.).
    void                    collectRenderables( DrawCommandBuilder* drawCmdBuilder ) const;

    void                    update( const f32 deltaTime );

    Entity                  createStaticMesh( const char* name = "Static Mesh" );

    void                    releaseEntity( Entity& entity );

    void                    attachTransformComponent( Entity& entity );

    void                    attachStaticGeometryComponent( Entity& entity );

    TransformDatabase*      getTransformDatabase() const;

    StaticGeometryDatabase* getStaticGeometryDatabase() const;

    EntityNameRegister*     getEntityNameRegister() const;

private:
    BaseAllocator*          memoryAllocator;

    EntityDatabase*         entityDatabase;

    // TODO Might need allocation/logic improvement.
    std::list<Entity>       staticGeometry;

    TransformDatabase*      transformDatabase;

    StaticGeometryDatabase* staticGeometryDatabase;

    EntityNameRegister*     entityNameRegister;

private:
	// Update this World area streaming.
	void                    updateStreaming();

    void                    assignEntityName( Entity& entity, const char* assignedName = "Entity" );
};
