/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_STUB
QueryPool* RenderDevice::createQueryPool( const eQueryType type, const u32 poolCapacity )
{
    DUSK_UNUSED_VARIABLE( type );
    DUSK_UNUSED_VARIABLE( poolCapacity );

    return nullptr;
}

void RenderDevice::destroyQueryPool( QueryPool* queryPool )
{
    DUSK_UNUSED_VARIABLE( queryPool );
}

f64 RenderDevice::convertTimestampToMs( const u64 timestamp ) const
{
    return static_cast<f64>( timestamp );
}

void CommandList::retrieveQueryResults( QueryPool& queryPool, const u32 startQueryIndex, const u32 queryCount )
{

}

void CommandList::getQueryResult( QueryPool& queryPool, u64* resultsArray, const u32 startQueryIndex, const u32 queryCount )
{

}

u32 CommandList::allocateQuery( QueryPool& queryPool )
{
    return 0;
}

void CommandList::beginQuery( QueryPool& queryPool, const u32 queryIndex )
{

}

void CommandList::endQuery( QueryPool& queryPool, const u32 queryIndex )
{

}

void CommandList::writeTimestamp( QueryPool& queryPool, const u32 queryIndex )
{

}
#endif
