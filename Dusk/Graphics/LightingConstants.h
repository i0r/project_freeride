/*
    Dusk Source Code
    Copyright (C) 2020 Prevost Baptiste
*/
#pragma once

#include "Maths/Vector.h"

struct PhysicalLightColor
{
    // Light color RGB (in sRGB space).
    dkVec3f Color;

    // Light color temperature (in Kelvins).
    f32 Temperature;

    // Name of the preset.
    const char* Name;
};

struct PhysicalLightIntensity
{
    // Light intensity (in lumens).
    f32 Intensity;

    // Name of the preset.
    const char* Name;
};

namespace dk
{
    namespace graphics
    {
        // Convert a given color temperature (in Kelvins) to a RGB color (in sRGB space).
        constexpr dkVec3f TemperatureToRGB( const f32 temperatureInKelvin )
        {
            f32 x = temperatureInKelvin / 1000.0f;
            f32 x2 = x * x;
            f32 x3 = x2 * x;
            f32 x4 = x3 * x;
            f32 x5 = x4 * x;

            f32 R = 0.0f, G = 0.0f, B = 0.0f;

            // Red Channel
            if ( temperatureInKelvin <= 6600.0f ) {
                R = 1.0f;
            } else {
                R = 0.0002889f * x5 - 0.01258f * x4 + 0.2148f * x3 - 1.776f * x2 + 6.907f * x - 8.723f;
            }

            // Green Channel
            if ( temperatureInKelvin <= 6600.0f ) {
                G = -4.593e-05f * x5 + 0.001424f * x4 - 0.01489f * x3 + 0.0498f * x2 + 0.1669f * x - 0.1653f;
            } else {
                G = -1.308e-07f * x5 + 1.745e-05f * x4 - 0.0009116f * x3 + 0.02348f * x2 - 0.3048f * x + 2.159f;
            }

            // Blue Channel
            if ( temperatureInKelvin <= 2000.0f ) {
                B = 0.0f;
            } else if ( temperatureInKelvin < 6600.0f ) {
                B = 1.764e-05f * x5 + 0.0003575f * x4 - 0.01554f * x3 + 0.1549f * x2 - 0.3682f * x + 0.2386f;
            } else {
                B = 1.0f;
            }

            return dkVec3f( R, G, B );
        }

        // Convert a given RGB color to a temperature color (in Kelvins).
        constexpr f32 RGBToTemperature( const dkVec3f& colorRgb )
        {
            // NOTE This is actually an approximation which might not be accurate for extreme values
            f32 tMin = 2000.0f;
            f32 tMax = 23000.0f;

            f32 colorTemperature = ( tMax + tMin ) / 2.0f;

            for ( ; tMax - tMin > 0.1f; colorTemperature = ( tMax + tMin ) / 2.0f ) {
                dkVec3f newColor = TemperatureToRGB( colorTemperature );

                if ( newColor.z / newColor.x > colorRgb.z / colorRgb.x ) {
                    tMax = colorTemperature;
                } else {
                    tMin = colorTemperature;
                }
            }

            return colorTemperature;
        }

        // Light Color Presets (for editor/procedural creation).
        static constexpr PhysicalLightColor CandleFlame = { TemperatureToRGB( 1930.0f ), 1930.0f, "Candle flame" };
        static constexpr PhysicalLightColor TungstenBulb = { TemperatureToRGB( 2900.0f ), 2900.0f, "Tungsten bulb" };
        static constexpr PhysicalLightColor TungstenLamp1K = { TemperatureToRGB( 3000.0f ), 3000.0f, "Tungsten lamp 500W-1K" };
        static constexpr PhysicalLightColor QuartzLight = { TemperatureToRGB( 3500.0f ), 3500.0f, "Quartz light" };
        static constexpr PhysicalLightColor TungstenLamp2K = { TemperatureToRGB( 3275.0f ), 3275.0f, "Tungsten lamp 2K" };
        static constexpr PhysicalLightColor TungstenLamp5K = { TemperatureToRGB( 3380.0f ), 3380.0f, "Tungsten lamp 5K-10K" };
        static constexpr PhysicalLightColor MercuryVaporBulb = { TemperatureToRGB( 5700.0f ), 5700.0f, "Mercury vapor bulb" };
        static constexpr PhysicalLightColor Daylight = { TemperatureToRGB( 6000.0f ), 6000.0f, "Daylight" };
        static constexpr PhysicalLightColor RGBMonitor = { TemperatureToRGB( 6280.0f ), 6280.0f, "RGB Monitor" };
        static constexpr PhysicalLightColor FluorescentLights = { TemperatureToRGB( 6500.0f ), 6500.0f, "Fluorescent lights" };
        static constexpr PhysicalLightColor SkyOvercast = { TemperatureToRGB( 7500.0f ), 7500.0f, "Sky overcast" };
        static constexpr PhysicalLightColor OutdoorShadeAreas = { TemperatureToRGB( 8000.0f ), 8000.0f, "Outdoor shade areas" };
        static constexpr PhysicalLightColor SkyCloudy = { TemperatureToRGB( 9000.0f ), 9000.0f, "Sky partly cloudy" };

        static constexpr i32 PhysicalLightColorPresetsCount = 13;
        static constexpr const char* PhysicalLightColorPresetsLabels[PhysicalLightColorPresetsCount] = {
            CandleFlame.Name,
            TungstenBulb.Name,
            TungstenLamp1K.Name,
            QuartzLight.Name,
            TungstenLamp2K.Name,
            TungstenLamp5K.Name,
            MercuryVaporBulb.Name,
            Daylight.Name,
            RGBMonitor.Name,
            FluorescentLights.Name,
            SkyOvercast.Name,
            OutdoorShadeAreas.Name,
            SkyCloudy.Name,
        };

        static constexpr const PhysicalLightColor* PhysicalLightColorPresetsPresets[PhysicalLightColorPresetsCount] = {
            &CandleFlame,
            &TungstenBulb,
            &TungstenLamp1K,
            &QuartzLight,
            &TungstenLamp2K,
            &TungstenLamp5K,
            &MercuryVaporBulb,
            &Daylight,
            &RGBMonitor,
            &FluorescentLights,
            &SkyOvercast,
            &OutdoorShadeAreas,
            &SkyCloudy,
        };

        static constexpr int intensityPresetCount = 12;
        static constexpr char* intensityPresetLabels[intensityPresetCount] = { "LED 37 mW", "Candle flame", "LED 1W", "Incandescent lamp 40W", "Fluorescent lamp", "Tungsten Bulb 100W", "Fluorescent light", "HID Car headlight", "Induction Lamp 200W", "Low pressure sodium vapor lamp 127W", "Metal-halide lamp 400W", "Direct sunlight" };
        static constexpr f32 intensityPresetValues[intensityPresetCount] = { 0.20f, 15.0f, 75.0f, 325.0f, 1200.0f, 1600.0f, 2600.0f, 3000.0f, 16000.0f, 25000.0f, 40000.0f, 64000.0f };

    }
}
