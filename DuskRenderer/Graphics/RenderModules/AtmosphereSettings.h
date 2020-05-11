/*
    Project Motorway Source Code
    Copyright (C) 2018 Prévost Baptiste

    This file is part of Project Motorway source code.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

enum atmosphereLuminance_t
{
    // Render the spectral radiance at kLambdaR, kLambdaG, kLambdaB.
    ATMOSPHERE_LUMINANCE_NONE,
    // Render the sRGB luminance, using an approximate (on the fly) conversion
    // from 3 spectral radiance values only (see section 14.3 in <a href=
    // "https://arxiv.org/pdf/1612.04336.pdf">A Qualitative and Quantitative
    //  Evaluation of 8 Clear Sky Models</a>).
    ATMOSPHERE_LUMINANCE_APPROXIMATE,
    // Render the sRGB luminance, precomputed from 15 spectral radiance values
    // (see section 4.4 in <a href=
    // "http://www.oskee.wz.cz/stranka/uploads/SCCG10ElekKmoch.pdf">Real-time
    //  Spectral Scattering in Large-scale Natural Participating Media</a>).
    ATMOSPHERE_LUMINANCE_PRECOMPUTED
};

// Atmospheric Model Settings
constexpr bool use_half_precision_ = true;
constexpr bool use_constant_solar_spectrum_ = false;
constexpr bool use_ozone_ = true;
constexpr bool use_combined_textures_ = true;
constexpr bool do_white_balance_ = true;
constexpr atmosphereLuminance_t use_luminance_ = ATMOSPHERE_LUMINANCE_NONE;
constexpr u32 num_precomputed_wavelengths = use_luminance_ == ATMOSPHERE_LUMINANCE_PRECOMPUTED ? 15 : 3;
constexpr u32 num_scattering_orders = 4;

static constexpr f64 kLambdaR = 680.0;
static constexpr f64 kLambdaG = 550.0;
static constexpr f64 kLambdaB = 440.0;

static constexpr const dkChar_t* ATMOSPHERE_TRANSMITTANCE_TEXTURE_NAME = static_cast<const dkChar_t* const>( DUSK_STRING( "GameData/textures/AtmosphereTransmittance.dds" ) );
static constexpr const dkChar_t* ATMOSPHERE_SCATTERING_TEXTURE_NAME = static_cast<const dkChar_t* const>( DUSK_STRING( "GameData/textures/AtmosphereScattering.dds" ) );
static constexpr const dkChar_t* ATMOSPHERE_IRRADIANCE_TEXTURE_NAME = static_cast<const dkChar_t* const>( DUSK_STRING( "GameData/textures/AtmosphereIrradiance.dds" ) );
