/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include <unordered_map>
#include <stack>

#include "Core/Timer.h"

class CpuProfiler
{
public:
    struct SectionData
    {
        // Timer used to profile this section.
        Timer   SectionTimer;

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
            : Sum( 0.0 )
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
    DUSK_INLINE auto begin() const { return profiledSections.begin(); }
    DUSK_INLINE auto end() const { return profiledSections.end(); }

public:
                    CpuProfiler();
                    CpuProfiler( CpuProfiler& ) = delete;
                    ~CpuProfiler();

    // Start a new profiling section. 
    void            beginSection( const char* sectionName );

    // End the latest section pushed to the session stack.
    void            endSection();

private:
    // Hashmap keeping track of the sections across several frames.
    std::unordered_map<dkStringHash_t, SectionData> profiledSections;

    // Stack keeping track of the active sections being recorded.
    std::stack<dkStringHash_t>                      sectionsStack;
};

extern CpuProfiler g_CpuProfiler;

struct CpuScopedSection
{
public:
    CpuScopedSection( const char* sectionName )
    {
        g_CpuProfiler.beginSection( sectionName );
    }

    ~CpuScopedSection()
    {
        g_CpuProfiler.endSection();
    }
};

// Automatically profile CPU operations for the active scope (brace enclosed).
#define DUSK_CPU_PROFILE_SCOPED( sectionName ) CpuScopedSection __CpuProfilingSection__( sectionName );

// Automatically profile CPU operations for the given function.
#define DUSK_CPU_PROFILE_FUNCTION CpuScopedSection __CpuProfilingSection__( __FUNCTION__ );
