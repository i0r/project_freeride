/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#include <Core/ViewFormat.h>
#include <Core/DebugHelpers.h>

class BaseAllocator;
class DisplaySurface;
class CommandList;
class LinearAllocator;

struct RenderContext;
struct Image;
struct Buffer;
struct PipelineState;
struct Shader;
struct Sampler;
struct QueryPool;

enum eSamplerAddress
{
    SAMPLER_ADDRESS_WRAP = 0,
    SAMPLER_ADDRESS_MIRROR,
    SAMPLER_ADDRESS_CLAMP_EDGE,
    SAMPLER_ADDRESS_CLAMP_BORDER,
    SAMPLER_ADDRESS_MIRROR_ONCE,

    SAMPLER_ADDRESS_COUNT,
};

enum eSamplerFilter
{
    SAMPLER_FILTER_POINT = 0,
    SAMPLER_FILTER_BILINEAR,
    SAMPLER_FILTER_TRILINEAR,
    SAMPLER_FILTER_ANISOTROPIC_8,
    SAMPLER_FILTER_ANISOTROPIC_16,
    SAMPLER_FILTER_COMPARISON_POINT,
    SAMPLER_FILTER_COMPARISON_BILINEAR,
    SAMPLER_FILTER_COMPARISON_TRILINEAR,
    SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8,
    SAMPLER_FILTER_COMPARISON_ANISOTROPIC_16,

    SAMPLER_FILTER_COUNT
};

enum ePrimitiveTopology
{
    PRIMITIVE_TOPOLOGY_TRIANGLELIST = 0,
    PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    PRIMITIVE_TOPOLOGY_LINELIST,
    PRIMITIVE_TOPOLOGY_POINTLIST,

    PRIMITIVE_TOPOLOGY_COUNT,
};

enum eComparisonFunction
{
    COMPARISON_FUNCTION_NEVER = 0,
    COMPARISON_FUNCTION_ALWAYS,
    COMPARISON_FUNCTION_LESS,
    COMPARISON_FUNCTION_GREATER,
    COMPARISON_FUNCTION_LEQUAL,
    COMPARISON_FUNCTION_GEQUAL,
    COMPARISON_FUNCTION_NOT_EQUAL,
    COMPARISON_FUNCTION_EQUAL,
    COMPARISON_FUNCTION_COUNT
};

enum eStencilOperation
{
    STENCIL_OPERATION_KEEP = 0,
    STENCIL_OPERATION_ZERO,
    STENCIL_OPERATION_REPLACE,
    STENCIL_OPERATION_INC,
    STENCIL_OPERATION_INC_WRAP,
    STENCIL_OPERATION_DEC,
    STENCIL_OPERATION_DEC_WRAP,
    STENCIL_OPERATION_INVERT,
    STENCIL_OPERATION_COUNT
};

enum eCullMode
{
    CULL_MODE_NONE = 0,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
    CULL_MODE_COUNT
};

enum eFillMode
{
    FILL_MODE_SOLID = 0,
    FILL_MODE_WIREFRAME,

    FILL_MODE_COUNT
};

enum eBlendSource
{
    BLEND_SOURCE_ZERO = 0,
    BLEND_SOURCE_ONE,
    BLEND_SOURCE_SRC_COLOR,
    BLEND_SOURCE_INV_SRC_COLOR,
    BLEND_SOURCE_SRC_ALPHA,
    BLEND_SOURCE_INV_SRC_ALPHA,
    BLEND_SOURCE_DEST_ALPHA,
    BLEND_SOURCE_INV_DEST_ALPHA,
    BLEND_SOURCE_DEST_COLOR,
    BLEND_SOURCE_INV_DEST_COLOR,
    BLEND_SOURCE_SRC_ALPHA_SAT,
    BLEND_SOURCE_BLEND_FACTOR,
    BLEND_SOURCE_INV_BLEND_FACTOR,

    BLEND_SOURCE_COUNT
};

enum eBlendOperation
{
    BLEND_OPERATION_ADD = 0,
    BLEND_OPERATION_SUB,
    BLEND_OPERATION_MIN,
    BLEND_OPERATION_MAX,
    BLEND_OPERATION_MUL,

    BLEND_OPERATION_COUNT
};

