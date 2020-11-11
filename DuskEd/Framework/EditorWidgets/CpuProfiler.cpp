/*
	Dusk Source Code
	Copyright (C) 2020 Prevost Baptiste
*/
#include <Shared.h>
#include "CpuProfiler.h"

#if DUSK_USE_IMGUI
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#endif

#include "Core/CpuProfiler.h"
#include "Core/Environment.h"
#include "Core/StringHelpers.h"

CpuProfilerWidget::CpuProfilerWidget()
	: isOpen( false )
{

}

CpuProfilerWidget::~CpuProfilerWidget()
{

}

void CpuProfilerWidget::displayEditorWindow()
{
	if ( !isOpen ) {
		return;
	}

	if( ImGui::Begin( "CPU Profiler", &isOpen ) ) {
        ImGui::Columns( 4, "ProfilerCols" ); // 4-ways, with border

        ImGui::Text( "Name" ); ImGui::NextColumn();
        ImGui::Text( "Average" ); ImGui::NextColumn();
        ImGui::Text( "Min" ); ImGui::NextColumn();
        ImGui::Text( "Max" ); ImGui::NextColumn();

        for ( auto& section : g_CpuProfiler ) {
            ImGui::Text( section.second.Name.c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( CpuProfiler::SectionData::CalculateAverage( section.second ) ).c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( section.second.Minimum ).c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( section.second.Maximum ).c_str() ); ImGui::NextColumn();
        }

        ImGui::Columns( 1 );
        ImGui::Separator();
		ImGui::End();
	}

    if ( ImGui::Begin( "GPU Profiler", &isOpen ) ) {
        ImGui::Columns( 4, "ProfilerCols" ); // 4-ways, with border

        ImGui::Text( "Name" ); ImGui::NextColumn();
        ImGui::Text( "Average" ); ImGui::NextColumn();
        ImGui::Text( "Min" ); ImGui::NextColumn();
        ImGui::Text( "Max" ); ImGui::NextColumn();

        for ( auto& section : g_GpuProfiler ) {
            ImGui::Text( section.second.Name.c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( GpuProfiler::SectionData::CalculateAverage( section.second ) ).c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( section.second.Minimum ).c_str() ); ImGui::NextColumn();
            ImGui::Text( std::to_string( section.second.Maximum ).c_str() ); ImGui::NextColumn();
        }

        ImGui::Columns( 1 );
        ImGui::Separator();
        ImGui::End();
    }
}

void CpuProfilerWidget::openWindow()
{
	isOpen = true;
}

void CpuProfilerWidget::loadCachedResources( GraphicsAssetCache* graphicsAssetCache )
{

}
