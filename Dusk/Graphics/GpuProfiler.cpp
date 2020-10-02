/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "GpuProfiler.h"

GpuProfiler::GpuProfiler()
	: timestampQueryPool( nullptr )
	, sectionCount( 0u )
	, sectionReadIndex( 0u )
	, sectionWriteIndex( 0u )
	, queryCount( 0u )
{
	memset( sections, 0, sizeof( SectionInfos ) * TOTAL_SECTION_COUNT );
	memset( timestampQueries, 0, sizeof( u32 ) * TOTAL_SECTION_COUNT );
	memset( timestampResults, 0, sizeof( u64 ) * TOTAL_SECTION_COUNT );
	memset( timestampResultsAsMs, 0, sizeof( f64 ) * TOTAL_SECTION_COUNT );
}

GpuProfiler::~GpuProfiler()
{

}

void GpuProfiler::create( RenderDevice& renderDevice )
{
	timestampQueryPool = renderDevice.createQueryPool( eQueryType::QUERY_TYPE_TIMESTAMP, TOTAL_SECTION_COUNT );
}

void GpuProfiler::destroy( RenderDevice& renderDevice )
{
	if ( timestampQueryPool != nullptr ) {
		renderDevice.destroyQueryPool( timestampQueryPool );
		timestampQueryPool = nullptr;
	}
}

void GpuProfiler::update( RenderDevice& renderDevice )
{
#if 0
	const size_t frameIndex = renderDevice.getFrameIndex();

	if ( ( frameIndex - RESULT_RETRIVAL_FRAME_LAG ) >= 0 ) {
		// Note: We explicitly request a cmdlist for query readback since 
		// Vulkan query readback must be done on a Graphics or Compute queue.
		CommandList& readBackCmdList = renderDevice.allocateComputeCommandList();
		readBackCmdList.begin();
		getSectionsResult( renderDevice, readBackCmdList );
		readBackCmdList.end();
		renderDevice.submitCommandList( readBackCmdList );

		sectionReadIndex = ( sectionReadIndex + MAX_PROFILE_SECTION_COUNT ) % TOTAL_SECTION_COUNT;
	}

	sectionWriteIndex = ( sectionWriteIndex + MAX_PROFILE_SECTION_COUNT ) % TOTAL_SECTION_COUNT;
	sectionCount = 0;
#endif
}

GpuProfiler::SectionHandle_t GpuProfiler::beginSection( CommandList& cmdList )
{
    const SectionHandle_t handle = ( sectionWriteIndex + sectionCount );
#if 0
	SectionInfos& section = sections[handle];
	section.BeginQueryHandle = allocateQueryHandle( cmdList );
	section.EndQueryHandle = allocateQueryHandle( cmdList );
	section.Result = -1.0;

	sectionCount++;

	// Write start timestamp.
	cmdList.writeTimestamp( *timestampQueryPool, timestampQueries[section.BeginQueryHandle] );

	std::stack<SectionHandle_t>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];
	sectionsStack.push( handle );
#endif

	return handle;
}

void GpuProfiler::endSection( CommandList& cmdList )
{
#if 0
	std::stack<SectionHandle_t>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];

	DUSK_ASSERT( !sectionsStack.empty(), "There is no active profiling section..." );
	if ( !sectionsStack.empty() ) {
		return;
	}

	SectionHandle_t latestSectionIdx = sectionsStack.top();
	const SectionInfos& section = sections[latestSectionIdx];

	// Write end timestamp.
	cmdList.writeTimestamp( *timestampQueryPool, timestampQueries[section.EndQueryHandle] );

    sectionsStack.pop();
#endif
}

f64 GpuProfiler::getSectionResultInMs( const SectionHandle_t handle ) const
{
	const SectionHandle_t readHandle = ( sectionReadIndex + sectionCount );
	return sections[readHandle].Result;
}

void GpuProfiler::getSectionsResult( RenderDevice& renderDevice, CommandList& cmdList )
{
#if 0
	cmdList.retrieveQueryResults( *timestampQueryPool, sectionReadIndex, sectionCount );
	cmdList.getQueryResult( *timestampQueryPool, timestampResults, sectionReadIndex, sectionCount );

	for ( std::size_t sectionIdx = 0; sectionIdx < sectionCount; sectionIdx++ ) {
		const std::size_t sectionArrayIdx = ( sectionReadIndex + sectionIdx );
		SectionInfos& section = sections[sectionArrayIdx];

		const uint64_t beginQueryResult = timestampResults[section.BeginQueryHandle];
		const uint64_t endQueryResult = timestampResults[section.EndQueryHandle];

		// Check if the current handle is valid and available.
		// If not, stop the retrieval here.
		if ( beginQueryResult <= 0ull || endQueryResult <= 0ull ) {
			return;
		}

		u64 elapsedTicks = ( endQueryResult - beginQueryResult );
		section.Result = renderDevice.convertTimestampToMs( elapsedTicks );
    }
#endif
}

u32 GpuProfiler::allocateQueryHandle( CommandList& cmdList )
{
    const u32 handle = queryCount;
#if 0
	timestampQueries[handle] = cmdList.allocateQuery( *timestampQueryPool );
    queryCount = ( ++queryCount % TOTAL_SECTION_COUNT );
#endif
	return handle;
}

GpuProfiler g_GpuProfiler;
