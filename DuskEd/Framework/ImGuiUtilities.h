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


		static void DragFloat3WithChannelLock( dkVec3f* inputFloat3, bool& lockScaleChannels )
		{
			dkVec3f preEditScale = *inputFloat3;
			ImGui::DragFloat3( "Scale", ( f32* )inputFloat3 );
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
		}
	}
}
#endif
#endif
