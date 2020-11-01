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
	struct SectionData
	{
		// Index in the timestampQueries array pointing to the section start query handle.
		u32     BeginQueryHandle;

		// Index in the timestampQueries array pointing to the section end query handle.
		u32     EndQueryHandle;

        // Sum of all the samples timing. 
        f64     Sum;

        // Number of sample for this section.
        u64     SampleCount;

        // Call count for this section (per frame).
        u64     CallCount;

        // Max timing for this section.
        f32     Maximum;

        // Min timing for this section.
        f32     Minimum;

        // Pointer to the parent for hierarchal profiling. Null if this section is orphan.
        SectionData* Parent;

        // Name of the section (used to calculate the hashcode of the function).
        std::string Name;

        SectionData()
            : BeginQueryHandle( 0 )
            , EndQueryHandle( 0 )
            , Sum( 0.0 )
            , SampleCount( 0ull )
            , CallCount( 0ull )
            , Maximum( -std::numeric_limits<f32>::max() )
            , Minimum( +std::numeric_limits<f32>::max() )
            , Parent( nullptr )
            , Name( "" )
        {

        }

        static f64 CalculateAverage( const SectionData& section )
        {
            return section.Sum / static_cast< f64 >( section.SampleCount );
        }
	};

public:
                    GpuProfiler();
                    GpuProfiler( GpuProfiler& ) = delete;
                    ~GpuProfiler();

    void            create( RenderDevice& renderDevice );

    void            destroy( RenderDevice& renderDevice );

    void            update( RenderDevice& renderDevice );

    // Start a new profiling section. Return a valid section handle if the 
    // operation was successful; return an invalid handle otherwise.
    void            beginSection( CommandList& cmdList, const std::string& sectionName );

    // End the latest section pushed to the session stack.
    void            endSection( CommandList& cmdList );

private:
    // The number of frame to wait until the Profiler can readback from the GPU.
	static constexpr i32 RESULT_RETRIVAL_FRAME_LAG = 5;

    // The maximum number of profiling section (per frame).
	static constexpr i32 MAX_PROFILE_SECTION_COUNT = 128;

    // The number of section allocated by the profiler.
	static constexpr i32 TOTAL_SECTION_COUNT = MAX_PROFILE_SECTION_COUNT * RESULT_RETRIVAL_FRAME_LAG;

private:
    // The QueryPool used for timestamp query allocation.
    QueryPool*       timestampQueryPool;

    // Last update frame index. 
    size_t           lastUpdateFrameIndex;

    // Hashmap keeping track of the sections across several frames.
    std::unordered_map<dkStringHash_t, SectionData> profiledSections;

    // Stack keeping track of the active sections being recorded.
    // We allocate one stack per command list to guarantee thread safeness
    // and avoid heavy synchronizations.
    std::stack<u32> sectionsStacks[RenderDevice::CMD_LIST_POOL_CAPACITY];

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
    // The command list used to execute GPU operations.
    CommandList&                    commandList;

public:
    GpuScopedSection( CommandList& cmdList, const std::string& sectionName )
        : commandList( cmdList )
    {
        g_GpuProfiler.beginSection( commandList, sectionName );
    }

    ~GpuScopedSection()
    {
        g_GpuProfiler.endSection( commandList );
    }
};

// Automatically profile GPU operations for the active scope (brace enclosed).
#define DUSK_GPU_PROFILE_SCOPED( cmdList, sectionName ) GpuScopedSection __GpuProfilingSection__( cmdList, sectionName );
