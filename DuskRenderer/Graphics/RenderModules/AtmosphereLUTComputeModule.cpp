/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "AtmosphereLUTComputeModule.h"

#include <Graphics/ShaderCache.h>
#include <Graphics/FrameGraph.h>

#include <Core/ViewFormat.h>
#include <Rendering/CommandList.h>

#include <Maths/Helpers.h>

// Atmospheric Model Settings
enum atmosphereLuminance_t {
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

constexpr atmosphereLuminance_t use_luminance_ = ATMOSPHERE_LUMINANCE_NONE;
constexpr bool use_half_precision_ = true;
constexpr bool use_constant_solar_spectrum_ = false;
constexpr bool use_ozone_ = true;
constexpr bool use_combined_textures_ = true;
constexpr u32 num_scattering_orders = 4;
constexpr f64 kLambdaR = 680.0;
constexpr f64 kLambdaG = 550.0;
constexpr f64 kLambdaB = 440.0;

// Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
// (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
// summed and averaged in each bin (e.g. the value for 360nm is the average
// of the ASTM G-173 values for all wavelengths between 360 and 370nm).
// Values in W.m^-2.
constexpr i32 kLambdaMin = 360;
constexpr i32 kLambdaMax = 830;
constexpr f64 kSolarIrradiance[48] = {
    1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
    1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
    1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
    1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
    1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
    1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
};
// Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
// referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
// each bin (e.g. the value for 360nm is the average of the original values
// for all wavelengths between 360 and 370nm). Values in m^2.
constexpr f64 kOzoneCrossSection[48] = {
    1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
    8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
    1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
    4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
    2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
    6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
    2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
};
// From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
constexpr f64 kDobsonUnit = 2.687e20;
// Maximum number density of ozone molecules, in m^-3 (computed so at to get
// 300 Dobson units of ozone - for this we divide 300 DU by the integral of
// the ozone density profile defined below, which is equal to 15km).
constexpr f64 kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;
// Wavelength independent solar irradiance "spectrum" (not physically
// realistic, but was used in the original implementation).
constexpr f64 kConstantSolarIrradiance = 1.5;
constexpr f64 kTopRadius = 6420000.0;
constexpr f64 kRayleigh = 1.24062e-6;
constexpr f64 kRayleighScaleHeight = 8000.0;
constexpr f64 kMieScaleHeight = 1200.0;
constexpr f64 kMieAngstromAlpha = 0.0;
constexpr f64 kMieAngstromBeta = 5.328e-3;
constexpr f64 kMieSingleScatteringAlbedo = 0.9;
constexpr f64 kMiePhaseFunctionG = 0.8;
constexpr f64 kGroundAlbedo = 0.1;
const f64 max_sun_zenith_angle =
( use_half_precision_ ? 102.0 : 120.0 ) / 180.0 * dk::maths::PI<f64>();


// An atmosphere layer of width 'width' (in m), and whose density is defined as
//   'exp_term' * exp('exp_scale' * h) + 'linear_term' * h + 'constant_term',
// clamped to [0,1], and where h is the altitude (in m). 'exp_term' and
// 'constant_term' are unitless, while 'exp_scale' and 'linear_term' are in
// m^-1.
struct densityProfileLayer_t {
    f64 width;
    f64 exp_term;
    f64 exp_scale;
    f64 linear_term;
    f64 constant_term;
};

constexpr i32 SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
constexpr i32 SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
constexpr i32 SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;

// The conversion factor between watts and lumens.
constexpr f64 MAX_LUMINOUS_EFFICACY = 683.0;

const f64 kSunSolidAngle = dk::maths::PI<f64>() * kSunAngularRadius * kSunAngularRadius;

// Values from "CIE (1931) 2-deg color matching functions", see
// "http://web.archive.org/web/20081228084047/
//    http://www.cvrl.org/database/data/cmfs/ciexyz31.txt".
constexpr f64 CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[380] = {
    360, 0.000129900000, 0.000003917000, 0.000606100000,
    365, 0.000232100000, 0.000006965000, 0.001086000000,
    370, 0.000414900000, 0.000012390000, 0.001946000000,
    375, 0.000741600000, 0.000022020000, 0.003486000000,
    380, 0.001368000000, 0.000039000000, 0.006450001000,
    385, 0.002236000000, 0.000064000000, 0.010549990000,
    390, 0.004243000000, 0.000120000000, 0.020050010000,
    395, 0.007650000000, 0.000217000000, 0.036210000000,
    400, 0.014310000000, 0.000396000000, 0.067850010000,
    405, 0.023190000000, 0.000640000000, 0.110200000000,
    410, 0.043510000000, 0.001210000000, 0.207400000000,
    415, 0.077630000000, 0.002180000000, 0.371300000000,
    420, 0.134380000000, 0.004000000000, 0.645600000000,
    425, 0.214770000000, 0.007300000000, 1.039050100000,
    430, 0.283900000000, 0.011600000000, 1.385600000000,
    435, 0.328500000000, 0.016840000000, 1.622960000000,
    440, 0.348280000000, 0.023000000000, 1.747060000000,
    445, 0.348060000000, 0.029800000000, 1.782600000000,
    450, 0.336200000000, 0.038000000000, 1.772110000000,
    455, 0.318700000000, 0.048000000000, 1.744100000000,
    460, 0.290800000000, 0.060000000000, 1.669200000000,
    465, 0.251100000000, 0.073900000000, 1.528100000000,
    470, 0.195360000000, 0.090980000000, 1.287640000000,
    475, 0.142100000000, 0.112600000000, 1.041900000000,
    480, 0.095640000000, 0.139020000000, 0.812950100000,
    485, 0.057950010000, 0.169300000000, 0.616200000000,
    490, 0.032010000000, 0.208020000000, 0.465180000000,
    495, 0.014700000000, 0.258600000000, 0.353300000000,
    500, 0.004900000000, 0.323000000000, 0.272000000000,
    505, 0.002400000000, 0.407300000000, 0.212300000000,
    510, 0.009300000000, 0.503000000000, 0.158200000000,
    515, 0.029100000000, 0.608200000000, 0.111700000000,
    520, 0.063270000000, 0.710000000000, 0.078249990000,
    525, 0.109600000000, 0.793200000000, 0.057250010000,
    530, 0.165500000000, 0.862000000000, 0.042160000000,
    535, 0.225749900000, 0.914850100000, 0.029840000000,
    540, 0.290400000000, 0.954000000000, 0.020300000000,
    545, 0.359700000000, 0.980300000000, 0.013400000000,
    550, 0.433449900000, 0.994950100000, 0.008749999000,
    555, 0.512050100000, 1.000000000000, 0.005749999000,
    560, 0.594500000000, 0.995000000000, 0.003900000000,
    565, 0.678400000000, 0.978600000000, 0.002749999000,
    570, 0.762100000000, 0.952000000000, 0.002100000000,
    575, 0.842500000000, 0.915400000000, 0.001800000000,
    580, 0.916300000000, 0.870000000000, 0.001650001000,
    585, 0.978600000000, 0.816300000000, 0.001400000000,
    590, 1.026300000000, 0.757000000000, 0.001100000000,
    595, 1.056700000000, 0.694900000000, 0.001000000000,
    600, 1.062200000000, 0.631000000000, 0.000800000000,
    605, 1.045600000000, 0.566800000000, 0.000600000000,
    610, 1.002600000000, 0.503000000000, 0.000340000000,
    615, 0.938400000000, 0.441200000000, 0.000240000000,
    620, 0.854449900000, 0.381000000000, 0.000190000000,
    625, 0.751400000000, 0.321000000000, 0.000100000000,
    630, 0.642400000000, 0.265000000000, 0.000049999990,
    635, 0.541900000000, 0.217000000000, 0.000030000000,
    640, 0.447900000000, 0.175000000000, 0.000020000000,
    645, 0.360800000000, 0.138200000000, 0.000010000000,
    650, 0.283500000000, 0.107000000000, 0.000000000000,
    655, 0.218700000000, 0.081600000000, 0.000000000000,
    660, 0.164900000000, 0.061000000000, 0.000000000000,
    665, 0.121200000000, 0.044580000000, 0.000000000000,
    670, 0.087400000000, 0.032000000000, 0.000000000000,
    675, 0.063600000000, 0.023200000000, 0.000000000000,
    680, 0.046770000000, 0.017000000000, 0.000000000000,
    685, 0.032900000000, 0.011280000000, 0.000000000000,
    690, 0.022700000000, 0.008210000000, 0.000000000000,
    695, 0.015840000000, 0.005723000000, 0.000000000000,
    700, 0.011359160000, 0.004102000000, 0.000000000000,
    705, 0.008110916000, 0.002929000000, 0.000000000000,
    710, 0.005790346000, 0.002091000000, 0.000000000000,
    715, 0.004109457000, 0.001484000000, 0.000000000000,
    720, 0.002899327000, 0.001047000000, 0.000000000000,
    725, 0.002049190000, 0.000740000000, 0.000000000000,
    730, 0.001439971000, 0.000520000000, 0.000000000000,
    735, 0.000999949300, 0.000361100000, 0.000000000000,
    740, 0.000690078600, 0.000249200000, 0.000000000000,
    745, 0.000476021300, 0.000171900000, 0.000000000000,
    750, 0.000332301100, 0.000120000000, 0.000000000000,
    755, 0.000234826100, 0.000084800000, 0.000000000000,
    760, 0.000166150500, 0.000060000000, 0.000000000000,
    765, 0.000117413000, 0.000042400000, 0.000000000000,
    770, 0.000083075270, 0.000030000000, 0.000000000000,
    775, 0.000058706520, 0.000021200000, 0.000000000000,
    780, 0.000041509940, 0.000014990000, 0.000000000000,
    785, 0.000029353260, 0.000010600000, 0.000000000000,
    790, 0.000020673830, 0.000007465700, 0.000000000000,
    795, 0.000014559770, 0.000005257800, 0.000000000000,
    800, 0.000010253980, 0.000003702900, 0.000000000000,
    805, 0.000007221456, 0.000002607800, 0.000000000000,
    810, 0.000005085868, 0.000001836600, 0.000000000000,
    815, 0.000003581652, 0.000001293400, 0.000000000000,
    820, 0.000002522525, 0.000000910930, 0.000000000000,
    825, 0.000001776509, 0.000000641530, 0.000000000000,
    830, 0.000001251141, 0.000000451810, 0.000000000000,
};

// The conversion matrix from XYZ to linear sRGB color spaces.
// Values from https://en.wikipedia.org/wiki/SRGB.
constexpr f64 XYZ_TO_SRGB[9] = {
    +3.2406, -1.5372, -0.4986,
    -0.9689, +1.8758, +0.0415,
    +0.0557, -0.2040, +1.0570
};

f64 Interpolate(
    const std::vector<f64>& wavelengths,
    const std::vector<f64>& wavelength_function,
    f64 wavelength )
{
    if ( wavelength < wavelengths[0] ) {
        return wavelength_function[0];
    }
    for ( u32 i = 0; i < wavelengths.size() - 1; ++i ) {
        if ( wavelength < wavelengths[i + 1] ) {
            f64 u =
                ( wavelength - wavelengths[i] ) / ( wavelengths[i + 1] - wavelengths[i] );
            return
                wavelength_function[i] * ( 1.0 - u ) + wavelength_function[i + 1] * u;
        }
    }
    return wavelength_function[wavelength_function.size() - 1];
}

f64 CieColorMatchingFunctionTableValue( f64 wavelength, i32 column )
{
    if ( wavelength <= kLambdaMin || wavelength >= kLambdaMax ) {
        return 0.0;
    }
    f64 u = ( wavelength - kLambdaMin ) / 5.0;
    i32 row = static_cast< i32 >( std::floor( u ) );
    u -= row;
    return CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * row + column] * ( 1.0 - u ) +
        CIE_2_DEG_COLOR_MATCHING_FUNCTIONS[4 * ( row + 1 ) + column] * u;
}