enum eQueryType
{
    QUERY_TYPE_TIMESTAMP = 0,

    QUERY_TYPE_COUNT
};

enum eShaderStage
{
    SHADER_STAGE_VERTEX = 1 << 1,
    SHADER_STAGE_PIXEL = 1 << 2,
    SHADER_STAGE_TESSELATION_CONTROL = 1 << 3,
    SHADER_STAGE_TESSELATION_EVALUATION = 1 << 4,
    SHADER_STAGE_COMPUTE = 1 << 5,

    SHADER_STAGE_COUNT = 5,

    SHADER_STAGE_ALL = ~0
};

enum eResourceBind : u32
{
    RESOURCE_BIND_UNBINDABLE = 1 << 0,
    RESOURCE_BIND_CONSTANT_BUFFER = 1 << 1,
    RESOURCE_BIND_VERTEX_BUFFER = 1 << 2,
    RESOURCE_BIND_INDICE_BUFFER = 1 << 3,
    RESOURCE_BIND_UNORDERED_ACCESS_VIEW = 1 << 4,
    RESOURCE_BIND_STRUCTURED_BUFFER = 1 << 5,
    RESOURCE_BIND_INDIRECT_ARGUMENTS = 1 << 6,
    RESOURCE_BIND_SHADER_RESOURCE = 1 << 7,
    RESOURCE_BIND_RENDER_TARGET_VIEW = 1 << 8,
    RESOURCE_BIND_DEPTH_STENCIL = 1 << 9,
    RESOURCE_BIND_APPEND_STRUCTURED_BUFFER = 1 << 10
};

enum eResourceUsage : u32
{
    RESOURCE_USAGE_DEFAULT = 0, // GPU READ / WRITE
    RESOURCE_USAGE_STATIC,      // GPU READ ONLY
    RESOURCE_USAGE_DYNAMIC,     // GPU READ ONLY / CPU WRITE ONLY
    RESOURCE_USAGE_STAGING,     // GPU TO CPU COPY
};

struct InputLayoutEntry
{
    u32             index;
    eViewFormat     format;
    u32             instanceCount;
    u32             vertexBufferIndex;
    u32             offsetInBytes;
    bool            needPadding;
    const char*     semanticName;
};

struct SamplerDesc
{
    eSamplerFilter         filter;
    eSamplerAddress        addressU;
    eSamplerAddress        addressV;
    eSamplerAddress        addressW;
    eComparisonFunction    comparisonFunction;
    i32                    minLOD;
    i32                    maxLOD;
    f32                    borderColor[4];

    constexpr SamplerDesc( const eSamplerFilter filter = eSamplerFilter::SAMPLER_FILTER_BILINEAR,
                 const eSamplerAddress addressU = eSamplerAddress::SAMPLER_ADDRESS_WRAP,
                 const eSamplerAddress addressV = eSamplerAddress::SAMPLER_ADDRESS_WRAP,
                 const eSamplerAddress addressW = eSamplerAddress::SAMPLER_ADDRESS_WRAP,
                 const eComparisonFunction comparisonFunction = eComparisonFunction::COMPARISON_FUNCTION_ALWAYS )
        : filter( filter )
        , addressU( addressU )
        , addressV( addressV )
        , addressW( addressW )
        , comparisonFunction( comparisonFunction )
        , minLOD( 0 )
        , maxLOD( 1000 )
        , borderColor{ 0.0f, 0.0f, 0.0f, 1.0f }
    {

    }


    bool operator == ( const SamplerDesc& desc ) const
    {
        return ( addressU == desc.addressU
            && addressV == desc.addressV
            && addressW == desc.addressW
            && comparisonFunction == desc.comparisonFunction
            && filter == desc.filter
            && maxLOD == desc.maxLOD
            && minLOD == desc.minLOD );
    }
};

struct BlendStateDesc
{
    union {
        struct {
            // Per color channel write mask (if true, the blend unit will write to the channel).
            u8        WriteR : 1;
            u8        WriteG : 1;
            u8        WriteB : 1;
            u8        WriteA : 1;

            // Enable/disable blend (this is applied to every RTs in the current framebuffer).
            u8        EnableBlend : 1;

