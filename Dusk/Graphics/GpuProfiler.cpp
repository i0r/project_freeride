/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "GpuProfiler.h"

GpuProfiler::GpuProfiler()
	: timestampQueryPool( nullptr )
	, lastUpdateFrameIndex( 0ull )
{

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
    const size_t frameIndex = renderDevice.getFrameIndex();
    const size_t retrievalFrameIndex = lastUpdateFrameIndex + RESULT_RETRIVAL_FRAME_LAG;

	if ( frameIndex >= retrievalFrameIndex ) {
		// Note: We explicitly request a cmdlist for query readback since 
		// Vulkan query readback must be done on a Graphics or Compute queue.
		CommandList& readBackCmdList = renderDevice.allocateComputeCommandList();
		readBackCmdList.begin();
		getSectionsResult( renderDevice, readBackCmdList );
		readBackCmdList.end();

		renderDevice.submitCommandList( readBackCmdList );

		lastUpdateFrameIndex = frameIndex;
	}
}

void GpuProfiler::beginSection( CommandList& cmdList, const std::string& sectionName )
{
 //   const SectionHandle_t handle = ( sectionWriteIndex + sectionCount );

	//SectionInfos& section = sections[handle];
	//section.BeginQueryHandle = allocateQueryHandle( cmdList );
	//section.EndQueryHandle = allocateQueryHandle( cmdList );
	//section.Result = -1.0;
	//section.Name = sectionName;

	//sectionCount++;

	//// Write start timestamp.
	//cmdList.writeTimestamp( *timestampQueryPool, timestampQueries[section.BeginQueryHandle] );

	//std::stack<SectionHandle_t>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];
	//sectionsStack.push( handle );

	//return handle;
}

void GpuProfiler::endSection( CommandList& cmdList )
{
	//std::stack<u32>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];

	//DUSK_ASSERT( !sectionsStack.empty(), "There is no active profiling section..." );
	//if ( sectionsStack.empty() ) {
	//	return;
	//}

	//SectionHandle_t latestSectionIdx = sectionsStack.top();
	//const SectionInfos& section = sections[latestSectionIdx];

	//// Write end timestamp.
	//cmdList.writeTimestamp( *timestampQueryPool, timestampQueries[section.EndQueryHandle] );

 //   sectionsStack.pop();
}

void GpuProfiler::getSectionsResult( RenderDevice& renderDevice, CommandList& cmdList )
{
	//cmdList.retrieveQueryResults( *timestampQueryPool, sectionReadIndex, sectionCount );
	//cmdList.getQueryResult( *timestampQueryPool, timestampResults, sectionReadIndex, sectionCount );
}

u32 GpuProfiler::allocateQueryHandle( CommandList& cmdList )
{
	return 0u;

    /*const u32 handle = queryCount;
	timestampQueries[handle] = cmdList.allocateQuery( *timestampQueryPool );
    queryCount = ( ++queryCount % TOTAL_SECTION_COUNT );
	return handle;*/
}

GpuProfiler g_GpuProfiler;