// The returned constants are in lumen.nm / watt.
void ComputeSpectralRadianceToLuminanceFactors(
    const std::vector<f64>& wavelengths,
    const std::vector<f64>& solar_irradiance,
    f64 lambda_power, f64* k_r, f64* k_g, f64* k_b )
{
    *k_r = 0.0;
    *k_g = 0.0;
    *k_b = 0.0;
    f64 solar_r = Interpolate( wavelengths, solar_irradiance, kLambdaR );
    f64 solar_g = Interpolate( wavelengths, solar_irradiance, kLambdaG );
    f64 solar_b = Interpolate( wavelengths, solar_irradiance, kLambdaB );
    i32 dlambda = 1;
    for ( i32 lambda = kLambdaMin; lambda < kLambdaMax; lambda += dlambda ) {
        f64 x_bar = CieColorMatchingFunctionTableValue( lambda, 1 );
        f64 y_bar = CieColorMatchingFunctionTableValue( lambda, 2 );
        f64 z_bar = CieColorMatchingFunctionTableValue( lambda, 3 );
        const f64* xyz2srgb = XYZ_TO_SRGB;
        f64 r_bar =
            xyz2srgb[0] * x_bar + xyz2srgb[1] * y_bar + xyz2srgb[2] * z_bar;
        f64 g_bar =
            xyz2srgb[3] * x_bar + xyz2srgb[4] * y_bar + xyz2srgb[5] * z_bar;
        f64 b_bar =
            xyz2srgb[6] * x_bar + xyz2srgb[7] * y_bar + xyz2srgb[8] * z_bar;
        f64 irradiance = Interpolate( wavelengths, solar_irradiance, lambda );
        *k_r += r_bar * irradiance / solar_r *
            pow( lambda / kLambdaR, lambda_power );
        *k_g += g_bar * irradiance / solar_g *
            pow( lambda / kLambdaG, lambda_power );
        *k_b += b_bar * irradiance / solar_b *
            pow( lambda / kLambdaB, lambda_power );
    }
    *k_r *= MAX_LUMINOUS_EFFICACY * dlambda;
    *k_g *= MAX_LUMINOUS_EFFICACY * dlambda;
    *k_b *= MAX_LUMINOUS_EFFICACY * dlambda;
}