            // If true, use a separate blend state for alpha channel.
            u8        UseSeperateAlpha : 1;

            // Enable/disable alpha to coverage (it might be costly depending on the use case).
            u8        EnableAlphaToCoverage : 1;

            struct {
                eBlendSource       Source : 4;
                eBlendSource       Destination : 4;
                eBlendOperation    Operation : 3;
            } BlendConfColor, BlendConfAlpha;

            // Explicit padding to correctly align the descriptor. (3bits left)
            u8 : 0;
        };

        u64 SortKey;
    };

    constexpr BlendStateDesc& operator = ( const BlendStateDesc& r )
    {
        SortKey = r.SortKey;
        return *this;
    }

    constexpr bool operator == ( const BlendStateDesc& r ) const
    {
        return ( SortKey ^ r.SortKey ) == 0;
    }

    constexpr bool operator != ( const BlendStateDesc& r ) const
    {
        return !( *this == r );
    }

    constexpr BlendStateDesc( 
        const bool enableBlend = false, 
        const bool useSeparateAlpha = false,
        const bool enableAlphaToCoverage = false,
        const bool writeR = false,
        const bool writeG = false,
        const bool writeB = false,
        const bool writeA = false,
        const decltype( BlendConfColor ) blendConfColor = { BLEND_SOURCE_ONE, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD },
        const decltype( BlendConfAlpha ) blendConfAlpha = { BLEND_SOURCE_ONE, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD }
    )
        : EnableBlend( enableBlend )
        , UseSeperateAlpha( useSeparateAlpha )
        , EnableAlphaToCoverage( enableAlphaToCoverage )
        , WriteR( writeR )
        , WriteG( writeG )
        , WriteB( writeB )
        , WriteA( writeA )
        , BlendConfColor( blendConfColor )
        , BlendConfAlpha( blendConfAlpha )
    {

    }
};

struct ImageViewDesc     
{
    union {
        struct {
            // Index of the first image to be covered by the view.
            u8              StartImageIndex;

            // Index of the first mip to be covered by the view. (0 being the highest detailed mip available).
            u8              StartMipIndex;

            // If 0, will cover the whole mipchain.
            u8              MipCount;

            // If 0, will cover the whole array of images.
            u8              ImageCount;

            // If VIEW_FORMAT_UNKNOWN, will use the default format of the resource associated to this view.
            eViewFormat    ViewFormat;
        };

        // The image view interpreted as a 64bits integer (for hashmap indexing).
        u64 SortKey;
    };

    constexpr ImageViewDesc()
        : SortKey( 0 )
    {

    }

    constexpr bool operator == ( const ImageViewDesc& r )
    {
        return ( SortKey ^ r.SortKey ) == 0ull;
    }
};

struct ImageDesc
{
    enum : u32 {
        DIMENSION_UNKNOWN = 0,

        DIMENSION_1D,
        DIMENSION_2D,
        DIMENSION_3D
    } dimension;

    // Misc. Flags
    enum {
        IS_CUBE_MAP                     = 1 << 1,
        ENABLE_HARDWARE_MIP_GENERATION  = 1 << 2
    };

    eViewFormat     format;
    u32             width;
    u32             height;
    u32             depth;
    u32             arraySize;
    u32             mipCount;
    u32             samplerCount;
    u32             bindFlags;
    u32             miscFlags;
    eResourceUsage  usage;

    // Describe the default view for this image RTV/SRV/UAV (depending on the BindFlags combination).
    ImageViewDesc   DefaultView;

    ImageDesc()
        : dimension( DIMENSION_UNKNOWN )
        , format( static_cast<eViewFormat>( 0 ) )
        , width( 0 )
        , height( 0 )
        , depth( 0 )
        , arraySize( 1 )
        , mipCount( 1 )
        , samplerCount( 1 )
        , bindFlags( 0 )
        , miscFlags( 0 )
        , usage( RESOURCE_USAGE_DEFAULT )
        , DefaultView()
    {

    }

    bool operator == ( const ImageDesc& r ) const {
        return format == r.format
            && width == r.width
            && height == r.height
            && depth == r.depth
            && arraySize == r.arraySize
            && mipCount == r.mipCount
            && samplerCount == r.samplerCount
            && bindFlags == r.bindFlags
            && miscFlags == r.miscFlags
            && usage == r.usage;
    }
};

