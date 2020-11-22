#include <Shared.h>
#include "LoggingConsole.h"

#include <Core/Logger.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "ThirdParty/Google/include/IconsMaterialDesign.h"

#if DUSK_USE_IMGUI
#if DUSK_UNICODE
static thread_local char logHistory[2 * 4096];

DUSK_INLINE static const char* NarrowLogging( const dkChar_t* streamPointer, const i32 streamLength )
{
    ImTextStrToUtf8( logHistory, ( sizeof( char ) * 2 * 4096 ), reinterpret_cast< const ImWchar* >( streamPointer ), reinterpret_cast<const ImWchar*>( streamPointer + streamLength ) );
    return logHistory;
}
#else
DUSK_INLINE static const char* NarrowLogging( const char* streamPointer, const i32 streamLength )
{
    return streamPointer;
}
#endif

void dk::editor::DisplayLoggingConsole()
{
    const char* logData = NarrowLogging( g_LogHistoryTest, g_LogHistoryPointer );

    if ( ImGui::Begin( ICON_MD_DEVELOPER_MODE " Console", nullptr ) ) {
        ImGui::BeginChild( "scrolling" );
        ImGui::TextUnformatted( logData );
        ImGui::SetScrollHereY( 1.0f );
        ImGui::EndChild();
    }
    ImGui::End();
}
#endif