/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include "Shared.h"
#include "World.h"

#include "EntityDatabase.h"
#include "EntityNameRegister.h"
#include "Transform.h"
#include "StaticGeometry.h"

#include <DuskRenderer/Graphics/DrawCommandBuilder.h>

static constexpr size_t MAX_ENTITY_COUNT = 10000;

World::World( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , entityDatabase( dk::core::allocate<EntityDatabase>( memoryAllocator ) )
    , entityNameRegister( dk::core::allocate<EntityNameRegister>( memoryAllocator, memoryAllocator ) )
    , transformDatabase( dk::core::allocate<TransformDatabase>( memoryAllocator, memoryAllocator ) )
    , staticGeometryDatabase( dk::core::allocate<StaticGeometryDatabase>( memoryAllocator, memoryAllocator ) )
{

}

World::~World()
{
	dk::core::free( memoryAllocator, entityDatabase );
	dk::core::free( memoryAllocator, entityNameRegister );
	dk::core::free( memoryAllocator, transformDatabase );
	dk::core::free( memoryAllocator, staticGeometryDatabase );
}

void World::create()
{
    entityNameRegister->create( MAX_ENTITY_COUNT );
    transformDatabase->create( MAX_ENTITY_COUNT );
    staticGeometryDatabase->create( MAX_ENTITY_COUNT );
}

void World::collectRenderables( DrawCommandBuilder2* drawCmdBuilder ) const
{
	for ( const Entity& geom : staticGeometry ) {
		const Model* model = staticGeometryDatabase->getModel( staticGeometryDatabase->lookup( geom ) );
        const dkMat4x4f& modelMatrix = transformDatabase->getWorldMatrix( transformDatabase->lookup( geom ) );

        drawCmdBuilder->addGeometryInstance( model, modelMatrix );
    }
}

void World::update( const f32 deltaTime )
{
    updateStreaming();

    transformDatabase->update( deltaTime );
}

Entity World::createStaticMesh( const dkChar_t* name )
{
    Entity entity = entityDatabase->allocateEntity();
    assignEntityName( entity, name );

    attachTransformComponent( entity );
    attachStaticGeometryComponent( entity );

    staticGeometry.push_back( entity );

    return entity;
}

void World::attachTransformComponent( Entity& entity )
{
    transformDatabase->allocateComponent( entity );
}

void World::attachStaticGeometryComponent( Entity& entity )
{
    staticGeometryDatabase->allocateComponent( entity );
}

void World::updateStreaming()
{

}

void World::assignEntityName( Entity& entity, const dkChar_t* assignedName )
{
    dkStringHash_t entityHashcode = dk::core::CRC32( assignedName );

    // Lookup the register to find is the name is already taken.
    bool isNameTaken = entityNameRegister->exist( entityHashcode );

	i32 nameCopyCount = 0;
    dkString_t uniqueName = dkString_t( assignedName );

    // If the name is already taken, try to find a unique one.
    while ( isNameTaken ) {
		uniqueName = dkString_t( assignedName );
		uniqueName.append( DUSK_STRING( " (" ) );
		uniqueName.append( DUSK_TO_STRING( nameCopyCount++ ) );
		uniqueName.append( DUSK_STRING( ")" ) );

        entityHashcode = dk::core::CRC32( uniqueName );
		isNameTaken = entityNameRegister->exist( entityHashcode );
    }

    entityNameRegister->setName( entity, uniqueName.c_str() );
}