struct BufferViewDesc
{
    union {
        struct {
            // Index of the first element to be covered by the view. (the units is dependent on the ViewFormat)
            u16             FirstElement;

            // If 0, will cover all the elements.
            u16             NumElements;

            // If VIEW_FORMAT_UNKNOWN, will use the default format of the resource associated to this view.
            eViewFormat    ViewFormat;
        };

        // The buffer view interpreted as a 64bits integer (for hashmap indexing).
        u64 SortKey;
    };

    // Generates BufferViewDesc sort key (helper for constexpr constructor; the sort key shouldn't be manually updated
    // thanks to memory aliasing)
    constexpr DUSK_INLINE u64 generateSortKey( const i16 firstElement = 0, const i16 numElements = -1, const eViewFormat viewFormat = VIEW_FORMAT_UNKNOWN ) const
    {
        return ( firstElement ) | ( numElements << 16 ) | ( static_cast< u64 >( viewFormat ) << 32 );
    }

    constexpr BufferViewDesc( const i16 firstElement = 0, const i16 numElements = 0, const eViewFormat viewFormat = VIEW_FORMAT_UNKNOWN )
        : SortKey( generateSortKey( firstElement, numElements, viewFormat ) )
    {

    }

    constexpr bool operator == ( const BufferViewDesc& r )
    {
        return ( SortKey ^ r.SortKey ) == 0ull;
    }
};

struct BufferDesc
{
    // Total size of the buffer (in bytes).
    u32             SizeInBytes;
    
    // Per element size (in bytes) (e.g. if you have a 16 bytes buffer of 32 bits integer, then stride should be 32bits = 4bytes).
    u32             StrideInBytes;

    // Bind flags for the resource (see eResourceBind). Since those flags have an impact on performances, try to avoid setting unnecessary flags.
    u32             BindFlags;

    // Usage of the resource (see eResourceUsage). Basically sets the location of the resource in memory and its r/w access).
    eResourceUsage  Usage;

    // Describe the default view for this buffers RTV/SRV/UAV (depending on the BindFlags combination).
    BufferViewDesc   DefaultView;

    BufferDesc()
        : SizeInBytes( 0 )
        , StrideInBytes( 1 )
        , BindFlags( 0 )
        , Usage( RESOURCE_USAGE_DEFAULT )
        , DefaultView() {

    }

    bool operator == ( const BufferDesc& r ) const {
        return SizeInBytes == r.SizeInBytes
            && StrideInBytes == r.StrideInBytes
            && BindFlags == r.BindFlags
            && Usage == r.Usage;
    }
};

struct DepthStencilStateDesc
{
    union {
        struct {
            u8                              EnableDepthTest : 1;
            u8                              EnableStencilTest : 1;
            u8                              EnableDepthWrite : 1;
            u8                              EnableDepthBoundsTest : 1;
            eComparisonFunction             DepthComparisonFunc : 4;
            u8                              StencilRefValue;
            u8                              StencilWriteMask;
            u8                              StencilReadMask;

            f32                             DepthBoundsMin;
            f32                             DepthBoundsMax;

            struct {
                eComparisonFunction         ComparisonFunction : 3;
                eStencilOperation           PassOperation : 3;
                eStencilOperation           FailOperation : 3;
                eStencilOperation           ZFailOperation : 3;
            } Front, Back;
            u8 : 0;
        };

        u64 SortKey[2];
    };

