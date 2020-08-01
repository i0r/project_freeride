/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#if DUSK_USE_IMGUI
#include <Maths/Vector.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace dk
{
	namespace imgui
	{
		static bool IsItemActiveLastFrame()
		{
			ImGuiContext& g = *GImGui;

			if ( g.ActiveIdPreviousFrame )
				return g.ActiveIdPreviousFrame == g.CurrentWindow->DC.LastItemId;
			return false;
		}

		static bool IsItemJustReleased() 
		{ 
			return IsItemActiveLastFrame() && !ImGui::IsItemActive(); 
		}
		
		static bool IsItemJustActivated() 
		{ 
			return !IsItemActiveLastFrame() && ImGui::IsItemActive(); 
		}

		static DUSK_INLINE void InputMatrix4x4( dkMat4x4f& inputMat44 )
        {
            ImGui::InputFloat4( "r0", ( f32* )inputMat44[0].scalars );
            ImGui::InputFloat4( "r1", ( f32* )inputMat44[1].scalars );
            ImGui::InputFloat4( "r2", ( f32* )inputMat44[2].scalars );
            ImGui::InputFloat4( "r3", ( f32* )inputMat44[3].scalars );
		}

		static bool DragFloat3WithChannelLock( const char* name, dkVec3f* inputFloat3, bool& lockScaleChannels )
		{
			dkVec3f preEditScale = *inputFloat3;
			bool hasBeenManipulated = ImGui::DragFloat3( name, ( f32* )inputFloat3 );
			ImGui::SameLine();
			if ( ImGui::Button( lockScaleChannels ? "U" : "L" ) ) {
				lockScaleChannels = !lockScaleChannels;
			}

			if ( lockScaleChannels ) {
				dkVec3f postEditScale = *inputFloat3;

				for ( i32 i = 0; i < 3; i++ ) {
					if ( preEditScale[i] != postEditScale[i] ) {
						inputFloat3[0] = inputFloat3[1] = inputFloat3[2] = postEditScale[i];
						break;
					}
				}
			}

			return hasBeenManipulated;
		}

        bool BeginPopupContextWindowWithCondition( const char* str_id, bool isOpened, bool also_over_items = true )
        {
            if ( !str_id )
                str_id = "window_context";
            ImGuiID id = GImGui->CurrentWindow->GetID( str_id );
            if ( isOpened && ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByPopup ) )
                if ( also_over_items || !ImGui::IsAnyItemHovered() )
                    ImGui::OpenPopupEx( id );
            return ImGui::BeginPopupEx( id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings );
        }
	}
}
#endif
