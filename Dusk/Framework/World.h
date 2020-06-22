/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

class BaseAllocator;
class DrawCommandBuilder2;
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
    TransformDatabase*      transformDatabase;

    StaticGeometryDatabase* staticGeometryDatabase;

public:
                            World( BaseAllocator* allocator );
                            ~World();

    void                    create();

    // Iterate over the streamed entities in the World and collect any
    // entity that is renderable (e.g. static geometry; lights; etc.).
    void                    collectRenderables( DrawCommandBuilder2* drawCmdBuilder ) const;

    void                    update( const f32 deltaTime );

    Entity                  createStaticMesh( const dkChar_t* name = DUSK_STRING( "Static Mesh" ) );

    void                    attachTransformComponent( Entity& entity );

    void                    attachStaticGeometryComponent( Entity& entity );

private:
    BaseAllocator*          memoryAllocator;

    EntityDatabase*         entityDatabase;

    EntityNameRegister*     entityNameRegister;

    // TODO Might need allocation/logic improvement.
    std::list<Entity>       staticGeometry;

private:
	// Update this World area streaming.
	void                    updateStreaming();

    void                    assignEntityName( Entity& entity, const dkChar_t* assignedName = DUSK_STRING( "Entity" ) );
};