    constexpr DepthStencilStateDesc( 
        const bool enableDepthTest = false,
        const bool enableStencilTest = false,
        const bool enableDepthWrite = false,
        const bool enableDepthBoundsTest = false,
        const f32 depthBoundsMin = 0.0f,
        const f32 depthBoundsMax = 1.0f,
        const eComparisonFunction depthComparisonFunc = COMPARISON_FUNCTION_ALWAYS,
        const u8 stencilRefValue = 0xff,
        const u8 stencilWriteMask = 0x00,
        const u8 stencilReadMask = 0x00,
        const decltype( Front ) frontStencil = { COMPARISON_FUNCTION_ALWAYS, STENCIL_OPERATION_KEEP, STENCIL_OPERATION_KEEP, STENCIL_OPERATION_KEEP },
        const decltype( Back ) backStencil = { COMPARISON_FUNCTION_ALWAYS, STENCIL_OPERATION_KEEP, STENCIL_OPERATION_KEEP, STENCIL_OPERATION_KEEP }
    )
        : SortKey{ 0, 0 }
        , EnableDepthTest( enableDepthTest )
        , EnableStencilTest( enableStencilTest )
        , EnableDepthWrite( enableDepthWrite )
        , EnableDepthBoundsTest( enableDepthBoundsTest )
        , DepthBoundsMin( depthBoundsMin )
        , DepthBoundsMax( depthBoundsMax )
        , DepthComparisonFunc( depthComparisonFunc )
        , StencilRefValue( stencilRefValue )
        , StencilWriteMask( stencilWriteMask )
        , StencilReadMask( stencilReadMask )
        , Front( frontStencil )
        , Back( backStencil )
    {

    }

    constexpr DepthStencilStateDesc& operator = ( const DepthStencilStateDesc& r )
    {
        SortKey[0] = r.SortKey[0];
        SortKey[1] = r.SortKey[1];

        return *this;
    }

    constexpr bool operator == ( const DepthStencilStateDesc& r ) const
    {
        return ( SortKey[0] ^ r.SortKey[0] ) == 0
            && ( SortKey[1] ^ r.SortKey[1] ) == 0;
    }

    constexpr bool operator != ( const DepthStencilStateDesc& r ) const
    {
        return !( *this == r );
    }
};

struct RasterizerStateDesc
{
    union {
        struct {
            // Depth bias.
            f32         DepthBias;

            // Slope scale.
            f32         SlopeScale;

            // Depth bias (clamped).
            f32         DepthBiasClamp;

            // Backface culling mode.
            eCullMode   CullMode : 4;

            // Primitive fill mode.
            eFillMode   FillMode : 2;

            // Use front counter clockwise face order.
            u8          UseTriangleCCW : 1;
        };

        u64 SortKey[2];
    };

    constexpr RasterizerStateDesc( 
        const eCullMode cullMode = CULL_MODE_NONE,
        const eFillMode fillMode = FILL_MODE_SOLID,
        const bool useTriangleCCW = false,
        const f32 depthBias = 0.0f,
        const f32 slopeScale = 0.0f,
        const f32 depthBiasClamp = 0.0f
    )
        : SortKey{ 0, 0 }
        , DepthBias( depthBias )
        , SlopeScale( slopeScale )
        , DepthBiasClamp( depthBiasClamp )
        , CullMode( cullMode )
        , FillMode( fillMode )
        , UseTriangleCCW( useTriangleCCW )
    {

    }

    constexpr RasterizerStateDesc& operator = ( const RasterizerStateDesc& r )
    {
        SortKey[0] = r.SortKey[0];
        SortKey[1] = r.SortKey[1];

        return *this;
    }

    constexpr bool operator == ( const RasterizerStateDesc& r ) const
    {
        return ( SortKey[0] ^ r.SortKey[0] ) == 0
            && ( SortKey[1] ^ r.SortKey[1] ) == 0;
    }

    constexpr bool operator != ( const RasterizerStateDesc& r ) const
    {
        return !( *this == r );
    }
};

struct FramebufferLayoutDesc
{
    static constexpr i32 MAX_ATTACHMENT_COUNT = 8;

    enum InitialState {
        DONT_CARE,
        CLEAR
    };

    enum BindMode {
        UNUSED = 0,
        WRITE_COLOR,
        WRITE_DEPTH,
        WRITE_SWAPCHAIN
    };

    struct AttachmentDesc {
        BindMode     bindMode       : 16;
        InitialState targetState    : 16;
        eViewFormat viewFormat     : 32;

        constexpr AttachmentDesc(
            const BindMode bindMode = UNUSED,
            const InitialState targetState = DONT_CARE,
            const eViewFormat viewFormat = VIEW_FORMAT_UNKNOWN )
            : bindMode( bindMode )
            , targetState( targetState )
            , viewFormat( viewFormat )
        {

        }
    } Attachments[MAX_ATTACHMENT_COUNT];

