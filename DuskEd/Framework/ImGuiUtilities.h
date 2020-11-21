/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#if DUSK_USE_IMGUI
#include <Maths/Vector.h>
#include "Graphics/LightingConstants.h"

#include "imgui.h"
#include "imgui_internal.h"

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

        static bool BeginPopupContextWindowWithCondition( const char* str_id, bool isOpened, bool also_over_items = true )
        {
            if ( !str_id )
                str_id = "window_context";
            ImGuiID id = GImGui->CurrentWindow->GetID( str_id );
            if ( isOpened && ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByPopup ) )
                if ( also_over_items || !ImGui::IsAnyItemHovered() )
                    ImGui::OpenPopupEx( id );
            return ImGui::BeginPopupEx( id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings );
        }

        static bool LightIntensity( f32& luminousIntensity )
        {
            bool hasChanged = false;

            static i32 intensityPresetIndex = -1;
            hasChanged = ImGui::SliderFloat( "Luminous Intensity (lm)", &luminousIntensity, 0.0f, 100000.0f );

            if ( hasChanged ) {
                intensityPresetIndex = -1;
            }

            if ( ImGui::Combo( "Luminous Intensity Preset", &intensityPresetIndex, dk::graphics::intensityPresetLabels, dk::graphics::intensityPresetCount ) && intensityPresetIndex != -1 ) {
                luminousIntensity = dk::graphics::intensityPresetValues[intensityPresetIndex];
                hasChanged = true;
            }

            return hasChanged;
        }

        static bool LightColor( dkVec3f& colorRGB )
        {
            bool hasChanged = false;

            static i32 activeColorMode = 0;
            static i32 temperaturePresetIndex = -1;
            
            constexpr const char* COLOR_MODES[2] =
            {
                "RGB | RGB COLOR",
                "KELVIN | TEMPERATURE"
            };

            ImGui::Combo( "Color Mode", ( i32* )&activeColorMode, COLOR_MODES, 2 );

            if ( activeColorMode == 0 ) {
                hasChanged = ImGui::ColorEdit3( "Color (sRGB)", ( f32* )&colorRGB[0] );
            } else {
                f32 colorTemperature = dk::graphics::RGBToTemperature( colorRGB );
                hasChanged = ImGui::SliderFloat( "Temperature (K)", &colorTemperature, 0.0f, 9000.0f );

                if ( hasChanged ) {
                    colorRGB = dk::graphics::TemperatureToRGB( colorTemperature );
                }
            }

            if ( hasChanged ) {
                temperaturePresetIndex = -1;
            }

            if ( ImGui::Combo( "Color Preset", &temperaturePresetIndex, dk::graphics::PhysicalLightColorPresetsLabels, dk::graphics::PhysicalLightColorPresetsCount ) && temperaturePresetIndex != -1 ) {
                colorRGB = dk::graphics::PhysicalLightColorPresetsPresets[temperaturePresetIndex]->Color;
                hasChanged = true;
            }

            return hasChanged;
        }
	}
}
#endif