AtmosphereLUTComputeModule::AtmosphereLUTComputeModule()
    : transmittance( nullptr )
    , scattering{ nullptr, nullptr }
    , irradiance{ nullptr, nullptr }
    , deltaIrradiance( nullptr )
    , deltaRayleighScattering( nullptr )
    , deltaMieScattering( nullptr )
    , deltaScatteringDensity( nullptr )
{

}

AtmosphereLUTComputeModule::~AtmosphereLUTComputeModule()
{

}

void AtmosphereLUTComputeModule::destroy( RenderDevice& renderDevice )
{
    renderDevice.destroyImage( transmittance );

    for ( i32 i = 0; i < 2; i++ ) {
        renderDevice.destroyImage( scattering[i] );
        renderDevice.destroyImage( irradiance[i] );
    }

    renderDevice.destroyImage( deltaIrradiance );
    renderDevice.destroyImage( deltaRayleighScattering );
    renderDevice.destroyImage( deltaMieScattering );
    renderDevice.destroyImage( deltaScatteringDensity );
    
    transmittance = nullptr;

    for ( i32 i = 0; i < 2; i++ ) {
        scattering[i] = nullptr;
		irradiance[i] = nullptr;
    }

    deltaIrradiance = nullptr;
    deltaRayleighScattering = nullptr;
    deltaMieScattering = nullptr;
    deltaScatteringDensity = nullptr;
}

void AtmosphereLUTComputeModule::loadCachedResources( RenderDevice& renderDevice, GraphicsAssetCache& graphicsAssetCache )
{
    ImageDesc transmittanceDesc;
    transmittanceDesc.dimension = ImageDesc::DIMENSION_2D;
    transmittanceDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
    transmittanceDesc.width = TRANSMITTANCE_TEXTURE_WIDTH;
    transmittanceDesc.height = TRANSMITTANCE_TEXTURE_HEIGHT;
    transmittanceDesc.depth = 1;
    transmittanceDesc.mipCount = 1;
    transmittanceDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
    transmittanceDesc.usage = RESOURCE_USAGE_DEFAULT;

    transmittance = renderDevice.createImage( transmittanceDesc );

    ImageDesc irradianceDesc;
    irradianceDesc.dimension = ImageDesc::DIMENSION_2D;
    irradianceDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
    irradianceDesc.width = IRRADIANCE_TEXTURE_WIDTH;
    irradianceDesc.height = IRRADIANCE_TEXTURE_HEIGHT;
    irradianceDesc.depth = 1;
    irradianceDesc.mipCount = 1;
    irradianceDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
    irradianceDesc.usage = RESOURCE_USAGE_DEFAULT;

    for ( i32 i = 0; i < 2; i++ ) {
        irradiance[i] = renderDevice.createImage( irradianceDesc );
    }

    deltaIrradiance = renderDevice.createImage( irradianceDesc );

    ImageDesc scatteringDesc;
    scatteringDesc.dimension = ImageDesc::DIMENSION_3D;
    scatteringDesc.format = eViewFormat::VIEW_FORMAT_R32G32B32A32_FLOAT;
    scatteringDesc.width = SCATTERING_TEXTURE_WIDTH;
    scatteringDesc.height = SCATTERING_TEXTURE_HEIGHT;
    scatteringDesc.depth = SCATTERING_TEXTURE_DEPTH;
    scatteringDesc.mipCount = 1;
    scatteringDesc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE | RESOURCE_BIND_UNORDERED_ACCESS_VIEW;
    scatteringDesc.usage = RESOURCE_USAGE_DEFAULT;

    for ( i32 i = 0; i < 2; i++ ) {
        scattering[i] = renderDevice.createImage( scatteringDesc );
    }

    deltaRayleighScattering = renderDevice.createImage( scatteringDesc );
    deltaMieScattering = renderDevice.createImage( scatteringDesc );
    deltaScatteringDensity = renderDevice.createImage( scatteringDesc );
}