    AttachmentDesc depthStencilAttachment;

    constexpr FramebufferLayoutDesc( 
        const AttachmentDesc attachment0 = AttachmentDesc(),
        const AttachmentDesc attachment1 = AttachmentDesc(),
        const AttachmentDesc attachment2 = AttachmentDesc(),
        const AttachmentDesc attachment3 = AttachmentDesc(),
        const AttachmentDesc attachment4 = AttachmentDesc(),
        const AttachmentDesc attachment5 = AttachmentDesc(),
        const AttachmentDesc attachment6 = AttachmentDesc(),
        const AttachmentDesc attachment7 = AttachmentDesc(),
        const AttachmentDesc depthStencil = AttachmentDesc()
    )
        : Attachments{ attachment0, attachment1, attachment2, attachment3, attachment4, attachment5, attachment6, attachment7 }
        , depthStencilAttachment( depthStencil )
    {

    }

    DUSK_INLINE u32 getAttachmentCount() const {
        for ( u32 i = 0; i < MAX_ATTACHMENT_COUNT; i++ ) {
            if ( Attachments[i].bindMode == BindMode::UNUSED ) {
                return i;
            }
        }

        return MAX_ATTACHMENT_COUNT;
    }

    DUSK_INLINE void declareRTV( const i32 attachmentIndex, const eViewFormat viewFormat, const InitialState initialState = InitialState::DONT_CARE )
    {
        DUSK_DEV_ASSERT( attachmentIndex < MAX_ATTACHMENT_COUNT, "Attachment Index Out of Bounds! (%i, MAX_ATTACHMENT_COUNT is %i)\n", attachmentIndex, MAX_ATTACHMENT_COUNT );

        AttachmentDesc& attachment = Attachments[attachmentIndex];
        attachment.bindMode = BindMode::WRITE_COLOR;
        attachment.targetState = initialState;
        attachment.viewFormat = viewFormat;
    }

    DUSK_INLINE void declareSwapchain( const i32 attachmentIndex, const eViewFormat viewFormat )
    {
        DUSK_DEV_ASSERT( attachmentIndex < MAX_ATTACHMENT_COUNT, "Attachment Index Out of Bounds! (%i, MAX_ATTACHMENT_COUNT is %i)\n", attachmentIndex, MAX_ATTACHMENT_COUNT );

        AttachmentDesc& attachment = Attachments[attachmentIndex];
        attachment.bindMode = BindMode::WRITE_SWAPCHAIN;
        attachment.targetState = InitialState::DONT_CARE;
        attachment.viewFormat = viewFormat;
    }

    DUSK_INLINE void declareDSV( const eViewFormat viewFormat, const InitialState initialState = InitialState::DONT_CARE )
    {
        depthStencilAttachment.bindMode = BindMode::WRITE_DEPTH;
        depthStencilAttachment.targetState = initialState;
        depthStencilAttachment.viewFormat = viewFormat;
    }
};

struct PipelineStateDesc {
    static constexpr i32 MAX_STATIC_SAMPLER_COUNT = 8;
    static constexpr i32 MAX_INPUT_LAYOUT_ENTRY_COUNT = 8;
    static constexpr i32 MAX_RESOURCE_COUNT = 64;

    Shader* vertexShader;
    Shader* tesselationControlShader;
    Shader* tesselationEvalShader;
    union {
        Shader* pixelShader;
        Shader* computeShader;
    };

    void* cachedPsoData;
    size_t                              cachedPsoSize;

    enum Type {
        GRAPHICS = 1 << 0,
        COMPUTE = 1 << 1
    } PipelineType;

    ePrimitiveTopology                  PrimitiveTopology;
    DepthStencilStateDesc               DepthStencilState;
    RasterizerStateDesc                 RasterizerState;
    BlendStateDesc                      BlendState;
    struct {
        InputLayoutEntry                Entry[MAX_INPUT_LAYOUT_ENTRY_COUNT];
    } InputLayout;
    FramebufferLayoutDesc               FramebufferLayout;

    struct {
        SamplerDesc                     StaticSamplersDesc[MAX_STATIC_SAMPLER_COUNT];
        i32                             StaticSamplerCount; // You should use the helper func. 'addStaticSampler' to avoid manually updating this var
    } StaticSamplers;

