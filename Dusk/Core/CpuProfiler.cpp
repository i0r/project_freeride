/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include "Shared.h"
#include "CpuProfiler.h"

#include "Core/Timer.h"

CpuProfiler::CpuProfiler()
{

}

CpuProfiler::~CpuProfiler()
{

}

void CpuProfiler::beginSection( const char* sectionName )
{
    dkStringHash_t sectionHashcode = dk::core::CRC32( sectionName );

    SectionData& section = profiledSections[sectionHashcode];
    section.SectionTimer.reset();
    section.Parent = ( !sectionsStack.empty() ) ? &profiledSections[sectionsStack.top()] : nullptr;
    section.Name = sectionName;

    sectionsStack.push( sectionHashcode );
}

void CpuProfiler::endSection()
{
    DUSK_ASSERT( !sectionsStack.empty(), "There is no active profiling section..." );
    
    if ( sectionsStack.empty() ) {
        return;
    }

    dkStringHash_t latestSectionIdx = sectionsStack.top();
    
    SectionData& section = profiledSections[latestSectionIdx];
    section.Sum += section.SectionTimer.getElapsedTimeAsMiliseconds();
    section.SampleCount++;
    section.CallCount++;

    f32 sectionTiming = static_cast< f32 >( section.SectionTimer.getElapsedTimeAsMiliseconds() );
    section.Maximum = Max( section.Maximum, sectionTiming );
    section.Minimum = Min( section.Minimum, sectionTiming );

    sectionsStack.pop();
}

CpuProfiler g_CpuProfiler;
