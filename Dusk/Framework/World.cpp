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
#include "PointLight.h"
#include "Vehicle.h"

#include "Graphics/DrawCommandBuilder.h"
#include "Graphics/LightGrid.h"

static constexpr size_t MAX_ENTITY_COUNT = 10000;

World::World( BaseAllocator* allocator )
    : memoryAllocator( allocator )
    , entityDatabase( dk::core::allocate<EntityDatabase>( allocator ) )
    , entityNameRegister( dk::core::allocate<EntityNameRegister>( allocator, allocator ) )
    , transformDatabase( dk::core::allocate<TransformDatabase>( allocator, allocator ) )
    , staticGeometryDatabase( dk::core::allocate<StaticGeometryDatabase>( allocator, allocator ) )
    , pointLightDatabase( dk::core::allocate<PointLightDatabase>( allocator, allocator ) )
    , vehicleDatabase( dk::core::allocate<VehicleDatabase>( allocator, allocator ) )
{

}

World::~World()
{
	dk::core::free( memoryAllocator, entityDatabase );
	dk::core::free( memoryAllocator, entityNameRegister );
	dk::core::free( memoryAllocator, transformDatabase );
    dk::core::free( memoryAllocator, staticGeometryDatabase );
    dk::core::free( memoryAllocator, pointLightDatabase );
    dk::core::free( memoryAllocator, vehicleDatabase );
}

void World::create()
{
    entityNameRegister->create( MAX_ENTITY_COUNT );
    transformDatabase->create( MAX_ENTITY_COUNT );
    staticGeometryDatabase->create( MAX_ENTITY_COUNT );
    pointLightDatabase->create( MAX_ENTITY_COUNT );
    vehicleDatabase->create( MAX_ENTITY_COUNT );
}

void World::collectRenderables( DrawCommandBuilder* drawCmdBuilder, LightGrid* lightGrid ) const
{
    DUSK_CPU_PROFILE_FUNCTION;

    // Collect static geometry.
	for ( const Entity& geom : staticGeometry ) {
		const Model* model = staticGeometryDatabase->getModel( staticGeometryDatabase->lookup( geom ) );
        const dkMat4x4f& modelMatrix = transformDatabase->getWorldMatrix( transformDatabase->lookup( geom ) );

#ifdef DUSKED
        // This is a editor only case; the client should always have a model binded.
        if ( model == nullptr ) {
            continue;
        }
#endif

        drawCmdBuilder->addStaticModelInstance( model, modelMatrix, geom.getIdentifier() );
    }

    // Update and collect relevant point lights (we don't care about visibility relevance; culling is done on the GPU only).
    for ( const Entity& pointLight : pointLights ) {
        PointLightGPU& pointLightInfos = pointLightDatabase->getLightData( pointLightDatabase->lookup( pointLight ) );

        // Forward transform infos to the POD structure (we want to duplicate the position info since the structure is 
        // uploaded as is on the GPU plus we can easily apply dynamic updates or animations on the entity).
        const dkVec3f& worldPosition = transformDatabase->getWorldPosition( transformDatabase->lookup( pointLight ) );
        pointLightInfos.WorldPosition = worldPosition;

        lightGrid->addPointLightData( std::forward<PointLightGPU>( pointLightInfos ) );
    }
}

void World::update( const f32 deltaTime )
{
    updateStreaming();

    // Note Update order is IMPORTANT!
    // e.g. Vehicles must be updated prior to Transform since each vehicle instance will update its wheels transform
    vehicleDatabase->update( deltaTime, transformDatabase );
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

Entity World::createPointLight( const char* name )
{
    Entity entity = entityDatabase->allocateEntity();
    assignEntityName( entity, name );

    attachTransformComponent( entity );
    attachPointLightComponent( entity );

    pointLights.push_back( entity );

    return entity;
}

void World::releaseEntity( Entity& entity )
{
    transformDatabase->removeComponent( entity );
    staticGeometryDatabase->removeComponent( entity );
    
    staticGeometry.remove_if( [entity]( Entity& sge ) { return sge == entity; } );

    entityDatabase->releaseEntity( entity );
    entityNameRegister->releaseEntityName( entity );
}

void World::attachTransformComponent( Entity& entity )
{
    transformDatabase->allocateComponent( entity );
}

void World::attachStaticGeometryComponent( Entity& entity )
{
    staticGeometryDatabase->allocateComponent( entity );
}

void World::attachPointLightComponent( Entity& entity )
{
    pointLightDatabase->allocateComponent( entity );
}

TransformDatabase* World::getTransformDatabase() const
{
    return transformDatabase;
}

StaticGeometryDatabase* World::getStaticGeometryDatabase() const
{
    return staticGeometryDatabase;
}

PointLightDatabase* World::getPointLightDatabase() const
{
    return pointLightDatabase;
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