    // Aka "Optimized Clear Values". This clear value will improve performances on modern GFX API only (Vulkan, D3D12, Metal).
    enum {
        CLEAR_COLOR_WHITE,
        CLEAR_COLOR_BLACK
    } ColorClearValue;

    f32                                 depthClearValue;
    u32                                 samplerCount;
    u8                                  stencilClearValue;

    constexpr PipelineStateDesc(
        const PipelineStateDesc::Type pipelineType = PipelineStateDesc::GRAPHICS,
        const ePrimitiveTopology primitiveTopology = ePrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        const DepthStencilStateDesc& depthStencilState = DepthStencilStateDesc(),
        const RasterizerStateDesc& rasterizerState = RasterizerStateDesc(),
        const BlendStateDesc& blendStateDesc = BlendStateDesc(),
        const FramebufferLayoutDesc& framebufferLayout = FramebufferLayoutDesc(),
        const decltype( StaticSamplers )& staticSamplers = {},
        const decltype( InputLayout )& inputLayout = {}
    )
        : vertexShader( nullptr )
        , tesselationControlShader( nullptr )
        , tesselationEvalShader( nullptr )
        , pixelShader( nullptr )
        , cachedPsoData( nullptr )
        , cachedPsoSize( 0 )
        , PipelineType( pipelineType )
        , PrimitiveTopology( primitiveTopology )
        , DepthStencilState( depthStencilState )
        , RasterizerState( rasterizerState )
        , BlendState( blendStateDesc )
        , InputLayout( inputLayout )
        , FramebufferLayout( framebufferLayout )
        , StaticSamplers( staticSamplers )
        , ColorClearValue( CLEAR_COLOR_BLACK )
        , depthClearValue( 0.0f )
        , samplerCount( 1 )
        , stencilClearValue( 0xff ) 
    {
    }

    DUSK_INLINE void addStaticSampler( const SamplerDesc& samplerDesc ) {
        DUSK_DEV_ASSERT( ( StaticSamplers.StaticSamplerCount < MAX_STATIC_SAMPLER_COUNT ),
            "Too many static samplers! If you need more than 8 sampler state for your pso, allocate a sampler state from the heap!" );

        StaticSamplers.StaticSamplersDesc[StaticSamplers.StaticSamplerCount++] = samplerDesc;
    }
};

// Compile-time generated descriptors for stuff regulary used (e.g. sampler states; buffer views; etc.)
// S_* : SamplerDesc
// BV_* : BufferViewDesc
// IV_* : ImageViewDesc
// BS_* : BlendStateDesc
namespace RenderingHelpers
{
    // Sampler
    static constexpr SamplerDesc S_BilinearWrap = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_BILINEAR,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    static constexpr SamplerDesc S_BilinearClampEdge = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_BILINEAR,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    static constexpr SamplerDesc S_BilinearClampBorder = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_BILINEAR,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    static constexpr SamplerDesc S_PointWrap = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_POINT,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eSamplerAddress::SAMPLER_ADDRESS_WRAP,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    static constexpr SamplerDesc S_PointClampEdge = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_POINT,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_EDGE,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    static constexpr SamplerDesc S_PointClampBorder = SamplerDesc(
        eSamplerFilter::SAMPLER_FILTER_POINT,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eSamplerAddress::SAMPLER_ADDRESS_CLAMP_BORDER,
        eComparisonFunction::COMPARISON_FUNCTION_ALWAYS
    );

    // Buffer View
    static constexpr BufferViewDesc BV_WholeBuffer = BufferViewDesc();

    // Image View
    static constexpr ImageViewDesc IV_WholeArrayAndMipchain = ImageViewDesc();

    // Blend State
    static constexpr BlendStateDesc BS_Disabled = BlendStateDesc();

    static constexpr BlendStateDesc BS_PremultipliedAlpha = BlendStateDesc(
        true,
        true,
        false,
        true, true, true, true,
        { BLEND_SOURCE_ONE, BLEND_SOURCE_INV_SRC_ALPHA, BLEND_OPERATION_ADD },
        { BLEND_SOURCE_ONE, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD }
    );