void AtmosphereLUTComputeModule::precomputePipelineResources( FrameGraph& frameGraph )
{
    // Values from "Reference Solar Spectral Irradiance: ASTM G-173", ETR column
    // (see http://rredc.nrel.gov/solar/spectra/am1.5/ASTMG173/ASTMG173.html),
    // summed and averaged in each bin (e.g. the value for 360nm is the average
    // of the ASTM G-173 values for all wavelengths between 360 and 370nm).
    // Values in W.m^-2.
    constexpr i32 kLambdaMin = 360;
    constexpr i32 kLambdaMax = 830;
    constexpr f64 kSolarIrradiance[48] = {
      1.11776, 1.14259, 1.01249, 1.14716, 1.72765, 1.73054, 1.6887, 1.61253,
      1.91198, 2.03474, 2.02042, 2.02212, 1.93377, 1.95809, 1.91686, 1.8298,
      1.8685, 1.8931, 1.85149, 1.8504, 1.8341, 1.8345, 1.8147, 1.78158, 1.7533,
      1.6965, 1.68194, 1.64654, 1.6048, 1.52143, 1.55622, 1.5113, 1.474, 1.4482,
      1.41018, 1.36775, 1.34188, 1.31429, 1.28303, 1.26758, 1.2367, 1.2082,
      1.18737, 1.14683, 1.12362, 1.1058, 1.07124, 1.04992
    };

    // Values from http://www.iup.uni-bremen.de/gruppen/molspec/databases/
    // referencespectra/o3spectra2011/index.html for 233K, summed and averaged in
    // each bin (e.g. the value for 360nm is the average of the original values
    // for all wavelengths between 360 and 370nm). Values in m^2.
    constexpr f64 kOzoneCrossSection[48] = {
      1.18e-27, 2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,
      8.451e-27, 1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26,
      1.48e-25, 1.602e-25, 2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25, 4.266e-25,
      4.672e-25, 4.398e-25, 4.701e-25, 5.019e-25, 4.305e-25, 3.74e-25, 3.215e-25,
      2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25, 1.209e-25, 9.423e-26, 7.455e-26,
      6.566e-26, 5.105e-26, 4.15e-26, 4.228e-26, 3.237e-26, 2.451e-26, 2.801e-26,
      2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27
    };

    // From https://en.wikipedia.org/wiki/Dobson_unit, in molecules.m^-2.
    constexpr f64 kDobsonUnit = 2.687e20;

    // Maximum number density of ozone molecules, in m^-3 (computed so at to get
    // 300 Dobson units of ozone - for this we divide 300 DU by the integral of
    // the ozone density profile defined below, which is equal to 15km).
    constexpr f64 kMaxOzoneNumberDensity = 300.0 * kDobsonUnit / 15000.0;

    // Wavelength independent solar irradiance "spectrum" (not physically
    // realistic, but was used in the original implementation).
    constexpr f64 kConstantSolarIrradiance = 1.5;
    constexpr f64 kBottomRadius = 6360000.0;
    constexpr f64 kTopRadius = 6420000.0;
    constexpr f64 kRayleigh = 1.24062e-6;
    constexpr f64 kRayleighScaleHeight = 8000.0;
    constexpr f64 kMieScaleHeight = 1200.0;
    constexpr f64 kMieAngstromAlpha = 0.0;
    constexpr f64 kMieAngstromBeta = 5.328e-3;
    constexpr f64 kMieSingleScatteringAlbedo = 0.9;
    constexpr f64 kMiePhaseFunctionG = 0.8;
    constexpr f64 kGroundAlbedo = 0.1;
    const f64 max_sun_zenith_angle = ( use_half_precision_ ? 102.0 : 120.0 ) / 180.0 * dk::maths::PI<f64>();

    DensityProfileLayer rayleigh_layer = { 0.0f / static_cast<f32>( kLengthUnitInMeters ), 1.0f, static_cast< f32 >( -1.0 / kRayleighScaleHeight ) * static_cast<f32>( kLengthUnitInMeters ), 0.0f * static_cast<f32>( kLengthUnitInMeters ), dkVec3f(0, 0, 0), 0.0f };
    DensityProfileLayer mie_layer = { 0.0f / static_cast<f32>( kLengthUnitInMeters ), 1.0f, static_cast< f32 >( -1.0 / kMieScaleHeight ) * static_cast<f32>( kLengthUnitInMeters ), 0.0f * static_cast<f32>( kLengthUnitInMeters ), dkVec3f( 0, 0, 0 ), 0.0f };

    // Density profile increasing linearly from 0 to 1 between 10 and 25km, and
    // decreasing linearly from 1 to 0 between 25 and 40km. This is an approximate
    // profile from http://www.kln.ac.lk/science/Chemistry/Teaching_Resources/
    // Documents/Introduction%20to%20atmospheric%20chemistry.pdf (page 10).
    std::vector<DensityProfileLayer> ozone_density;
    ozone_density.push_back( { 25000.0f / static_cast<f32>( kLengthUnitInMeters ), 0.0f, 0.0f * static_cast<f32>( kLengthUnitInMeters ), static_cast<f32>( 1.0 / 15000.0 ) * static_cast<f32>( kLengthUnitInMeters ), dkVec3f( 0, 0, 0 ), static_cast<f32>( -2.0 / 3.0 ) } );
    ozone_density.push_back( { 0.0f / static_cast<f32>( kLengthUnitInMeters ), 0.0f, 0.0f * static_cast<f32>( kLengthUnitInMeters ), static_cast<f32>( -1.0 / 15000.0 ) * static_cast<f32>( kLengthUnitInMeters ), dkVec3f( 0, 0, 0 ), static_cast<f32>( 8.0 / 3.0 ) } );

    std::vector<f64> wavelengths;
    std::vector<f64> solar_irradiance;
    std::vector<f64> rayleigh_scattering;
    std::vector<f64> mie_scattering;
    std::vector<f64> mie_extinction;
    std::vector<f64> absorption_extinction;
    std::vector<f64> ground_albedo;
    for ( i32 l = kLambdaMin; l <= kLambdaMax; l += 10 ) {
        f64 lambda = static_cast< f64 >( l ) * 1e-3;  // micro-meters
        f64 mie =
            kMieAngstromBeta / kMieScaleHeight * pow( lambda, -kMieAngstromAlpha );
        wavelengths.push_back( l );
        if ( use_constant_solar_spectrum_ ) {
            solar_irradiance.push_back( kConstantSolarIrradiance );
        } else {
            solar_irradiance.push_back( kSolarIrradiance[( l - kLambdaMin ) / 10] );
        }
        rayleigh_scattering.push_back( kRayleigh * pow( lambda, -4 ) );
        mie_scattering.push_back( mie * kMieSingleScatteringAlbedo );
        mie_extinction.push_back( mie );
        absorption_extinction.push_back( use_ozone_ ?
                                         kMaxOzoneNumberDensity * kOzoneCrossSection[( l - kLambdaMin ) / 10] :
                                         0.0 );
        ground_albedo.push_back( kGroundAlbedo );
    }
    const f64 sun_angular_radius = kSunAngularRadius;

    f64 bottom_radius = kBottomRadius;
    f64 top_radius = kTopRadius;
    const std::vector<DensityProfileLayer>& rayleigh_density = { rayleigh_layer };
    const std::vector<DensityProfileLayer>& mie_density = { mie_layer };
    f64 mie_phase_function_g = kMiePhaseFunctionG;
    const std::vector<DensityProfileLayer>& absorption_density = ozone_density;
    f64 length_unit_in_meters = kLengthUnitInMeters;
    u32 num_precomputed_wavelengths = use_luminance_ == atmosphereLuminance_t::ATMOSPHERE_LUMINANCE_PRECOMPUTED ? 15 : 3;
    bool combine_scattering_textures = use_combined_textures_;
    bool half_precision = use_half_precision_;

    auto InterpolateWavelengths = [&wavelengths]( const std::vector<f64>& v,
                                     const dkVec3f& lambdas, f64 scale ) {
        f64 r = Interpolate( wavelengths, v, lambdas[0] ) * scale;
        f64 g = Interpolate( wavelengths, v, lambdas[1] ) * scale;
        f64 b = Interpolate( wavelengths, v, lambdas[2] ) * scale;
        return dkVec3f( static_cast< f32 >( r ), static_cast< f32 >( g ), static_cast< f32 >( b ) );
    };

    // Compute the values for the SKY_RADIANCE_TO_LUMINANCE constant. In theory
    // this should be 1 in precomputed illuminance mode (because the precomputed
    // textures already contain illuminance values). In practice, however, storing
    // true illuminance values in half precision textures yields artefacts
    // (because the values are too large), so we store illuminance values divided
    // by MAX_LUMINOUS_EFFICACY instead. This is why, in precomputed illuminance
    // mode, we set SKY_RADIANCE_TO_LUMINANCE to MAX_LUMINOUS_EFFICACY.
    bool precompute_illuminance = num_precomputed_wavelengths > 3;
    f64 sky_k_r, sky_k_g, sky_k_b;
    if ( precompute_illuminance ) {
        sky_k_r = sky_k_g = sky_k_b = MAX_LUMINOUS_EFFICACY;
    } else {
        ComputeSpectralRadianceToLuminanceFactors( wavelengths, solar_irradiance,
                                                   -3 /* lambda_power */, &sky_k_r, &sky_k_g, &sky_k_b );
    }
    // Compute the values for the SUN_RADIANCE_TO_LUMINANCE constant.
    f64 sun_k_r, sun_k_g, sun_k_b;
    ComputeSpectralRadianceToLuminanceFactors( wavelengths, solar_irradiance,
                                               0 /* lambda_power */, &sun_k_r, &sun_k_g, &sun_k_b );

     SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = dkVec3f(
        static_cast< f32 >( sky_k_r ),
        static_cast< f32 >( sky_k_g ),
        static_cast< f32 >( sky_k_b )
    );

    SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = dkVec3f(
        static_cast< f32 >( sun_k_r ),
        static_cast< f32 >( sun_k_g ),
        static_cast< f32 >( sun_k_b )
    );

    // The actual precomputations depend on whether we want to store precomputed
    // irradiance or illuminance values.
    if ( num_precomputed_wavelengths <= 3 ) {
        dkVec3f lambdas = dkVec3f( kLambdaR, kLambdaG, kLambdaB );

        properties.LuminanceFromRadianceX = dkVec4f( 1, 0, 0, 0 );
        properties.LuminanceFromRadianceY = dkVec4f( 0, 1, 0, 0 );
        properties.LuminanceFromRadianceZ = dkVec4f( 0, 0, 1, 0 );

        DensityProfileLayer DummyLayer = { 0.000000f,0.000000f,0.000000f,0.000000f, dkVec3f::Zero, 0.000000f };
		properties.AtmosphereParams = {
            { DummyLayer, rayleigh_density[0] },
            { DummyLayer, mie_density[0] },
			{ absorption_density[0], absorption_density[1] },

			InterpolateWavelengths( solar_irradiance, lambdas, 1.0 ),
            static_cast< f32 >( mie_phase_function_g ),

			InterpolateWavelengths( rayleigh_scattering, lambdas, length_unit_in_meters ),
			static_cast< f32 >( sun_angular_radius ),

			InterpolateWavelengths( mie_scattering, lambdas, length_unit_in_meters ),
			static_cast< f32 >( top_radius / length_unit_in_meters ),

            InterpolateWavelengths( mie_extinction, lambdas, length_unit_in_meters ),
            0,

            InterpolateWavelengths( absorption_extinction, lambdas, length_unit_in_meters ),
            static_cast< f32 >( bottom_radius / length_unit_in_meters ),

            InterpolateWavelengths( ground_albedo, lambdas, 1.0 ),
            static_cast< f32 >( cos( max_sun_zenith_angle ) )
        };

        precomputeIteration( frameGraph, lambdas, num_scattering_orders, false );
    } else {
        constexpr f64 kLambdaMin = 360.0;
        constexpr f64 kLambdaMax = 830.0;
        i32 num_iterations = ( num_precomputed_wavelengths + 2 ) / 3;
        f64 dlambda = ( kLambdaMax - kLambdaMin ) / ( 3 * num_iterations );
        for ( i32 i = 0; i < num_iterations; ++i ) {
            dkVec3f lambdas = dkVec3f(
                static_cast< f32 >( kLambdaMin + ( 3 * i + 0.5 ) * dlambda ),
                static_cast< f32 >( kLambdaMin + ( 3 * i + 1.5 ) * dlambda ),
                static_cast< f32 >( kLambdaMin + ( 3 * i + 2.5 ) * dlambda )
            );

            auto coeff = [dlambda]( f64 lambda, i32 component ) {
                // Note that we don't include MAX_LUMINOUS_EFFICACY here, to avoid
                // artefacts due to too large values when using half precision on GPU.
                // We add this term back in kAtmosphereShader, via
                // SKY_SPECTRAL_RADIANCE_TO_LUMINANCE (see also the comments in the
                // Model constructor).
                f64 x = CieColorMatchingFunctionTableValue( lambda, 1 );
                f64 y = CieColorMatchingFunctionTableValue( lambda, 2 );
                f64 z = CieColorMatchingFunctionTableValue( lambda, 3 );
                return static_cast< f32 >( (
                    XYZ_TO_SRGB[component * 3] * x +
                    XYZ_TO_SRGB[component * 3 + 1] * y +
                    XYZ_TO_SRGB[component * 3 + 2] * z ) * dlambda );
            };

            properties.LuminanceFromRadianceX = dkVec4f( coeff( lambdas[0], 0 ), coeff( lambdas[1], 0 ), coeff( lambdas[2], 0 ), 0 );
            properties.LuminanceFromRadianceY = dkVec4f( coeff( lambdas[0], 1 ), coeff( lambdas[1], 1 ), coeff( lambdas[2], 1 ), 0 );
            properties.LuminanceFromRadianceZ = dkVec4f( coeff( lambdas[0], 2 ), coeff( lambdas[1], 2 ), coeff( lambdas[2], 2 ), 0 );

			properties.AtmosphereParams = {
				{ rayleigh_density[0], rayleigh_density[1] },
				{ mie_density[0], mie_density[1] },
				{ absorption_density[0], absorption_density[1] },

				InterpolateWavelengths( solar_irradiance, lambdas, 1.0 ),
				static_cast< f32 >( mie_phase_function_g ),

				InterpolateWavelengths( rayleigh_scattering, lambdas, length_unit_in_meters ),
				static_cast< f32 >( sun_angular_radius ),

				InterpolateWavelengths( mie_scattering, lambdas, length_unit_in_meters ),
				static_cast< f32 >( top_radius / length_unit_in_meters ),

				InterpolateWavelengths( mie_extinction, lambdas, length_unit_in_meters ),
				0,

				InterpolateWavelengths( absorption_extinction, lambdas, length_unit_in_meters ),
				static_cast< f32 >( bottom_radius / length_unit_in_meters ),

				InterpolateWavelengths( ground_albedo, lambdas, 1.0 ),
				static_cast< f32 >( cos( max_sun_zenith_angle ) )
			};

            precomputeIteration( frameGraph, lambdas, num_scattering_orders, i > 0 );
        }
    }
}

