/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <stack>

#include "Rendering/RenderDevice.h" // Required by CMD_LIST_POOL_CAPACITY.

class CommandList;
struct QueryPool;

class GpuProfiler
{
public:
    using SectionHandle_t = u32;
public:
	struct SectionInfos
	{
		// Index in the timestampQueries array pointing to the section start query handle.
		u32     BeginQueryHandle;

		// Index in the timestampQueries array pointing to the section end query handle.
		u32     EndQueryHandle;

		// Timing measured for this section (in ms).
		f64     Result;
	};
public:
    DUSK_INLINE const SectionInfos* begin() const { return static_cast< const SectionInfos* >( &sections[0] ); }
	DUSK_INLINE const SectionInfos* end() const { return static_cast< const SectionInfos* >( &sections[sectionCount] ); }

public:
                    GpuProfiler();
                    GpuProfiler( GpuProfiler& ) = delete;
                    ~GpuProfiler();

    void            create( RenderDevice& renderDevice );

    void            destroy( RenderDevice& renderDevice );

    void            update( RenderDevice& renderDevice );

    // Start a new profiling section. Return a valid section handle if the 
    // operation was successful; return an invalid handle otherwise.
    SectionHandle_t beginSection( CommandList& cmdList );

    // End the latest section pushed to the session stack.
    void            endSection( CommandList& cmdList );

    // Return the measured timing (in ms) of a given section. Return < 0 if
    // the section result is not available yet).
    f64             getSectionResultInMs( const SectionHandle_t handle ) const;

private:
    // The number of frame to wait until the Profiler can readback from the GPU.
	static constexpr i32 RESULT_RETRIVAL_FRAME_LAG = 5;

    // The maximum number of profiling section (per frame).
	static constexpr i32 MAX_PROFILE_SECTION_COUNT = 128;

    // The number of section allocated by the profiler.
	static constexpr i32 TOTAL_SECTION_COUNT = MAX_PROFILE_SECTION_COUNT * RESULT_RETRIVAL_FRAME_LAG;

private:
    // The QueryPool used for timestamp query allocation.
    QueryPool*      timestampQueryPool;

    // The number of section recorded for a given frame. Must be atomic
    // since it will modified from multiple thread at once.
    std::atomic<u32> sectionCount;
    
    // Index pointing to the latest section read by the Profiler.
    u32             sectionReadIndex;

    // Index pointing to the next writable section.
    u32             sectionWriteIndex;

    // Index pointing to the next query allocable in timestampQueries array.
    u32             queryCount;

    // Stack keeping track of the active sections being recorded.
    // We allocate one stack per command list to guarantee thread safeness
    // and avoid heavy synchronizations.
    std::stack<SectionHandle_t> sectionsStacks[RenderDevice::CMD_LIST_POOL_CAPACITY];

	// Array holding active profiling sections.
    SectionInfos                sections[TOTAL_SECTION_COUNT];

    // Query handle allocated for section profiling.
    u32                         timestampQueries[TOTAL_SECTION_COUNT];

    // Query result retrieved from the GPU. The result is API specific.
    u64                         timestampResults[TOTAL_SECTION_COUNT];

    // Query result as milliseconds. This is basically timestampResults as
    // milliseconds.
    f64                         timestampResultsAsMs[TOTAL_SECTION_COUNT];

private:
    // Retrieve query timing from the GPU until we reach the end of the queries
    // or an unavailable query.
    void getSectionsResult( RenderDevice& renderDevice, CommandList& cmdList );

    // Allocate a query from the timestamp pool and return its index in the timestampQueries array.
    u32  allocateQueryHandle( CommandList& cmdList );
};

extern GpuProfiler g_GpuProfiler;

struct GpuScopedSection
{
private:
    GpuProfiler::SectionHandle_t    sectionHandle;

    CommandList&                    commandList;

public:
    GpuScopedSection( CommandList& cmdList )
        : sectionHandle( g_GpuProfiler.beginSection( cmdList ) )
        , commandList( cmdList )
    {

    }

    ~GpuScopedSection()
    {
        g_GpuProfiler.endSection( commandList );
    }
};

// Automatically profile GPU operations for the active scope (brace enclosed).
#define DUSK_GPU_PROFILE_SCOPED( cmdList ) GpuScopedSection __ProfilingSection__( cmdList );
