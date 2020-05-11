/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>
#include "DirectDrawSurface.h"

#include <FileSystem/FileSystemObject.h>

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((u32)(uint8_t)(ch0) | ((u32)(uint8_t)(ch1) << 8) |       \
                ((u32)(uint8_t)(ch2) << 16) | ((u32)(uint8_t)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//
// See DDS.h in the 'Texconv' sample and the 'DirectXTex' library
//--------------------------------------------------------------------------------------
#pragma pack(push,1)

static constexpr u32 DDS_MAGIC = 0x20534444; // "DDS "

                                                  // Copy/Pasted from msdn for better abstraction
struct DDS_PIXELFORMAT
{
    u32 dwSize;
    u32 dwFlags;
    u32 dwFourCC;
    u32 dwRGBBitCount;
    u32 dwRBitMask;
    u32 dwGBitMask;
    u32 dwBBitMask;
    u32 dwABitMask;
};

typedef struct
{
    u32           dwSize;
    u32           dwFlags;
    u32           dwHeight;
    u32           dwWidth;
    u32           dwPitchOrLinearSize;
    u32           dwDepth;
    u32           dwMipMapCount;
    u32           dwReserved1[11];
    DDS_PIXELFORMAT    ddspf;
    u32           dwCaps;
    u32           dwCaps2;
    u32           dwCaps3;
    u32           dwCaps4;
    u32           dwReserved2;
} DDS_HEADER;

typedef enum __D3D10_RESOURCE_DIMENSION
{
    _D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
    _D3D10_RESOURCE_DIMENSION_BUFFER = 1,
    _D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    _D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    _D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
} __D3D10_RESOURCE_DIMENSION;

typedef enum _DXGI_FORMAT
{
    DXGI_DIMENSION_UNKNOWN = 0,
    DXGI_DIMENSION_R32G32B32A32_TYPELESS = 1,
    DXGI_DIMENSION_R32G32B32A32_FLOAT = 2,
    DXGI_DIMENSION_R32G32B32A32_UINT = 3,
    DXGI_DIMENSION_R32G32B32A32_SINT = 4,
    DXGI_DIMENSION_R32G32B32_TYPELESS = 5,
    DXGI_DIMENSION_R32G32B32_FLOAT = 6,
    DXGI_DIMENSION_R32G32B32_UINT = 7,
    DXGI_DIMENSION_R32G32B32_SINT = 8,
    DXGI_DIMENSION_R16G16B16A16_TYPELESS = 9,
    DXGI_DIMENSION_R16G16B16A16_FLOAT = 10,
    DXGI_DIMENSION_R16G16B16A16_UNORM = 11,
    DXGI_DIMENSION_R16G16B16A16_UINT = 12,
    DXGI_DIMENSION_R16G16B16A16_SNORM = 13,
    DXGI_DIMENSION_R16G16B16A16_SINT = 14,
    DXGI_DIMENSION_R32G32_TYPELESS = 15,
    DXGI_DIMENSION_R32G32_FLOAT = 16,
    DXGI_DIMENSION_R32G32_UINT = 17,
    DXGI_DIMENSION_R32G32_SINT = 18,
    DXGI_DIMENSION_R32G8X24_TYPELESS = 19,
    DXGI_DIMENSION_D32_FLOAT_S8X24_UINT = 20,
    DXGI_DIMENSION_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_DIMENSION_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_DIMENSION_R10G10B10A2_TYPELESS = 23,
    DXGI_DIMENSION_R10G10B10A2_UNORM = 24,
    DXGI_DIMENSION_R10G10B10A2_UINT = 25,
    DXGI_DIMENSION_R11G11B10_FLOAT = 26,
    DXGI_DIMENSION_R8G8B8A8_TYPELESS = 27,
    DXGI_DIMENSION_R8G8B8A8_UNORM = 28,
    DXGI_DIMENSION_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_DIMENSION_R8G8B8A8_UINT = 30,
    DXGI_DIMENSION_R8G8B8A8_SNORM = 31,
    DXGI_DIMENSION_R8G8B8A8_SINT = 32,
    DXGI_DIMENSION_R16G16_TYPELESS = 33,
    DXGI_DIMENSION_R16G16_FLOAT = 34,
    DXGI_DIMENSION_R16G16_UNORM = 35,
    DXGI_DIMENSION_R16G16_UINT = 36,
    DXGI_DIMENSION_R16G16_SNORM = 37,
    DXGI_DIMENSION_R16G16_SINT = 38,
    DXGI_DIMENSION_R32_TYPELESS = 39,
    DXGI_DIMENSION_D32_FLOAT = 40,
    DXGI_DIMENSION_R32_FLOAT = 41,
    DXGI_DIMENSION_R32_UINT = 42,
    DXGI_DIMENSION_R32_SINT = 43,
    DXGI_DIMENSION_R24G8_TYPELESS = 44,
    DXGI_DIMENSION_D24_UNORM_S8_UINT = 45,
    DXGI_DIMENSION_R24_UNORM_X8_TYPELESS = 46,
    DXGI_DIMENSION_X24_TYPELESS_G8_UINT = 47,
    DXGI_DIMENSION_R8G8_TYPELESS = 48,
    DXGI_DIMENSION_R8G8_UNORM = 49,
    DXGI_DIMENSION_R8G8_UINT = 50,
    DXGI_DIMENSION_R8G8_SNORM = 51,
    DXGI_DIMENSION_R8G8_SINT = 52,
    DXGI_DIMENSION_R16_TYPELESS = 53,
    DXGI_DIMENSION_R16_FLOAT = 54,
    DXGI_DIMENSION_D16_UNORM = 55,
    DXGI_DIMENSION_R16_UNORM = 56,
    DXGI_DIMENSION_R16_UINT = 57,
    DXGI_DIMENSION_R16_SNORM = 58,
    DXGI_DIMENSION_R16_SINT = 59,
    DXGI_DIMENSION_R8_TYPELESS = 60,
    DXGI_DIMENSION_R8_UNORM = 61,
    DXGI_DIMENSION_R8_UINT = 62,
    DXGI_DIMENSION_R8_SNORM = 63,
    DXGI_DIMENSION_R8_SINT = 64,
    DXGI_DIMENSION_A8_UNORM = 65,
    DXGI_DIMENSION_R1_UNORM = 66,
    DXGI_DIMENSION_R9G9B9E5_SHAREDEXP = 67,
    DXGI_DIMENSION_R8G8_B8G8_UNORM = 68,
    DXGI_DIMENSION_G8R8_G8B8_UNORM = 69,
    DXGI_DIMENSION_BC1_TYPELESS = 70,
    DXGI_DIMENSION_BC1_UNORM = 71,
    DXGI_DIMENSION_BC1_UNORM_SRGB = 72,
    DXGI_DIMENSION_BC2_TYPELESS = 73,
    DXGI_DIMENSION_BC2_UNORM = 74,
    DXGI_DIMENSION_BC2_UNORM_SRGB = 75,
    DXGI_DIMENSION_BC3_TYPELESS = 76,
    DXGI_DIMENSION_BC3_UNORM = 77,
    DXGI_DIMENSION_BC3_UNORM_SRGB = 78,
    DXGI_DIMENSION_BC4_TYPELESS = 79,
    DXGI_DIMENSION_BC4_UNORM = 80,
    DXGI_DIMENSION_BC4_SNORM = 81,
    DXGI_DIMENSION_BC5_TYPELESS = 82,
    DXGI_DIMENSION_BC5_UNORM = 83,
    DXGI_DIMENSION_BC5_SNORM = 84,
    DXGI_DIMENSION_B5G6R5_UNORM = 85,
    DXGI_DIMENSION_B5G5R5A1_UNORM = 86,
    DXGI_DIMENSION_B8G8R8A8_UNORM = 87,
    DXGI_DIMENSION_B8G8R8X8_UNORM = 88,
    DXGI_DIMENSION_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_DIMENSION_B8G8R8A8_TYPELESS = 90,
    DXGI_DIMENSION_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_DIMENSION_B8G8R8X8_TYPELESS = 92,
    DXGI_DIMENSION_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_DIMENSION_BC6H_TYPELESS = 94,
    DXGI_DIMENSION_BC6H_UF16 = 95,
    DXGI_DIMENSION_BC6H_SF16 = 96,
    DXGI_DIMENSION_BC7_TYPELESS = 97,
    DXGI_DIMENSION_BC7_UNORM = 98,
    DXGI_DIMENSION_BC7_UNORM_SRGB = 99,
    DXGI_DIMENSION_AYUV = 100,
    DXGI_DIMENSION_Y410 = 101,
    DXGI_DIMENSION_Y416 = 102,
    DXGI_DIMENSION_NV12 = 103,
    DXGI_DIMENSION_P010 = 104,
    DXGI_DIMENSION_P016 = 105,
    DXGI_DIMENSION_420_OPAQUE = 106,
    DXGI_DIMENSION_YUY2 = 107,
    DXGI_DIMENSION_Y210 = 108,
    DXGI_DIMENSION_Y216 = 109,
    DXGI_DIMENSION_NV11 = 110,
    DXGI_DIMENSION_AI44 = 111,
    DXGI_DIMENSION_IA44 = 112,
    DXGI_DIMENSION_P8 = 113,
    DXGI_DIMENSION_A8P8 = 114,
    DXGI_DIMENSION_B4G4R4A4_UNORM = 115,
    DXGI_DIMENSION_P208 = 130,
    DXGI_DIMENSION_V208 = 131,
    DXGI_DIMENSION_V408 = 132,
    DXGI_DIMENSION_FORCE_UINT = 0xffffffff
} _DXGI_FORMAT;

typedef struct
{
    _DXGI_FORMAT              dxgiFormat;
    __D3D10_RESOURCE_DIMENSION resourceDimension;
    u32                     miscFlag;
    u32                     arraySize;
    u32                     miscFlags2;
} DDS_HEADER_DXT10;

typedef enum __D3D11_RESOURCE_MISC_FLAG
{
    _D3D11_RESOURCE_MISC_GENERATE_MIPS = 0x1L,
    _D3D11_RESOURCE_MISC_SHARED = 0x2L,
    _D3D11_RESOURCE_MISC_TEXTURECUBE = 0x4L,
    _D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS = 0x10L,
    _D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS = 0x20L,
    _D3D11_RESOURCE_MISC_BUFFER_STRUCTURED = 0x40L,
    _D3D11_RESOURCE_MISC_RESOURCE_CLAMP = 0x80L,
    _D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX = 0x100L,
    _D3D11_RESOURCE_MISC_GDI_COMPATIBLE = 0x200L,
    _D3D11_RESOURCE_MISC_SHARED_NTHANDLE = 0x800L,
    _D3D11_RESOURCE_MISC_RESTRICTED_CONTENT = 0x1000L,
    _D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE = 0x2000L,
    _D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER = 0x4000L,
    _D3D11_RESOURCE_MISC_GUARDED = 0x8000L,
    _D3D11_RESOURCE_MISC_TILE_POOL = 0x20000L,
    _D3D11_RESOURCE_MISC_TILED = 0x40000L,
    _D3D11_RESOURCE_MISC_HW_PROTECTED = 0x80000L
} __D3D11_RESOURCE_MISC_FLAG;

#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
#define DDS_RGB         0x00000040  // DDPF_RGB
#define DDS_LUMINANCE   0x00020000  // DDPF_LUMINANCE
#define DDS_ALPHA       0x00000002  // DDPF_ALPHA
#define DDS_BUMPDUDV    0x00080000  // DDPF_BUMPDUDV

#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH

#define DDS_HEIGHT 0x00000002 // DDSD_HEIGHT
#define DDS_WIDTH  0x00000004 // DDSD_WIDTH

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

static constexpr u32 DDS_CAPS = 0x00000001;
static constexpr u32 DDS_DEPTH = 0x00800000;

enum DDS_MISC_FLAGS2
{
    DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
};

#pragma pack(pop)

static std::size_t BitsPerPixel( const _DXGI_FORMAT fmt )
{
    switch ( fmt ) {
    case DXGI_DIMENSION_R32G32B32A32_TYPELESS:
    case DXGI_DIMENSION_R32G32B32A32_FLOAT:
    case DXGI_DIMENSION_R32G32B32A32_UINT:
    case DXGI_DIMENSION_R32G32B32A32_SINT:
        return 128;

    case DXGI_DIMENSION_R32G32B32_TYPELESS:
    case DXGI_DIMENSION_R32G32B32_FLOAT:
    case DXGI_DIMENSION_R32G32B32_UINT:
    case DXGI_DIMENSION_R32G32B32_SINT:
        return 96;

    case DXGI_DIMENSION_R16G16B16A16_TYPELESS:
    case DXGI_DIMENSION_R16G16B16A16_FLOAT:
    case DXGI_DIMENSION_R16G16B16A16_UNORM:
    case DXGI_DIMENSION_R16G16B16A16_UINT:
    case DXGI_DIMENSION_R16G16B16A16_SNORM:
    case DXGI_DIMENSION_R16G16B16A16_SINT:
    case DXGI_DIMENSION_R32G32_TYPELESS:
    case DXGI_DIMENSION_R32G32_FLOAT:
    case DXGI_DIMENSION_R32G32_UINT:
    case DXGI_DIMENSION_R32G32_SINT:
    case DXGI_DIMENSION_R32G8X24_TYPELESS:
    case DXGI_DIMENSION_D32_FLOAT_S8X24_UINT:
    case DXGI_DIMENSION_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_DIMENSION_X32_TYPELESS_G8X24_UINT:
    case DXGI_DIMENSION_Y416:
    case DXGI_DIMENSION_Y210:
    case DXGI_DIMENSION_Y216:
        return 64;

    case DXGI_DIMENSION_R10G10B10A2_TYPELESS:
    case DXGI_DIMENSION_R10G10B10A2_UNORM:
    case DXGI_DIMENSION_R10G10B10A2_UINT:
    case DXGI_DIMENSION_R11G11B10_FLOAT:
    case DXGI_DIMENSION_R8G8B8A8_TYPELESS:
    case DXGI_DIMENSION_R8G8B8A8_UNORM:
    case DXGI_DIMENSION_R8G8B8A8_UNORM_SRGB:
    case DXGI_DIMENSION_R8G8B8A8_UINT:
    case DXGI_DIMENSION_R8G8B8A8_SNORM:
    case DXGI_DIMENSION_R8G8B8A8_SINT:
    case DXGI_DIMENSION_R16G16_TYPELESS:
    case DXGI_DIMENSION_R16G16_FLOAT:
    case DXGI_DIMENSION_R16G16_UNORM:
    case DXGI_DIMENSION_R16G16_UINT:
    case DXGI_DIMENSION_R16G16_SNORM:
    case DXGI_DIMENSION_R16G16_SINT:
    case DXGI_DIMENSION_R32_TYPELESS:
    case DXGI_DIMENSION_D32_FLOAT:
    case DXGI_DIMENSION_R32_FLOAT:
    case DXGI_DIMENSION_R32_UINT:
    case DXGI_DIMENSION_R32_SINT:
    case DXGI_DIMENSION_R24G8_TYPELESS:
    case DXGI_DIMENSION_D24_UNORM_S8_UINT:
    case DXGI_DIMENSION_R24_UNORM_X8_TYPELESS:
    case DXGI_DIMENSION_X24_TYPELESS_G8_UINT:
    case DXGI_DIMENSION_R9G9B9E5_SHAREDEXP:
    case DXGI_DIMENSION_R8G8_B8G8_UNORM:
    case DXGI_DIMENSION_G8R8_G8B8_UNORM:
    case DXGI_DIMENSION_B8G8R8A8_UNORM:
    case DXGI_DIMENSION_B8G8R8X8_UNORM:
    case DXGI_DIMENSION_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_DIMENSION_B8G8R8A8_TYPELESS:
    case DXGI_DIMENSION_B8G8R8A8_UNORM_SRGB:
    case DXGI_DIMENSION_B8G8R8X8_TYPELESS:
    case DXGI_DIMENSION_B8G8R8X8_UNORM_SRGB:
    case DXGI_DIMENSION_AYUV:
    case DXGI_DIMENSION_Y410:
    case DXGI_DIMENSION_YUY2:
        return 32;

    case DXGI_DIMENSION_P010:
    case DXGI_DIMENSION_P016:
        return 24;

    case DXGI_DIMENSION_R8G8_TYPELESS:
    case DXGI_DIMENSION_R8G8_UNORM:
    case DXGI_DIMENSION_R8G8_UINT:
    case DXGI_DIMENSION_R8G8_SNORM:
    case DXGI_DIMENSION_R8G8_SINT:
    case DXGI_DIMENSION_R16_TYPELESS:
    case DXGI_DIMENSION_R16_FLOAT:
    case DXGI_DIMENSION_D16_UNORM:
    case DXGI_DIMENSION_R16_UNORM:
    case DXGI_DIMENSION_R16_UINT:
    case DXGI_DIMENSION_R16_SNORM:
    case DXGI_DIMENSION_R16_SINT:
    case DXGI_DIMENSION_B5G6R5_UNORM:
    case DXGI_DIMENSION_B5G5R5A1_UNORM:
    case DXGI_DIMENSION_A8P8:
    case DXGI_DIMENSION_B4G4R4A4_UNORM:
        return 16;

    case DXGI_DIMENSION_NV12:
    case DXGI_DIMENSION_420_OPAQUE:
    case DXGI_DIMENSION_NV11:
        return 12;

    case DXGI_DIMENSION_R8_TYPELESS:
    case DXGI_DIMENSION_R8_UNORM:
    case DXGI_DIMENSION_R8_UINT:
    case DXGI_DIMENSION_R8_SNORM:
    case DXGI_DIMENSION_R8_SINT:
    case DXGI_DIMENSION_A8_UNORM:
    case DXGI_DIMENSION_AI44:
    case DXGI_DIMENSION_IA44:
    case DXGI_DIMENSION_P8:
        return 8;

    case DXGI_DIMENSION_R1_UNORM:
        return 1;

    case DXGI_DIMENSION_BC1_TYPELESS:
    case DXGI_DIMENSION_BC1_UNORM:
    case DXGI_DIMENSION_BC1_UNORM_SRGB:
    case DXGI_DIMENSION_BC4_TYPELESS:
    case DXGI_DIMENSION_BC4_UNORM:
    case DXGI_DIMENSION_BC4_SNORM:
        return 4;

    case DXGI_DIMENSION_BC2_TYPELESS:
    case DXGI_DIMENSION_BC2_UNORM:
    case DXGI_DIMENSION_BC2_UNORM_SRGB:
    case DXGI_DIMENSION_BC3_TYPELESS:
    case DXGI_DIMENSION_BC3_UNORM:
    case DXGI_DIMENSION_BC3_UNORM_SRGB:
    case DXGI_DIMENSION_BC5_TYPELESS:
    case DXGI_DIMENSION_BC5_UNORM:
    case DXGI_DIMENSION_BC5_SNORM:
    case DXGI_DIMENSION_BC6H_TYPELESS:
    case DXGI_DIMENSION_BC6H_UF16:
    case DXGI_DIMENSION_BC6H_SF16:
    case DXGI_DIMENSION_BC7_TYPELESS:
    case DXGI_DIMENSION_BC7_UNORM:
    case DXGI_DIMENSION_BC7_UNORM_SRGB:
        return 8;

    default:
        return 0;
    }
}

#define ISBITMASK( r,g,b,a ) ( ddpf.dwRBitMask == r && ddpf.dwGBitMask == g && ddpf.dwBBitMask == b && ddpf.dwABitMask == a )

_DXGI_FORMAT GetDXGIFormat( const DDS_PIXELFORMAT& ddpf )
{
    if ( ddpf.dwFlags & DDS_RGB ) {
        // Note that sRGB formats are written using the "DX10" extended header

        switch ( ddpf.dwRGBBitCount ) {
        case 32:
            if ( ISBITMASK( 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 ) ) {
                return DXGI_DIMENSION_R8G8B8A8_UNORM;
            }

            if ( ISBITMASK( 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 ) ) {
                return DXGI_DIMENSION_B8G8R8A8_UNORM;
            }

            if ( ISBITMASK( 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 ) ) {
                return DXGI_DIMENSION_B8G8R8X8_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0x00000000) aka D3DFMT_X8B8G8R8

            // Note that many common DDS reader/writers (including D3DX) swap the
            // the RED/BLUE masks for 10:10:10:2 formats. We assume
            // below that the 'backwards' header mask is being used since it is most
            // likely written by D3DX. The more robust solution is to use the 'DX10'
            // header extension and specify the DXGI_DIMENSION_R10G10B10A2_UNORM format directly

            // For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
            if ( ISBITMASK( 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 ) ) {
                return DXGI_DIMENSION_R10G10B10A2_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

            if ( ISBITMASK( 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 ) ) {
                return DXGI_DIMENSION_R16G16_UNORM;
            }

            if ( ISBITMASK( 0xffffffff, 0x00000000, 0x00000000, 0x00000000 ) ) {
                // Only 32-bit color channel format in D3D9 was R32F
                return DXGI_DIMENSION_R32_FLOAT; // D3DX writes this out as a FourCC of 114
            }
            break;

        case 24:
            // No 24bpp DXGI formats aka D3DFMT_R8G8B8
            break;

        case 16:
            if ( ISBITMASK( 0x7c00, 0x03e0, 0x001f, 0x8000 ) ) {
                return DXGI_DIMENSION_B5G5R5A1_UNORM;
            }
            if ( ISBITMASK( 0xf800, 0x07e0, 0x001f, 0x0000 ) ) {
                return DXGI_DIMENSION_B5G6R5_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0x0000) aka D3DFMT_X1R5G5B5

            if ( ISBITMASK( 0x0f00, 0x00f0, 0x000f, 0xf000 ) ) {
                return DXGI_DIMENSION_B4G4R4A4_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0x0000) aka D3DFMT_X4R4G4B4

            // No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
            break;
        }
    } else if ( ddpf.dwFlags & DDS_LUMINANCE ) {
        if ( 8 == ddpf.dwRGBBitCount ) {
            if ( ISBITMASK( 0x000000ff, 0x00000000, 0x00000000, 0x00000000 ) ) {
                return DXGI_DIMENSION_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
            }

            // No DXGI format maps to ISBITMASK(0x0f,0x00,0x00,0xf0) aka D3DFMT_A4L4

            if ( ISBITMASK( 0x000000ff, 0x00000000, 0x00000000, 0x0000ff00 ) ) {
                return DXGI_DIMENSION_R8G8_UNORM; // Some DDS writers assume the bitcount should be 8 instead of 16
            }
        }

        if ( 16 == ddpf.dwRGBBitCount ) {
            if ( ISBITMASK( 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 ) ) {
                return DXGI_DIMENSION_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
            }
            if ( ISBITMASK( 0x000000ff, 0x00000000, 0x00000000, 0x0000ff00 ) ) {
                return DXGI_DIMENSION_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
            }
        }
    } else if ( ddpf.dwFlags & DDS_ALPHA ) {
        if ( 8 == ddpf.dwRGBBitCount ) {
            return DXGI_DIMENSION_A8_UNORM;
        }
    } else if ( ddpf.dwFlags & DDS_BUMPDUDV ) {
        if ( 16 == ddpf.dwRGBBitCount ) {
            if ( ISBITMASK( 0x00ff, 0xff00, 0x0000, 0x0000 ) ) {
                return DXGI_DIMENSION_R8G8_SNORM; // D3DX10/11 writes this out as DX10 extension
            }
        }

        if ( 32 == ddpf.dwRGBBitCount ) {
            if ( ISBITMASK( 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 ) ) {
                return DXGI_DIMENSION_R8G8B8A8_SNORM; // D3DX10/11 writes this out as DX10 extension
            }
            if ( ISBITMASK( 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 ) ) {
                return DXGI_DIMENSION_R16G16_SNORM; // D3DX10/11 writes this out as DX10 extension
            }

            // No DXGI format maps to ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000) aka D3DFMT_A2W10V10U10
        }
    } else if ( ddpf.dwFlags & DDS_FOURCC ) {
        if ( MAKEFOURCC( 'D', 'X', 'T', '1' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC1_UNORM;
        }
        if ( MAKEFOURCC( 'D', 'X', 'T', '3' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC2_UNORM;
        }
        if ( MAKEFOURCC( 'D', 'X', 'T', '5' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC3_UNORM;
        }

        // While pre-multiplied alpha isn't directly supported by the DXGI formats,
        // they are basically the same as these BC formats so they can be mapped
        if ( MAKEFOURCC( 'D', 'X', 'T', '2' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC2_UNORM;
        }
        if ( MAKEFOURCC( 'D', 'X', 'T', '4' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC3_UNORM;
        }

        if ( MAKEFOURCC( 'A', 'T', 'I', '1' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC4_UNORM;
        }
        if ( MAKEFOURCC( 'B', 'C', '4', 'U' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC4_UNORM;
        }
        if ( MAKEFOURCC( 'B', 'C', '4', 'S' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC4_SNORM;
        }

        if ( MAKEFOURCC( 'A', 'T', 'I', '2' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC5_UNORM;
        }
        if ( MAKEFOURCC( 'B', 'C', '5', 'U' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC5_UNORM;
        }
        if ( MAKEFOURCC( 'B', 'C', '5', 'S' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_BC5_SNORM;
        }

        // BC6H and BC7 are written using the "DX10" extended header

        if ( MAKEFOURCC( 'R', 'G', 'B', 'G' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_R8G8_B8G8_UNORM;
        }
        if ( MAKEFOURCC( 'G', 'R', 'G', 'B' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_G8R8_G8B8_UNORM;
        }

        if ( MAKEFOURCC( 'Y', 'U', 'Y', '2' ) == ddpf.dwFourCC ) {
            return DXGI_DIMENSION_YUY2;
        }

        // Check for D3DFORMAT enums being set here
        switch ( ddpf.dwFourCC ) {
        case 36: // D3DFMT_A16B16G16R16
            return DXGI_DIMENSION_R16G16B16A16_UNORM;

        case 110: // D3DFMT_Q16W16V16U16
            return DXGI_DIMENSION_R16G16B16A16_SNORM;

        case 111: // D3DFMT_R16F
            return DXGI_DIMENSION_R16_FLOAT;

        case 112: // D3DFMT_G16R16F
            return DXGI_DIMENSION_R16G16_FLOAT;

        case 113: // D3DFMT_A16B16G16R16F
            return DXGI_DIMENSION_R16G16B16A16_FLOAT;

        case 114: // D3DFMT_R32F
            return DXGI_DIMENSION_R32_FLOAT;

        case 115: // D3DFMT_G32R32F
            return DXGI_DIMENSION_R32G32_FLOAT;

        case 116: // D3DFMT_A32B32G32R32F
            return DXGI_DIMENSION_R32G32B32A32_FLOAT;
        }
    }

    return DXGI_DIMENSION_UNKNOWN;
}

void dk::io::LoadDirectDrawSurface( FileSystemObject* stream, DirectDrawSurface& data )
{
    u32 fileMagic;
    stream->read( fileMagic );

    if ( fileMagic != DDS_MAGIC ) {
        DUSK_LOG_ERROR( "Invalid DDS file!\n" );
        return;
    }

    DDS_HEADER hdr;
    stream->read<DDS_HEADER>( hdr );

    // Verify header to validate DDS file
    if ( hdr.dwSize != sizeof( DDS_HEADER )
        || hdr.ddspf.dwSize != sizeof( DDS_PIXELFORMAT ) ) {
        return;
    }

    ImageDesc desc;
    desc.bindFlags = RESOURCE_BIND_SHADER_RESOURCE;
    desc.usage = RESOURCE_USAGE_STATIC;
    desc.width = hdr.dwWidth;
    desc.height = hdr.dwHeight;
    desc.depth = hdr.dwDepth;
    desc.mipCount = hdr.dwMipMapCount;
    desc.arraySize = 1;

    bool isDXT10Header = ( hdr.ddspf.dwFlags & DDS_FOURCC ) && ( MAKEFOURCC( 'D', 'X', '1', '0' ) == hdr.ddspf.dwFourCC );

    if ( isDXT10Header ) {
        DDS_HEADER_DXT10 d3d10ext;
        stream->read( d3d10ext );

        desc.arraySize = d3d10ext.arraySize;
        desc.format = static_cast<eViewFormat>( d3d10ext.dxgiFormat );

        switch ( d3d10ext.resourceDimension ) {
        case _D3D10_RESOURCE_DIMENSION_TEXTURE1D:
            desc.height = desc.depth = 1;
            desc.dimension = ImageDesc::DIMENSION_1D;
            break;

        case _D3D10_RESOURCE_DIMENSION_TEXTURE2D:
            if ( d3d10ext.miscFlag & _D3D11_RESOURCE_MISC_TEXTURECUBE ) {
                desc.arraySize *= 6;
                desc.miscFlags |= ImageDesc::IS_CUBE_MAP;
            }
            desc.depth = 1;
            desc.dimension = ImageDesc::DIMENSION_2D;
            break;

        case _D3D10_RESOURCE_DIMENSION_TEXTURE3D:
            if ( !( hdr.dwFlags & DDS_HEADER_FLAGS_VOLUME ) ) {
                return;
            }

            if ( desc.arraySize > 1 ) {
                return;
            }
            desc.dimension = ImageDesc::DIMENSION_3D;
            break;

        default:
            return;
        }
    } else {
        desc.format = static_cast<eViewFormat>( GetDXGIFormat( hdr.ddspf ) );

        if ( desc.format == 0 ) {
            return;
        }

        if ( hdr.dwFlags & DDS_HEADER_FLAGS_VOLUME ) {
            desc.dimension = ImageDesc::DIMENSION_3D;
        } else {
            if ( hdr.dwCaps2 & DDS_CUBEMAP ) {
                if ( ( hdr.dwCaps2 & DDS_CUBEMAP_ALLFACES ) != DDS_CUBEMAP_ALLFACES ) {
                    return;
                }

                desc.arraySize = 6;
                desc.miscFlags |= ImageDesc::IS_CUBE_MAP;
            }

            desc.depth = 1;
            desc.dimension = ImageDesc::DIMENSION_2D;

            // NOTE There's no way for a legacy Direct3D 9 DDS to express a '1D' texture
        }
    }

    u64 streamSize = stream->getSize();
    size_t texelsSize = streamSize - stream->tell();

    data.imageDesc = desc;
    data.textureData.resize( texelsSize );

    stream->read( &data.textureData[0], texelsSize );
}