void AtmosphereLUTComputeModule::precomputeIteration( FrameGraph& frameGraph, const dkVec3f& lambdas, const u32 num_scattering_orders, const bool enableBlending )
{
    struct PassData {
        ResHandle_t PerPassBuffer;
    };

	// Compute the transmittance, and store it in transmittance.
    frameGraph.addRenderPass<PassData>(
        AtmosphereLUTCompute::ComputeTransmittance_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            BufferDesc passBufferDesc;
            passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( RenderingHelpers::PS_Compute, AtmosphereLUTCompute::ComputeTransmittance_ShaderBinding );
            cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeTransmittance_EventName );

            cmdList->bindPipelineState( pipelineState );

            cmdList->bindImage( AtmosphereLUTCompute::ComputeTransmittance_OutputRenderTarget_Hashcode, transmittance );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
            parameters.AtmosphereParams = properties.AtmosphereParams;
            parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
            parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
            parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
            parameters.ScatteringOrder = 0;

            cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties ) );

            cmdList->prepareAndBindResourceList( pipelineState );

            constexpr u32 ThreadCountX = TRANSMITTANCE_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeTransmittance_DispatchX;
            constexpr u32 ThreadCountY = TRANSMITTANCE_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeTransmittance_DispatchY;

            cmdList->dispatchCompute( ThreadCountX, ThreadCountY, 1u );

            cmdList->popEventMarker();
        }
    );

	// Compute the direct irradiance, store it in delta_irradiance_texture and,
	// depending on 'blend', either initialize irradiance_texture_ with zeros or
	// leave it unchanged (we don't want the direct irradiance in
	// irradiance_texture_, but only the irradiance from the sky).
    frameGraph.addRenderPass<PassData>(
        AtmosphereLUTCompute::ComputeDirectIrradiance_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            BufferDesc passBufferDesc;
            passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
           Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            constexpr PipelineStateDesc DirectIrradiancePipelineDesc = PipelineStateDesc(
                PipelineStateDesc::COMPUTE,
                PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                DepthStencilStateDesc(),
                RasterizerStateDesc(),
                BlendStateDesc(),
                FramebufferLayoutDesc(),
                { { RenderingHelpers::S_BilinearClampEdge }, 1 }
            );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( DirectIrradiancePipelineDesc, AtmosphereLUTCompute::ComputeDirectIrradiance_ShaderBinding );
            cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeDirectIrradiance_EventName );

            cmdList->bindPipelineState( pipelineState );

            cmdList->bindImage( AtmosphereLUTCompute::ComputeDirectIrradiance_TransmittanceComputedTexture_Hashcode, transmittance );
            cmdList->bindImage( AtmosphereLUTCompute::ComputeDirectIrradiance_OutputRenderTarget_Hashcode, deltaIrradiance );
            cmdList->bindImage( AtmosphereLUTCompute::ComputeDirectIrradiance_OutputRenderTarget2_Hashcode, irradiance[0] );
            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
            parameters.AtmosphereParams = properties.AtmosphereParams;
            parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
            parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
            parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
            parameters.ScatteringOrder = 0;

            cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties ) );

            cmdList->prepareAndBindResourceList( pipelineState );

            constexpr u32 ThreadCountX = IRRADIANCE_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeDirectIrradiance_DispatchX;
            constexpr u32 ThreadCountY = IRRADIANCE_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeDirectIrradiance_DispatchY;

            cmdList->dispatchCompute( ThreadCountX, ThreadCountY, 1u );

            cmdList->popEventMarker();
        }
    );

	// Compute the rayleigh and mie single scattering, store them in
	// delta_rayleigh_scattering_texture and delta_mie_scattering_texture, and
	// either store them or accumulate them in scattering_texture_ and
	// optional_single_mie_scattering_texture_.
    frameGraph.addRenderPass<PassData>(
        AtmosphereLUTCompute::ComputeSingleScatteringPass_Name,
        [&]( FrameGraphBuilder& builder, PassData& passData ) {
            builder.useAsyncCompute();

            BufferDesc passBufferDesc;
            passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
            passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
            passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeSingleScatteringPassRuntimeProperties );

            passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
        },
        [=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
            Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

            constexpr PipelineStateDesc ComputeSingleScatteringPipelineDesc = PipelineStateDesc(
                PipelineStateDesc::COMPUTE,
                PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                DepthStencilStateDesc(),
                RasterizerStateDesc(),
                BlendStateDesc(),
                FramebufferLayoutDesc(),
                { { RenderingHelpers::S_BilinearClampEdge }, 1 }
            );

            PipelineState* pipelineState = psoCache->getOrCreatePipelineState( ComputeSingleScatteringPipelineDesc, AtmosphereLUTCompute::ComputeSingleScatteringPass_ShaderBinding );
            cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeSingleScatteringPass_EventName );

            cmdList->bindPipelineState( pipelineState );

            cmdList->bindImage( AtmosphereLUTCompute::ComputeSingleScatteringPass_TransmittanceComputedTexture_Hashcode, transmittance );
            cmdList->bindImage( AtmosphereLUTCompute::ComputeSingleScatteringPass_DeltaRayleigh_Hashcode, deltaRayleighScattering );
            cmdList->bindImage( AtmosphereLUTCompute::ComputeSingleScatteringPass_DeltaMie_Hashcode, deltaMieScattering );
            cmdList->bindImage( AtmosphereLUTCompute::ComputeSingleScatteringPass_Scattering_Hashcode, scattering[0] );

            cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

            AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
            parameters.AtmosphereParams = properties.AtmosphereParams;
            parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
            parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
            parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
            parameters.ScatteringOrder = 0;

            cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeTransmittanceRuntimeProperties ) );

            cmdList->prepareAndBindResourceList( pipelineState );

            constexpr u32 ThreadCountX = SCATTERING_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeSingleScatteringPass_DispatchX;
            constexpr u32 ThreadCountY = SCATTERING_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeSingleScatteringPass_DispatchY;
            constexpr u32 ThreadCountZ = SCATTERING_TEXTURE_DEPTH / AtmosphereLUTCompute::ComputeSingleScatteringPass_DispatchZ;

            cmdList->dispatchCompute( ThreadCountX, ThreadCountY, ThreadCountZ );

            cmdList->popEventMarker();
        }
    );

    for ( u32 scattering_order = 2u; scattering_order <= num_scattering_orders; ++scattering_order ) {
		// Compute the scattering density, and store it in
		// delta_scattering_density_texture.
        frameGraph.addRenderPass<PassData>(
            AtmosphereLUTCompute::ComputeScatteringDensity_Name,
            [&]( FrameGraphBuilder& builder, PassData& passData ) {
                builder.useAsyncCompute();

                BufferDesc passBufferDesc;
                passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
                passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
                passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeScatteringDensityRuntimeProperties );

                passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
            },
			[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
				Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

                constexpr PipelineStateDesc ComputeScatteringDensityPipelineDesc = PipelineStateDesc(
                    PipelineStateDesc::COMPUTE,
                    PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                    DepthStencilStateDesc(),
                    RasterizerStateDesc(),
                    BlendStateDesc(),
                    FramebufferLayoutDesc(),
                    { { RenderingHelpers::S_BilinearClampEdge }, 1 }
                );

                PipelineState* pipelineState = psoCache->getOrCreatePipelineState( ComputeScatteringDensityPipelineDesc, AtmosphereLUTCompute::ComputeScatteringDensity_ShaderBinding );
                cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeScatteringDensity_EventName );

                cmdList->bindPipelineState( pipelineState );

                cmdList->bindImage( AtmosphereLUTCompute::ComputeScatteringDensity_TransmittanceComputedTexture_Hashcode, transmittance );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeScatteringDensity_SingleRayleighScatteringTexture_Hashcode, deltaRayleighScattering );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeScatteringDensity_IrradianceTextureInput_Hashcode, deltaIrradiance );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeScatteringDensity_SingleMieScatteringTexture_Hashcode, deltaMieScattering );

                cmdList->bindImage( AtmosphereLUTCompute::ComputeScatteringDensity_ScatteringDensity_Hashcode, deltaScatteringDensity );

                cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
                parameters.AtmosphereParams = properties.AtmosphereParams;
                parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
                parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
                parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
                parameters.ScatteringOrder = scattering_order;

                cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeScatteringDensityRuntimeProperties ) );

                cmdList->prepareAndBindResourceList( pipelineState );

                constexpr u32 ThreadCountX = SCATTERING_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeScatteringDensity_DispatchX;
                constexpr u32 ThreadCountY = SCATTERING_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeScatteringDensity_DispatchY;
                constexpr u32 ThreadCountZ = SCATTERING_TEXTURE_DEPTH / AtmosphereLUTCompute::ComputeScatteringDensity_DispatchZ;

                cmdList->dispatchCompute( ThreadCountX, ThreadCountY, ThreadCountZ );

                cmdList->popEventMarker();
            }
        );

		// Compute the indirect irradiance, store it in delta_irradiance_texture and
		// accumulate it in irradiance_texture_.
		frameGraph.addRenderPass<PassData>(
			AtmosphereLUTCompute::ComputeIndirectIrradiance_Name,
			[&]( FrameGraphBuilder& builder, PassData& passData ) {
			    builder.useAsyncCompute();

			    BufferDesc passBufferDesc;
			    passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			    passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			    passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeIndirectIrradianceRuntimeProperties );

			    passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
		    },
			[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
				i32 irradianceWriteIndex = ( scattering_order % 2 == 0 ) ? 1 : 0;
                i32 irradianceReadIndex = ( irradianceWriteIndex == 0 ) ? 0 : 1;
                
			    Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

			    constexpr PipelineStateDesc ComputeScatteringDensityPipelineDesc = PipelineStateDesc(
				    PipelineStateDesc::COMPUTE,
				    PRIMITIVE_TOPOLOGY_TRIANGLELIST,
				    DepthStencilStateDesc(),
				    RasterizerStateDesc(),
				    BlendStateDesc(),
				    FramebufferLayoutDesc(),
				    { { RenderingHelpers::S_BilinearClampEdge }, 1 }
			    );

			    PipelineState* pipelineState = psoCache->getOrCreatePipelineState( ComputeScatteringDensityPipelineDesc, AtmosphereLUTCompute::ComputeIndirectIrradiance_ShaderBinding );
			    cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeIndirectIrradiance_EventName );

			    cmdList->bindPipelineState( pipelineState );

				cmdList->bindImage( AtmosphereLUTCompute::ComputeIndirectIrradiance_IrradianceTextureInput_Hashcode, irradiance[irradianceReadIndex] );
			    cmdList->bindImage( AtmosphereLUTCompute::ComputeIndirectIrradiance_SingleRayleighScatteringTexture_Hashcode, deltaRayleighScattering );
				cmdList->bindImage( AtmosphereLUTCompute::ComputeIndirectIrradiance_SingleMieScatteringTexture_Hashcode, deltaMieScattering );

				cmdList->bindImage( AtmosphereLUTCompute::ComputeIndirectIrradiance_DeltaIrradiance_Hashcode, deltaIrradiance );
				cmdList->bindImage( AtmosphereLUTCompute::ComputeIndirectIrradiance_OutputRenderTarget_Hashcode, irradiance[irradianceWriteIndex] );
                
			    cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
                parameters.AtmosphereParams = properties.AtmosphereParams;
                parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
                parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
                parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
                parameters.ScatteringOrder = scattering_order - 1;

			    cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeIndirectIrradianceRuntimeProperties ) );

			    cmdList->prepareAndBindResourceList( pipelineState );

			    constexpr u32 ThreadCountX = IRRADIANCE_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeIndirectIrradiance_DispatchX;
			    constexpr u32 ThreadCountY = IRRADIANCE_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeIndirectIrradiance_DispatchY;
			    constexpr u32 ThreadCountZ = 1u;

			    cmdList->dispatchCompute( ThreadCountX, ThreadCountY, ThreadCountZ );

			    cmdList->popEventMarker();
		    }
		);


        // Compute the multiple scattering, store it in
        // delta_multiple_scattering_texture, and accumulate it in
        // scattering_texture_.
        // 
        frameGraph.addRenderPass<PassData>(
			AtmosphereLUTCompute::ComputeMultipleScattering_Name,
			[&]( FrameGraphBuilder& builder, PassData& passData ) {
			    builder.useAsyncCompute();

			    BufferDesc passBufferDesc;
			    passBufferDesc.BindFlags = RESOURCE_BIND_CONSTANT_BUFFER;
			    passBufferDesc.Usage = RESOURCE_USAGE_DYNAMIC;
			    passBufferDesc.SizeInBytes = sizeof( AtmosphereLUTCompute::ComputeMultipleScatteringRuntimeProperties );

			    passData.PerPassBuffer = builder.allocateBuffer( passBufferDesc, eShaderStage::SHADER_STAGE_COMPUTE );
		    },
			[=]( const PassData& passData, const FrameGraphResources* resources, CommandList* cmdList, PipelineStateCache* psoCache ) {
				i32 irradianceWriteIndex = ( scattering_order % 2 == 0 ) ? 1 : 0;
                i32 irradianceReadIndex = ( irradianceWriteIndex == 0 ) ? 0 : 1;
                
			    Buffer* perPassBuffer = resources->getBuffer( passData.PerPassBuffer );

			    constexpr PipelineStateDesc ComputeScatteringDensityPipelineDesc = PipelineStateDesc(
				    PipelineStateDesc::COMPUTE,
				    PRIMITIVE_TOPOLOGY_TRIANGLELIST,
				    DepthStencilStateDesc(),
				    RasterizerStateDesc(),
				    BlendStateDesc(),
				    FramebufferLayoutDesc(),
				    { { RenderingHelpers::S_BilinearClampEdge }, 1 }
			    );

			    PipelineState* pipelineState = psoCache->getOrCreatePipelineState( ComputeScatteringDensityPipelineDesc, AtmosphereLUTCompute::ComputeMultipleScattering_ShaderBinding );
			    cmdList->pushEventMarker( AtmosphereLUTCompute::ComputeMultipleScattering_EventName );

			    cmdList->bindPipelineState( pipelineState );

                cmdList->bindImage( AtmosphereLUTCompute::ComputeMultipleScattering_TransmittanceComputedTexture_Hashcode, transmittance );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeMultipleScattering_ScatteringTextureInput_Hashcode, deltaScatteringDensity );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeMultipleScattering_DeltaRayleigh_Hashcode, deltaRayleighScattering );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeMultipleScattering_Scattering_Hashcode, scattering[irradianceWriteIndex] );
                cmdList->bindImage( AtmosphereLUTCompute::ComputeMultipleScattering_ScatteringAccumulate_Hashcode, scattering[irradianceReadIndex] );

			    cmdList->bindConstantBuffer( PerPassBufferHashcode, perPassBuffer );

                AtmosphereLUTCompute::ComputeDirectIrradianceRuntimeProperties parameters;
                parameters.AtmosphereParams = properties.AtmosphereParams;
                parameters.LuminanceFromRadianceX = properties.LuminanceFromRadianceX;
                parameters.LuminanceFromRadianceY = properties.LuminanceFromRadianceY;
                parameters.LuminanceFromRadianceZ = properties.LuminanceFromRadianceZ;
                parameters.ScatteringOrder = scattering_order - 1;

			    cmdList->updateBuffer( *perPassBuffer, &parameters, sizeof( AtmosphereLUTCompute::ComputeMultipleScatteringRuntimeProperties ) );

			    cmdList->prepareAndBindResourceList( pipelineState );

			    constexpr u32 ThreadCountX = SCATTERING_TEXTURE_WIDTH / AtmosphereLUTCompute::ComputeMultipleScattering_DispatchX;
			    constexpr u32 ThreadCountY = SCATTERING_TEXTURE_HEIGHT / AtmosphereLUTCompute::ComputeMultipleScattering_DispatchY;
			    constexpr u32 ThreadCountZ = SCATTERING_TEXTURE_DEPTH / AtmosphereLUTCompute::ComputeMultipleScattering_DispatchZ;

			    cmdList->dispatchCompute( ThreadCountX, ThreadCountY, ThreadCountZ );

			    cmdList->popEventMarker();
		    }
		);
    }
}
