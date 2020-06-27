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

#include "Graphics/DrawCommandBuilder.h"

static constexpr size_t MAX_ENTITY_COUNT = 10000;

World::World( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , entityDatabase( dk::core::allocate<EntityDatabase>( allocator ) )
    , entityNameRegister( dk::core::allocate<EntityNameRegister>( allocator, allocator ) )
    , transformDatabase( dk::core::allocate<TransformDatabase>( allocator, allocator ) )
    , staticGeometryDatabase( dk::core::allocate<StaticGeometryDatabase>( allocator, allocator ) )
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

        // This is a editor only case; the client should always have a model binded.
        if ( model == nullptr ) {
            continue;
        }

        drawCmdBuilder->addStaticModelInstance( model, modelMatrix, geom.getIdentifier() );
    }
}

void World::update( const f32 deltaTime )
{
    updateStreaming();

    transformDatabase->update( deltaTime );
}

Entity World::createStaticMesh( const char* name )
{
    Entity entity = entityDatabase->allocateEntity();
    assignEntityName( entity, name );

    attachTransformComponent( entity );
    attachStaticGeometryComponent( entity );

    staticGeometry.push_back( entity );

    return entity;
}

void World::releaseEntity( Entity& entity )
{
	staticGeometry.remove( entity );
 
    entityDatabase->releaseEntity( entity );
}

void World::attachTransformComponent( Entity& entity )
{
    transformDatabase->allocateComponent( entity );
}

void World::attachStaticGeometryComponent( Entity& entity )
{
    staticGeometryDatabase->allocateComponent( entity );
}

TransformDatabase* World::getTransformDatabase() const
{
    return transformDatabase;
}

StaticGeometryDatabase* World::getStaticGeometryDatabase() const
{
    return staticGeometryDatabase;
}

EntityNameRegister* World::getEntityNameRegister() const
{
    return entityNameRegister;
}

void World::updateStreaming()
{

}

void World::assignEntityName( Entity& entity, const char* assignedName )
{
    dkStringHash_t entityHashcode = dk::core::CRC32( assignedName );

    // Lookup the register to find is the name is already taken.
    bool isNameTaken = entityNameRegister->exist( entityHashcode );

	i32 nameCopyCount = 0;
    std::string uniqueName = std::string( assignedName );

    // If the name is already taken, try to find a unique one.
    while ( isNameTaken ) {
		uniqueName = std::string( assignedName );
		uniqueName.append( " (" );
		uniqueName.append( std::to_string( nameCopyCount++ ) );
		uniqueName.append( ")" );

        entityHashcode = dk::core::CRC32( uniqueName );
		isNameTaken = entityNameRegister->exist( entityHashcode );
    }

    entityNameRegister->setName( entity, uniqueName.c_str() );
}