    static constexpr BlendStateDesc BS_Additive = BlendStateDesc(
        true,
        true,
        false,
        true, true, true, true,
        { BLEND_SOURCE_SRC_ALPHA, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD },
        { BLEND_SOURCE_ZERO, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD }
    );
    
    static constexpr BlendStateDesc BS_AdditiveNoAlpha = BlendStateDesc(
        true,
        false,
        false,
        true, true, true, false,
        { BLEND_SOURCE_SRC_ALPHA, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD },
        { BLEND_SOURCE_ZERO, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD }
    );

    static constexpr BlendStateDesc BS_SpriteBlend = BlendStateDesc(
        true,
        false,
        false,
        true, true, true, false,
        { BLEND_SOURCE_SRC_ALPHA, BLEND_SOURCE_INV_SRC_ALPHA, BLEND_OPERATION_ADD },
        { BLEND_SOURCE_ZERO, BLEND_SOURCE_ONE, BLEND_OPERATION_ADD }
    );

    static constexpr PipelineStateDesc PS_Compute = PipelineStateDesc( PipelineStateDesc::COMPUTE );
}

class RenderDevice
{
public:
#if DUSK_D3D11
    // NOTE Resource buffering is already done by the driver
    static constexpr i32        PENDING_FRAME_COUNT = 1;
#else
    static constexpr i32        PENDING_FRAME_COUNT = 3;
#endif

    static constexpr i32        CMD_LIST_POOL_CAPACITY = 32; // Capacity for each Cmdlist type

public:
    // Returns a pointer to the active RenderContext used by the RenderDevice
    // This should only be used by third-party APIs/libraries which requires access
    // to the underlying rendering API
    DUSK_INLINE RenderContext*  getRenderContext() const { return renderContext; }

public:
                                RenderDevice( BaseAllocator* allocator );
                                RenderDevice( RenderDevice& ) = delete;
                                RenderDevice& operator = ( RenderDevice& ) = delete;
                                ~RenderDevice();

    void                        create( DisplaySurface& displaySurface, const u32 desiredRefreshRate, const bool useDebugContext = false );
    void                        enableVerticalSynchronisation( const bool enabled );
    void                        present();

    void                        waitForPendingFrameCompletion();

    static const dkChar_t*      getBackendName();

    CommandList&                allocateGraphicsCommandList();
    CommandList&                allocateComputeCommandList();
    CommandList&                allocateCopyCommandList();

    void                        submitCommandList( CommandList& cmdList );
    void                        submitCommandLists( CommandList** cmdLists, const u32 cmdListCount );

    size_t                      getFrameIndex() const;
    Image*                      getSwapchainBuffer();
    u32                         getActiveRefreshRate() const;

    Image*                      createImage( const ImageDesc& description, const void* initialData = nullptr, const size_t initialDataSize = 0 );
    Buffer*                     createBuffer( const BufferDesc& description, const void* initialData = nullptr );
    Shader*                     createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize );
    Sampler*                    createSampler( const SamplerDesc& description );
    PipelineState*              createPipelineState( const PipelineStateDesc& description );
    QueryPool*                  createQueryPool( const eQueryType type, const u32 poolCapacity );

    void                        destroyImage( Image* image );
    void                        destroyBuffer( Buffer* buffer );
    void                        destroyShader( Shader* shader );
    void                        destroyPipelineState( PipelineState* pipelineState );
    void                        destroyPipelineStateCache( PipelineState* pipelineState );
    void                        destroySampler( Sampler* sampler );
    void                        destroyQueryPool( QueryPool* queryPool );

    void                        setDebugMarker( Image& image, const dkChar_t* objectName );
    void                        setDebugMarker( Buffer& buffer, const dkChar_t* objectName );

    f64                         convertTimestampToMs( const u64 timestamp ) const;
    void                        getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize );

private:
    BaseAllocator*              memoryAllocator;
    LinearAllocator*            pipelineStateCacheAllocator;
    RenderContext*              renderContext;
    Image*                      swapChainImage;
    size_t                      frameIndex;
    u32                         refreshRate;
    
    LinearAllocator*            graphicsCmdListAllocator[PENDING_FRAME_COUNT];
    LinearAllocator*            computeCmdListAllocator[PENDING_FRAME_COUNT];
};
