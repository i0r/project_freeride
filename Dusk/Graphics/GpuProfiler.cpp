/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "GpuProfiler.h"

#include "Rendering/CommandList.h"

GpuProfiler::GpuProfiler()
	: timestampQueryPool( nullptr )
	, internalIndex( -1 )
{
    memset( perInternalFrameSectionCount, 0, sizeof( u32 ) * RESULT_RETRIVAL_FRAME_LAG );
    memset( perInternalFrameSectionStartIndex, 0, sizeof( u32 ) * RESULT_RETRIVAL_FRAME_LAG );
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
	internalIndex = ( ++internalIndex % RESULT_RETRIVAL_FRAME_LAG );

    const size_t frameIndex = renderDevice.getFrameIndex();
    if ( frameIndex < RESULT_RETRIVAL_FRAME_LAG - 1 ) {
        return;
    }

    const u32 retrievalInternalIndex = static_cast<u32>( frameIndex - ( RESULT_RETRIVAL_FRAME_LAG - 1 ) ) % RESULT_RETRIVAL_FRAME_LAG;

	// Note: We explicitly request a cmdlist for query readback since 
	// Vulkan query readback must be done on a Graphics or Compute queue.
	CommandList& readBackCmdList = renderDevice.allocateComputeCommandList();
    readBackCmdList.begin();
    readBackCmdList.retrieveQueryResults( *timestampQueryPool, 
                                            perInternalFrameSectionStartIndex[retrievalInternalIndex], 
                                            perInternalFrameSectionCount[retrievalInternalIndex] * 2 );

    // Readback query results from the GPU.
    u64 timestampsResults[MAX_PROFILE_SECTION_COUNT];
    memset( timestampsResults, 0, sizeof( u64 ) * MAX_PROFILE_SECTION_COUNT );

    readBackCmdList.getQueryResult( *timestampQueryPool, 
                                    timestampsResults, 
                                    perInternalFrameSectionStartIndex[retrievalInternalIndex],
                                    perInternalFrameSectionCount[retrievalInternalIndex] * 2 );

    // Update section records.
    i32 timestampIdx = 0;
    for ( auto& sectionEntry : profiledSections ) {
        SectionData& section = sectionEntry.second;
            
        u64 beginTiming = timestampsResults[timestampIdx + 0];
        u64 endTiming = timestampsResults[timestampIdx + 1];

        f64 beginTimingMs = renderDevice.convertTimestampToMs( beginTiming );
        f64 endTimingMs = renderDevice.convertTimestampToMs( endTiming );

        f32 sectionTiming = static_cast<f32>( endTimingMs - beginTimingMs );

        section.Sum += sectionTiming;
        section.SampleCount++;
        section.CallCount++;

        section.Maximum = Max( section.Maximum, sectionTiming );
        section.Minimum = Min( section.Minimum, sectionTiming );

        timestampIdx += 2;
    }
	readBackCmdList.end();

	renderDevice.submitCommandList( readBackCmdList );

    perInternalFrameSectionCount[retrievalInternalIndex] = 0;
    perInternalFrameSectionStartIndex[retrievalInternalIndex] = 0;
}

void GpuProfiler::beginSection( CommandList& cmdList, const std::string& sectionName )
{
    dkStringHash_t sectionHashcode = dk::core::CRC32( sectionName.c_str() );

    SectionData& section = profiledSections[sectionHashcode];
	section.Name = sectionName;
    section.BeginQueryHandle[internalIndex] = cmdList.allocateQuery( *timestampQueryPool );
    section.EndQueryHandle[internalIndex] = cmdList.allocateQuery( *timestampQueryPool );

	std::stack<dkStringHash_t>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];
    section.Parent = ( !sectionsStack.empty() ) ? &profiledSections[sectionsStack.top()] : nullptr;
    
    sectionsStack.push( sectionHashcode );

	cmdList.writeTimestamp( *timestampQueryPool, section.BeginQueryHandle[internalIndex] );
}

void GpuProfiler::endSection( CommandList& cmdList )
{
    std::stack<dkStringHash_t>& sectionsStack = sectionsStacks[cmdList.getCommandListPooledIndex()];
    DUSK_ASSERT( !sectionsStack.empty(), "There is no active profiling section..." );

    if ( sectionsStack.empty() ) {
        return;
    }

    dkStringHash_t latestSectionIdx = sectionsStack.top();

    SectionData& section = profiledSections[latestSectionIdx];

    // We linearly allocate handles from the pool. Therefore the first handle allocated is the first begin timestamp
    // allocated for a given frame.
    if ( perInternalFrameSectionCount[internalIndex] == 0 ) {
        perInternalFrameSectionStartIndex[internalIndex] = section.BeginQueryHandle[internalIndex];
    }
    perInternalFrameSectionCount[internalIndex]++;

    cmdList.writeTimestamp( *timestampQueryPool, section.EndQueryHandle[internalIndex] );

    sectionsStack.pop();
}

void GpuProfiler::getSectionsResult( RenderDevice& renderDevice, CommandList& cmdList )
{
}

GpuProfiler g_GpuProfiler;
