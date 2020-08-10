/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "BakerCache.h"

#include "FileSystem/FileSystemObject.h"

BakerCache::BakerCache()
{

}

BakerCache::~BakerCache()
{
	cacheData.clear();
}

void BakerCache::load( FileSystemObject* stream )
{
	u32 entryCount;
	stream->read<u32>( entryCount );

	for ( u32 i = 0; i < entryCount; i++ ) {
		dkStringHash_t hashcode;
		u32 contentHashcode;

		stream->read<dkStringHash_t>( hashcode );
		stream->read<u32>( contentHashcode );

		cacheData.insert( std::make_pair( hashcode, contentHashcode ) );
	}
}

void BakerCache::store( FileSystemObject* stream )
{
	stream->write<u32>( static_cast< u32 >( cacheData.size() ) );

	for ( auto& entry : cacheData ) {
		stream->write<dkStringHash_t>( entry.first );
		stream->write<u32>( entry.second );
	}
}

u32 BakerCache::getContentHashcode( const dkStringHash_t key ) const
{
	auto cacheIt = cacheData.find( key );
	return cacheIt != cacheData.end() ? cacheIt->second : 0u;
}

void BakerCache::updateOrCreateEntry( const dkStringHash_t key, const u32 value )
{
	cacheData[key] = value;
}

bool BakerCache::isEntryDirty( const dkStringHash_t key, const u32 newValue ) const
{
	u32 oldValue = getContentHashcode( key );
	return ( oldValue == 0u || oldValue == newValue );
}
