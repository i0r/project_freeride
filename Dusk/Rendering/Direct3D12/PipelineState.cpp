/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D12
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "PipelineState.h"
#include "ComparisonFunctions.h"
#include "Image.h"
#include "Sampler.h" // LUTs for static sampler description
#include "Buffer.h"

#include <Core/Allocators/LinearAllocator.h>

#include <d3d12.h>

#include <ThirdParty/dxc/include/dxc/dxcapi.use.h>

static constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE PRIMITIVE_TOPOLOGY_TYPE_LUT[ePrimitiveTopology::PRIMITIVE_TOPOLOGY_COUNT] = 
{
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
};

static constexpr D3D12_PRIMITIVE_TOPOLOGY PRIMITIVE_TOPOLOGY_LUT[ePrimitiveTopology::PRIMITIVE_TOPOLOGY_COUNT] = 
{
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    D3D_PRIMITIVE_TOPOLOGY_LINELIST,
};

static constexpr D3D12_FILL_MODE FILL_MODE_LUT[eFillMode::FILL_MODE_COUNT] = 
{
    D3D12_FILL_MODE_SOLID,
    D3D12_FILL_MODE_WIREFRAME
};

static constexpr D3D12_CULL_MODE CULL_MODE_LUT[eCullMode::CULL_MODE_COUNT] = 
{
    D3D12_CULL_MODE_NONE,
    D3D12_CULL_MODE_FRONT,
    D3D12_CULL_MODE_BACK
};

static constexpr D3D12_BLEND BLEND_SOURCE_LUT[eBlendSource::BLEND_SOURCE_COUNT] = {
    D3D12_BLEND_ZERO,
    D3D12_BLEND_ONE,

    D3D12_BLEND_SRC_COLOR,
    D3D12_BLEND_INV_SRC_COLOR,

    D3D12_BLEND_SRC_ALPHA,
    D3D12_BLEND_INV_SRC_ALPHA,

    D3D12_BLEND_DEST_ALPHA,
    D3D12_BLEND_INV_DEST_ALPHA,

    D3D12_BLEND_DEST_COLOR,
    D3D12_BLEND_INV_DEST_COLOR,

    D3D12_BLEND_SRC_ALPHA_SAT,

    D3D12_BLEND_BLEND_FACTOR,
    D3D12_BLEND_INV_BLEND_FACTOR,
};

static constexpr D3D12_BLEND_OP BLEND_OPERATION_LUT[eBlendOperation::BLEND_OPERATION_COUNT] =
{
    D3D12_BLEND_OP_ADD,
    D3D12_BLEND_OP_SUBTRACT,
    D3D12_BLEND_OP_MIN,
    D3D12_BLEND_OP_MAX,
};

static constexpr D3D12_STENCIL_OP STENCIL_OPERATION_LUT[eStencilOperation::STENCIL_OPERATION_COUNT] =
{
    D3D12_STENCIL_OP_KEEP,
    D3D12_STENCIL_OP_ZERO,
    D3D12_STENCIL_OP_REPLACE,
    D3D12_STENCIL_OP_INCR,
    D3D12_STENCIL_OP_INCR_SAT,
    D3D12_STENCIL_OP_DECR,
    D3D12_STENCIL_OP_DECR_SAT,
    D3D12_STENCIL_OP_INVERT
};

// Not sure if there is a point exposing the Shader implementation on D3D12
struct Shader
{
    D3D12_SHADER_BYTECODE bytecode;

    // Misc Infos
    struct
    {
        std::string name;

        u32 spaceIndex;
        u32 bindingIndex;

        D3D12_ROOT_PARAMETER_TYPE type;
        size_t resourceSize;

        D3D12_SHADER_VISIBILITY shaderVisibility;

        struct
        {
            size_t offset;
            size_t range;
        } rangeBinding;
    } resourcesReflectionInfos[96];

    u32 reflectedResourceCount;
};

static DUSK_INLINE D3D12_SHADER_VISIBILITY GetShaderVisibility( const eShaderStage stage )
{
    switch ( stage ) {
    case SHADER_STAGE_VERTEX:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case SHADER_STAGE_TESSELATION_CONTROL:
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    case SHADER_STAGE_TESSELATION_EVALUATION:
        return D3D12_SHADER_VISIBILITY_HULL;
    case SHADER_STAGE_PIXEL:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    default:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

Shader* RenderDevice::createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize )
{
    Shader* shader = dk::core::allocate<Shader>( memoryAllocator );

    const D3D12_SHADER_VISIBILITY shaderVisibility = GetShaderVisibility( stage );

    // Input memory is not owned by the render subsystem (most of the time)
    // which is why we have to reallocate memory to store the bytecode permanently
    void* bytecodeMem = memoryAllocator->allocate( bytecodeSize );
    shader->bytecode.pShaderBytecode = bytecodeMem;
    memcpy( bytecodeMem, bytecode, bytecodeSize );
    shader->bytecode.BytecodeLength = bytecodeSize;

    // Reflect resources bound to the stage
    shader->reflectedResourceCount = 0;

    ID3D12ShaderReflection* reflector = nullptr;
    
    IDxcBlobEncoding* blob = NULL;
    renderContext->dxcLibrary->CreateBlobWithEncodingFromPinned( ( LPBYTE )bytecodeMem, ( UINT32 )bytecodeSize, 0, &blob );

    static constexpr UINT32 DXIL_MAGIC = MakeFourCC( 'D', 'X', 'I', 'L' );

    UINT32 shaderIdx;
    renderContext->dxcContainerReflection->Load( blob );
    renderContext->dxcContainerReflection->FindFirstPartKind( DXIL_MAGIC, &shaderIdx );

    HRESULT reflectionResult = renderContext->dxcContainerReflection->GetPartReflection( shaderIdx, IID_PPV_ARGS( &reflector ) );
    DUSK_ASSERT( reflectionResult == S_OK, "Shader Reflection FAILED! (error code 0x%x)", reflectionResult )
    
    blob->Release();

    D3D12_SHADER_DESC shaderDesc;
    reflector->GetDesc( &shaderDesc );

    for ( UINT i = 0; i < shaderDesc.BoundResources; i++ ) {
        D3D12_SHADER_INPUT_BIND_DESC shaderInputDesc;
        reflector->GetResourceBindingDesc( i, &shaderInputDesc );

        auto& reflectedRes = shader->resourcesReflectionInfos[shader->reflectedResourceCount++];
        reflectedRes.name = shaderInputDesc.Name;
        reflectedRes.spaceIndex = shaderInputDesc.Space;
        reflectedRes.bindingIndex = shaderInputDesc.BindPoint;
        reflectedRes.shaderVisibility = shaderVisibility;

        switch ( shaderInputDesc.Type ) {
        case D3D_SIT_CBUFFER:
            reflectedRes.type = D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_CBV;
            break;
        case D3D_SIT_TEXTURE:
        case D3D_SIT_TBUFFER:
        case D3D_SIT_STRUCTURED:
            reflectedRes.type = D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_SRV;
            break;
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
            reflectedRes.type = D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_UAV;
            break;

        case D3D_SIT_SAMPLER:
        default:
            break;
        }
    }

    return shader;
}

void RenderDevice::destroyShader( Shader* shader )
{
    dk::core::free( memoryAllocator, const_cast< void* >( shader->bytecode.pShaderBytecode ) );
    dk::core::free( memoryAllocator, shader );
}

// Pipeline State Stuff
void FillInputAssemblyDesc( const PipelineStateDesc& description, D3D12_INPUT_ELEMENT_DESC* iaDesc, i32& iaCount )
{
    for ( iaCount = 0; iaCount < 8; iaCount++ ) {
        if ( description.InputLayout.Entry[iaCount].semanticName == nullptr ) {
            break;
        }

        iaDesc[iaCount].SemanticName = description.InputLayout.Entry[iaCount].semanticName;
        iaDesc[iaCount].InputSlot = description.InputLayout.Entry[iaCount].vertexBufferIndex;
        iaDesc[iaCount].SemanticIndex = description.InputLayout.Entry[iaCount].index;
        iaDesc[iaCount].Format = static_cast< DXGI_FORMAT >( description.InputLayout.Entry[iaCount].format );
        iaDesc[iaCount].AlignedByteOffset = ( description.InputLayout.Entry[iaCount].needPadding )
            ? D3D12_APPEND_ALIGNED_ELEMENT
            : description.InputLayout.Entry[iaCount].offsetInBytes;
        iaDesc[iaCount].InputSlotClass = ( description.InputLayout.Entry[iaCount].instanceCount == 0 )
            ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
            : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
        iaDesc[iaCount].InstanceDataStepRate = description.InputLayout.Entry[iaCount].instanceCount;
    }
}

void FillRasterizerStateDesc( D3D12_RASTERIZER_DESC& nativeRasterState, const RasterizerStateDesc& rasterizerState )
{
    nativeRasterState.FillMode = FILL_MODE_LUT[rasterizerState.FillMode];
    nativeRasterState.CullMode = CULL_MODE_LUT[rasterizerState.CullMode];
    nativeRasterState.FrontCounterClockwise = rasterizerState.UseTriangleCCW;
    nativeRasterState.DepthBias = static_cast< INT >( rasterizerState.DepthBias );
    nativeRasterState.DepthBiasClamp = rasterizerState.DepthBiasClamp;
    nativeRasterState.SlopeScaledDepthBias = rasterizerState.SlopeScale;
    nativeRasterState.DepthClipEnable = TRUE;
    nativeRasterState.MultisampleEnable = FALSE;
    nativeRasterState.AntialiasedLineEnable = FALSE;
    nativeRasterState.ForcedSampleCount = 0;
    nativeRasterState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

// NOTE We need to provide the pipeline state descriptor since the blend state will implicitly set the sample mask of the pipeline
void FillBlendStateDesc( D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc, const BlendStateDesc& blendState )
{
    psoDesc.SampleMask = 0xffffffff;

    psoDesc.BlendState.AlphaToCoverageEnable = blendState.EnableAlphaToCoverage;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;

    memset( psoDesc.BlendState.RenderTarget, 0, sizeof( D3D12_RENDER_TARGET_BLEND_DESC ) * 8 );

    for ( i32 i = 0; i < 8; i++ ) {
        psoDesc.BlendState.RenderTarget[i].BlendEnable = blendState.EnableBlend;
        psoDesc.BlendState.RenderTarget[i].SrcBlend = BLEND_SOURCE_LUT[blendState.BlendConfColor.Source];
        psoDesc.BlendState.RenderTarget[i].DestBlend = BLEND_SOURCE_LUT[blendState.BlendConfColor.Destination];
        psoDesc.BlendState.RenderTarget[i].BlendOp = BLEND_OPERATION_LUT[blendState.BlendConfColor.Operation];
        psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = BLEND_SOURCE_LUT[blendState.BlendConfAlpha.Source];
        psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = BLEND_SOURCE_LUT[blendState.BlendConfAlpha.Destination];
        psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = BLEND_OPERATION_LUT[blendState.BlendConfAlpha.Operation];
        psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
}

void FillDepthStencilStateDesc( D3D12_DEPTH_STENCIL_DESC& nativeDsState, const DepthStencilStateDesc& dsState )
{
    nativeDsState = {
        static_cast< BOOL >( dsState.EnableDepthTest ),
        ( dsState.EnableDepthWrite ) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
        COMPARISON_FUNCTION_LUT[dsState.DepthComparisonFunc],
        static_cast< BOOL >( dsState.EnableStencilTest ),
        dsState.StencilReadMask,							            //      UINT8 StencilReadMask
        dsState.StencilWriteMask,							            //      UINT8 StencilWriteMask

        // D3D12_DEPTH_STENCILOP_DESC FrontFace
        {
            STENCIL_OPERATION_LUT[dsState.Front.FailOperation],		        //		D3D12_STENCIL_OP StencilFailOp
            STENCIL_OPERATION_LUT[dsState.Front.ZFailOperation],		        //		D3D12_STENCIL_OP StencilDepthFailOp
            STENCIL_OPERATION_LUT[dsState.Front.PassOperation],		        //		D3D12_STENCIL_OP StencilPassOp
            COMPARISON_FUNCTION_LUT[dsState.Front.ComparisonFunction],	    //		D3D12_COMPARISON_FUNC StencilFunc
        },

        // D3D12_DEPTH_STENCILOP_DESC BackFace
        {
            STENCIL_OPERATION_LUT[dsState.Back.FailOperation],		            //		D3D12_STENCIL_OP StencilFailOp
            STENCIL_OPERATION_LUT[dsState.Back.ZFailOperation],		        //		D3D12_STENCIL_OP StencilDepthFailOp
            STENCIL_OPERATION_LUT[dsState.Back.PassOperation],		            //		D3D12_STENCIL_OP StencilPassOp
            COMPARISON_FUNCTION_LUT[dsState.Back.ComparisonFunction],	    //		D3D12_COMPARISON_FUNC StencilFunc
        },
    };
}

void FillStageRootDescriptor( Shader* shaderStage, D3D12_ROOT_PARAMETER rootParameters[256], UINT& parameterCount, UINT& cbvCount, UINT& srvCount, UINT& uavCount )
{
    if ( shaderStage == nullptr ) {
        return;
    }

    for ( u32 i = 0; i < shaderStage->reflectedResourceCount; i++ ) {
        const auto& reflectedRes = shaderStage->resourcesReflectionInfos[i];    
        //const u32 paramListIdx = ( parameterCount + i );

        //rootParameters[paramListIdx].ShaderVisibility = reflectedRes.shaderVisibility;
        //rootParameters[paramListIdx].ParameterType = reflectedRes.type;
        //rootParameters[paramListIdx].Descriptor.RegisterSpace = reflectedRes.spaceIndex;
        //rootParameters[paramListIdx].Descriptor.ShaderRegister = reflectedRes.bindingIndex;

        if ( reflectedRes.type == D3D12_ROOT_PARAMETER_TYPE_CBV ) cbvCount++;
        else if ( reflectedRes.type == D3D12_ROOT_PARAMETER_TYPE_SRV ) srvCount++;
        else if ( reflectedRes.type == D3D12_ROOT_PARAMETER_TYPE_UAV ) uavCount++;
    }

    // parameterCount += shaderStage->reflectedResourceCount;
}

PipelineState* RenderDevice::createPipelineState( const PipelineStateDesc& description )
{
    PipelineState* pipelineState = dk::core::allocate<PipelineState>( memoryAllocator );
    pipelineState->primitiveTopology = PRIMITIVE_TOPOLOGY_LUT[description.PrimitiveTopology];

    switch ( description.ColorClearValue ) {
    case PipelineStateDesc::CLEAR_COLOR_WHITE:
        pipelineState->colorClearValue[0] = 1.0f;
        pipelineState->colorClearValue[1] = 1.0f;
        pipelineState->colorClearValue[2] = 1.0f; 
        pipelineState->colorClearValue[3] = 1.0f;
        break;
    case PipelineStateDesc::CLEAR_COLOR_BLACK:
        pipelineState->colorClearValue[0] = 0.0f;
        pipelineState->colorClearValue[1] = 0.0f;
        pipelineState->colorClearValue[2] = 0.0f;
        pipelineState->colorClearValue[3] = 0.0f;
        break;
    }

    pipelineState->depthClearValue = description.depthClearValue;
    pipelineState->stencilClearValue = description.stencilClearValue;

     // Create Root Signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // First parameter will always be the descriptor table
    rootSignatureDesc.NumParameters = 1;

    // TODO Tweak those arbitrary values? (no idea if 256 slots is enough and doesn't exceed the max)
    D3D12_ROOT_PARAMETER rootParameters[256];
    D3D12_DESCRIPTOR_RANGE descRanges[100];
    DXGI_FORMAT RTVFormats[8];

    DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
    UINT NumRenderTargets = 0;
    i32 descriptorRangeCount = 0;

    UINT cbvCount = 0;
    UINT srvCount = 0;
    UINT uavCount = 0;

    pipelineState->resourceCount = 0;

    FillStageRootDescriptor( description.vertexShader, rootParameters, rootSignatureDesc.NumParameters, cbvCount, srvCount, uavCount );
    FillStageRootDescriptor( description.tesselationControlShader, rootParameters, rootSignatureDesc.NumParameters, cbvCount, srvCount, uavCount );
    FillStageRootDescriptor( description.tesselationEvalShader, rootParameters, rootSignatureDesc.NumParameters, cbvCount, srvCount, uavCount );
    FillStageRootDescriptor( description.pixelShader, rootParameters, rootSignatureDesc.NumParameters, cbvCount, srvCount, uavCount );

    const FramebufferLayoutDesc& fboLayout = description.FramebufferLayout;
    const u32 rtvCount = fboLayout.getAttachmentCount();
    for ( u32 i = 0; i < rtvCount; i++ ) {
        const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( fboLayout.Attachments[i].viewFormat );

        PipelineState::RTVDesc& rtvDesc = pipelineState->renderTargetViewDesc[i];
        rtvDesc.clearRenderTarget = ( fboLayout.Attachments[i].targetState == FramebufferLayoutDesc::CLEAR );
        rtvDesc.viewFormat = viewFormat;
        rtvDesc.arrayIndex = 0;
        rtvDesc.mipIndex = 0;

        RTVFormats[i] = viewFormat;
    }
    NumRenderTargets = rtvCount;
    
    const bool hasDsv = ( fboLayout.depthStencilAttachment.bindMode != FramebufferLayoutDesc::UNUSED );
    if ( hasDsv ) {
        const DXGI_FORMAT viewFormat = static_cast< DXGI_FORMAT >( fboLayout.depthStencilAttachment.viewFormat );

        PipelineState::RTVDesc& dsvDesc = pipelineState->depthStencilViewDesc;
        dsvDesc.clearRenderTarget = ( fboLayout.depthStencilAttachment.targetState == FramebufferLayoutDesc::CLEAR );
        dsvDesc.viewFormat = viewFormat;
        dsvDesc.arrayIndex = 0;
        dsvDesc.mipIndex = 0;

        DSVFormat = viewFormat;
    }

    pipelineState->rtvCount = rtvCount;
    pipelineState->hasDepthStencilView = hasDsv;
    
    if ( cbvCount != 0 ) {
        D3D12_DESCRIPTOR_RANGE& descRange = descRanges[descriptorRangeCount++];
        descRange.BaseShaderRegister = 0;
        descRange.NumDescriptors = cbvCount;
        descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        descRange.RegisterSpace = 1;
        descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    }

    // Root constant should go here (if we need any)

    if ( srvCount != 0 ) {
        D3D12_DESCRIPTOR_RANGE& srvDescRange = descRanges[descriptorRangeCount++];
        srvDescRange.BaseShaderRegister = 0;
        srvDescRange.NumDescriptors = srvCount;
        srvDescRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        srvDescRange.RegisterSpace = 2;
        srvDescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }

    if ( uavCount != 0 ) {
        D3D12_DESCRIPTOR_RANGE& uavDescRange = descRanges[descriptorRangeCount++];
        uavDescRange.BaseShaderRegister = 0;
        uavDescRange.NumDescriptors = uavCount;
        uavDescRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        uavDescRange.RegisterSpace = 3;
        uavDescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = descriptorRangeCount;
    rootParameters[0].DescriptorTable.pDescriptorRanges = descRanges;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    rootSignatureDesc.pParameters = rootParameters;

    D3D12_STATIC_SAMPLER_DESC samplerDescs[PipelineStateDesc::MAX_STATIC_SAMPLER_COUNT];

    for ( i32 i = 0; i < description.StaticSamplers.StaticSamplerCount; i++ ) {
        D3D12_STATIC_SAMPLER_DESC& nativeSamplerDesc = samplerDescs[i];
        const SamplerDesc& samplerDesc = description.StaticSamplers.StaticSamplersDesc[i];

        nativeSamplerDesc.RegisterSpace = 0;
        nativeSamplerDesc.ShaderRegister = i;
        nativeSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO Nvidia doc say that perfs are (somehow) better with visibilty all set (why tho?)

        nativeSamplerDesc.Filter = FILTER_LUT[samplerDesc.filter];
        nativeSamplerDesc.AddressU = TEXTURE_ADDRESS_LUT[samplerDesc.addressU];
        nativeSamplerDesc.AddressV = TEXTURE_ADDRESS_LUT[samplerDesc.addressV];
        nativeSamplerDesc.AddressW = TEXTURE_ADDRESS_LUT[samplerDesc.addressW];
        nativeSamplerDesc.MipLODBias = 0.0F;

        if ( samplerDesc.filter == SAMPLER_FILTER_ANISOTROPIC_8
            || samplerDesc.filter == SAMPLER_FILTER_COMPARISON_ANISOTROPIC_8 ) {
            nativeSamplerDesc.MaxAnisotropy = 8;
        } else if ( samplerDesc.filter == SAMPLER_FILTER_ANISOTROPIC_16
                    || samplerDesc.filter == SAMPLER_FILTER_COMPARISON_ANISOTROPIC_16 ) {
            nativeSamplerDesc.MaxAnisotropy = 16;
        } else {
            nativeSamplerDesc.MaxAnisotropy = 0;
        }

        nativeSamplerDesc.ComparisonFunc = COMPARISON_FUNCTION_LUT[samplerDesc.comparisonFunction];
        nativeSamplerDesc.BorderColor = ( samplerDesc.borderColor[0] == 0.0f && samplerDesc.borderColor[1] == 0.0f && samplerDesc.borderColor[2] == 0.0f )
            ? D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
            : D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        nativeSamplerDesc.MinLOD = static_cast< FLOAT >( samplerDesc.minLOD );
        nativeSamplerDesc.MaxLOD = static_cast< FLOAT >( samplerDesc.maxLOD );
    }

    rootSignatureDesc.NumStaticSamplers = description.StaticSamplers.StaticSamplerCount;
    rootSignatureDesc.pStaticSamplers = samplerDescs;

    if ( description.PipelineType == PipelineStateDesc::COMPUTE ) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        psoDesc.NodeMask = 0;

        DUSK_RAISE_FATAL_ERROR( description.computeShader != nullptr, "A compute pipeline state must have a valid compute shader set in its descriptor!" );

        psoDesc.CS = description.computeShader->bytecode;

        // Load cached pipeline state from the backing store (if available)
        ID3DBlob* errorBlob;
        if ( description.cachedPsoData != nullptr ) {
            psoDesc.CachedPSO.CachedBlobSizeInBytes = *reinterpret_cast< size_t* >( description.cachedPsoData );
            psoDesc.CachedPSO.pCachedBlob = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t );

            u8* rsSizeOffset = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t ) + psoDesc.CachedPSO.CachedBlobSizeInBytes;
            size_t rootSignatureSize = *rsSizeOffset;
            void* rootSignaturePointer = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t ) + psoDesc.CachedPSO.CachedBlobSizeInBytes + sizeof( size_t );

            renderContext->device->CreateRootSignature( 0, rootSignaturePointer, rootSignatureSize, IID_PPV_ARGS( &pipelineState->rootSignature ) );
        } else {
            psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
            psoDesc.CachedPSO.pCachedBlob = nullptr;

            HRESULT rootSignatureSerialResult = D3D12SerializeRootSignature( &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pipelineState->rootSignatureCache, &errorBlob );
            if ( FAILED( rootSignatureSerialResult ) ) {
                DUSK_LOG_ERROR( "D3D12SerializeRootSignature FAILED : %s\n", static_cast< char* >( errorBlob->GetBufferPointer() ) );
                errorBlob->Release();
            } else {
                renderContext->device->CreateRootSignature( 0, pipelineState->rootSignatureCache->GetBufferPointer(), pipelineState->rootSignatureCache->GetBufferSize(), IID_PPV_ARGS( &pipelineState->rootSignature ) );
            }
        }

        psoDesc.pRootSignature = pipelineState->rootSignature;


        HRESULT operationResult = renderContext->device->CreateComputePipelineState( &psoDesc, IID_PPV_ARGS( &pipelineState->pso ) );
       
        DUSK_ASSERT( SUCCEEDED( operationResult ), "CreateComputePipelineState FAILED! (error code: 0x%x)\n", operationResult );
    } else if ( description.PipelineType == PipelineStateDesc::GRAPHICS ) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;    
        psoDesc.NodeMask = 0;

        memset( psoDesc.RTVFormats, 0, sizeof( DXGI_FORMAT ) * 8 );
        memcpy( psoDesc.RTVFormats, RTVFormats, sizeof( DXGI_FORMAT ) * NumRenderTargets );
        psoDesc.NumRenderTargets = NumRenderTargets;
        psoDesc.DSVFormat = DSVFormat;

        // Input Assembly
        D3D12_INPUT_ELEMENT_DESC iaDesc[PipelineStateDesc::MAX_INPUT_LAYOUT_ENTRY_COUNT];
        i32 iaCount = 0;
        FillInputAssemblyDesc( description, static_cast< D3D12_INPUT_ELEMENT_DESC*>( iaDesc ), iaCount );        
        psoDesc.InputLayout.NumElements = iaCount;
        psoDesc.InputLayout.pInputElementDescs = iaDesc;
        psoDesc.PrimitiveTopologyType = PRIMITIVE_TOPOLOGY_TYPE_LUT[description.PrimitiveTopology];

        // Pipeline Stages
#define DUSK_BIND_IF_AVAILABLE( stage, shader )\
        if ( shader != nullptr ) {\
            psoDesc.##stage = shader->bytecode;\
        } else {\
            psoDesc.##stage.pShaderBytecode = nullptr;\
            psoDesc.##stage.BytecodeLength = 0;\
        }

        DUSK_BIND_IF_AVAILABLE( VS, description.vertexShader );
        DUSK_BIND_IF_AVAILABLE( HS, description.tesselationControlShader );
        DUSK_BIND_IF_AVAILABLE( DS, description.tesselationEvalShader );
        DUSK_BIND_IF_AVAILABLE( PS, description.pixelShader );

#undef DUSK_BIND_IF_AVAILABLE

        // TODO Support geometry stage (for debug features only)
        psoDesc.GS.pShaderBytecode = nullptr;
        psoDesc.GS.BytecodeLength = 0;

        // Rasterizer State
        FillRasterizerStateDesc( psoDesc.RasterizerState, description.RasterizerState );

        // Blend State
        FillBlendStateDesc( psoDesc, description.BlendState );

        // DepthStencil State
        FillDepthStencilStateDesc( psoDesc.DepthStencilState, description.DepthStencilState );

        psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        psoDesc.SampleDesc.Count = description.samplerCount;
        psoDesc.SampleDesc.Quality = ( description.samplerCount > 1 ) ? DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN : 0;

        // Resource List Layout

        if ( psoDesc.InputLayout.NumElements != 0 )
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        if ( psoDesc.VS.pShaderBytecode == nullptr )
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

        if ( psoDesc.HS.pShaderBytecode == nullptr )
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

        if ( psoDesc.DS.pShaderBytecode == nullptr )
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

        if ( psoDesc.PS.pShaderBytecode == nullptr )
            rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        // Load cached pipeline state from the backing store (if available)
        ID3DBlob* errorBlob;
        if ( description.cachedPsoData != nullptr ) {
            psoDesc.CachedPSO.CachedBlobSizeInBytes = *reinterpret_cast< size_t* >( description.cachedPsoData );
            psoDesc.CachedPSO.pCachedBlob = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t );

            u8* rsSizeOffset = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t ) + psoDesc.CachedPSO.CachedBlobSizeInBytes;
            size_t rootSignatureSize = *rsSizeOffset;
            void* rootSignaturePointer = reinterpret_cast< u8* >( description.cachedPsoData ) + sizeof( size_t ) + psoDesc.CachedPSO.CachedBlobSizeInBytes + sizeof( size_t );

            renderContext->device->CreateRootSignature( 0, rootSignaturePointer, rootSignatureSize, IID_PPV_ARGS( &pipelineState->rootSignature ) );
        } else {
            psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
            psoDesc.CachedPSO.pCachedBlob = nullptr;

            HRESULT rootSignatureSerialResult = D3D12SerializeRootSignature( &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pipelineState->rootSignatureCache, &errorBlob );
            if ( FAILED( rootSignatureSerialResult ) ) {
                // Somehow, the error blob might be invalid even if the serialization failed...
                if ( errorBlob != nullptr ) {
                    DUSK_LOG_ERROR( "D3D12SerializeRootSignature FAILED : %s\n", static_cast< char* >( errorBlob->GetBufferPointer() ) );
                    errorBlob->Release();
                }
            } else {
                renderContext->device->CreateRootSignature( 0, pipelineState->rootSignatureCache->GetBufferPointer(), pipelineState->rootSignatureCache->GetBufferSize(), IID_PPV_ARGS( &pipelineState->rootSignature ) );
            }
        }

        psoDesc.pRootSignature = pipelineState->rootSignature;

        // Unused for now
        psoDesc.StreamOutput.NumEntries = 0;
        psoDesc.StreamOutput.NumStrides = 0;
        psoDesc.StreamOutput.pBufferStrides = nullptr;
        psoDesc.StreamOutput.pSODeclaration = nullptr;
        psoDesc.StreamOutput.RasterizedStream = 0;

        HRESULT operationResult = renderContext->device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &pipelineState->pso ) );
        DUSK_ASSERT( SUCCEEDED( operationResult ), "CreateGraphicsPipelineState FAILED! (error code: 0x%x)\n", operationResult );
    }

    return pipelineState;
}

void CommandList::prepareAndBindResourceList()
{
    //u32 srvIndex = 0;
   

    //const i32 internalFrameIdx = ( frameIndex % PENDING_FRAME_COUNT );
    //const size_t descriptorsStartOffset = renderContext->srvDescriptorHeapOffset[internalFrameIdx];

    //ID3D12GraphicsCommandList* cmdList = commandList.getNativeCommandList()->graphicsCmdList;

    //cmdList->SetDescriptorHeaps( 1, &renderContext->srvDescriptorHeap );

    //    case PipelineState::InternalResourceType::RESOURCE_TYPE_CBUFFER_VIEW: {
    //        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    //        cbvDesc.BufferLocation = renderContext->volatileBuffers[internalFrameIdx]->GetGPUVirtualAddress();
    //        cbvDesc.SizeInBytes = resourceList.resources[i].buffer->size;

    //        D3D12_CPU_DESCRIPTOR_HANDLE heapDescriptorOffset = renderContext->srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    //        heapDescriptorOffset.ptr += renderContext->srvDescriptorHeapOffset[internalFrameIdx];
    //        renderContext->device->CreateConstantBufferView( &cbvDesc, heapDescriptorOffset );
    //        
    //        renderContext->srvDescriptorHeapOffset[internalFrameIdx] += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    //        // cmdList->SetGraphicsRootConstantBufferView( i, renderContext->volatileBuffers[internalFrameIdx]->GetGPUVirtualAddress() );
    //    } break;
    //        
    //    case PipelineState::InternalResourceType::RESOURCE_TYPE_SHADER_RESOURCE_VIEW_IMAGE: {
    //        Image* image = resourceList.resources[i].image;

    //        const PipelineState::RTVDesc& psoRtvDesc = pipelineState.resourceViewDesc[srvIndex++];

    //        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    //        srvDesc.Format = ( psoRtvDesc.viewFormat == DXGI_FORMAT_UNKNOWN ) ? image->defaultFormat : psoRtvDesc.viewFormat;

    //        switch ( image->dimension ) {
    //        case ImageDesc::DIMENSION_1D:
    //        {
    //            if ( image->arraySize > 1 ) {
    //                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    //            } else {
    //                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
    //            }
    //        } break;
    //        case ImageDesc::DIMENSION_2D:
    //        {
    //            // TODO Handle cubemap SRV ( check misc bitfield )
    //            if ( image->samplerCount > 1 ) {
    //                if ( image->arraySize > 1 ) {
    //                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    //                } else {
    //                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    //                }
    //            } else {
    //                if ( image->arraySize > 1 ) {
    //                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    //                } else {
    //                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    //                    srvDesc.Texture2D.MipLevels = image->mipCount;
    //                    srvDesc.Texture2D.MostDetailedMip = 0;
    //                    srvDesc.Texture2D.PlaneSlice = 0;
    //                    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    //                }
    //            }
    //        } break;
    //        case ImageDesc::DIMENSION_3D:
    //        {
    //            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    //            srvDesc.Texture3D.MostDetailedMip = 0;
    //            srvDesc.Texture3D.MipLevels = image->mipCount;
    //        } break;
    //        }

    //        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    //        // Allocate descriptor from the heap
    //        D3D12_CPU_DESCRIPTOR_HANDLE heapDescriptorOffset = renderContext->srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    //        heapDescriptorOffset.ptr += renderContext->srvDescriptorHeapOffset[internalFrameIdx];

    //        renderContext->device->CreateShaderResourceView( image->resource[internalFrameIdx], &srvDesc, heapDescriptorOffset );

    //        renderContext->srvDescriptorHeapOffset[internalFrameIdx] += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    //        //D3D12_GPU_DESCRIPTOR_HANDLE heapDescriptorOffset = ;
    //        //heapDescriptorOffset.ptr += resourceList.resources[i].image->shaderResourceView[VIEW_FORMAT_BC3_UNORM]; // TODO TMP TEST
    //    } break;

    //    case PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW_IMAGE:
    //    {
    //        Image* image = resourceList.resources[i].image;

    //        const PipelineState::RTVDesc& psoRtvDesc = pipelineState.resourceViewDesc[srvIndex++];

    //        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    //        uavDesc.Format = image->defaultFormat;
    //        switch ( image->dimension ) {
    //        case ImageDesc::DIMENSION_1D:
    //        {
    //            if ( image->arraySize > 1 ) {
    //                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
    //                uavDesc.Texture1DArray.MipSlice = psoRtvDesc.mipIndex;
    //                uavDesc.Texture1DArray.ArraySize = 1;
    //                uavDesc.Texture1DArray.FirstArraySlice = psoRtvDesc.arrayIndex;
    //            } else {
    //                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
    //                uavDesc.Texture1D.MipSlice = psoRtvDesc.mipIndex;
    //            }
    //        } break;
    //        case ImageDesc::DIMENSION_2D:
    //        {
    //            if ( image->arraySize > 1 ) {
    //                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    //                uavDesc.Texture2DArray.MipSlice = psoRtvDesc.mipIndex;
    //                uavDesc.Texture2DArray.ArraySize = 1;
    //                uavDesc.Texture2DArray.PlaneSlice = 0;
    //                uavDesc.Texture2DArray.FirstArraySlice = psoRtvDesc.arrayIndex;
    //            } else {
    //                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    //                uavDesc.Texture2D.MipSlice = psoRtvDesc.mipIndex;
    //                uavDesc.Texture2D.PlaneSlice = 0;
    //            }
    //        } break;
    //        case ImageDesc::DIMENSION_3D:
    //        {
    //            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    //            uavDesc.Texture3D.MipSlice = psoRtvDesc.mipIndex;
    //            uavDesc.Texture3D.FirstWSlice = psoRtvDesc.arrayIndex;
    //            uavDesc.Texture3D.WSize = 1;
    //        } break;
    //        }

    //        // Allocate descriptor from the heap
    //        D3D12_CPU_DESCRIPTOR_HANDLE heapDescriptorOffset = renderContext->srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    //        heapDescriptorOffset.ptr += renderContext->srvDescriptorHeapOffset[internalFrameIdx];

    //        renderContext->device->CreateUnorderedAccessView( image->resource[internalFrameIdx], nullptr, &uavDesc, heapDescriptorOffset );

    //        renderContext->srvDescriptorHeapOffset[internalFrameIdx] += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
    //    } break;

    //    case PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW_BUFFER:
    //    {
    //        const PipelineState::RTVDesc& psoRtvDesc = pipelineState.resourceViewDesc[srvIndex++];

    //        Buffer* buffer = resourceList.resources[i].buffer;

    //        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    //        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    //        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    //        
    //        uavDesc.Buffer.CounterOffsetInBytes = 0;
    //        uavDesc.Buffer.FirstElement = 0;
    //        uavDesc.Buffer.StructureByteStride = buffer->size / buffer->stride;
    //        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    //        uavDesc.Buffer.NumElements = buffer->stride;

    //        // Allocate descriptor from the heap
    //        D3D12_CPU_DESCRIPTOR_HANDLE heapDescriptorOffset = renderContext->srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    //        heapDescriptorOffset.ptr += renderContext->srvDescriptorHeapOffset[internalFrameIdx];

    //        renderContext->device->CreateUnorderedAccessView( buffer->resource[internalFrameIdx], nullptr, &uavDesc, heapDescriptorOffset );

    //        renderContext->srvDescriptorHeapOffset[internalFrameIdx] += renderContext->device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
    //    } break;
    //    }
    //}

    //// Bind descriptor table once the resources view have been created
    //D3D12_GPU_DESCRIPTOR_HANDLE descriptorTable = renderContext->srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    //descriptorTable.ptr += descriptorsStartOffset;

    //if ( commandList.getCommandListType() == CommandList::COMPUTE ) {
    //    cmdList->SetComputeRootDescriptorTable( 0, descriptorTable );
    //} else {
    //    cmdList->SetGraphicsRootDescriptorTable( 0, descriptorTable );

    //    // Initialize / Clear render buffers
    //    for ( u32 i = 0; i < clearRenderTargetCount; i++ ) {
    //        cmdList->ClearRenderTargetView( clearRtvs[i], pipelineState.colorClearValue, 1, &clearRectangles[i] );
    //    }

    //    if ( shouldClearDsv ) {
    //        cmdList->ClearDepthStencilView( dsv, D3D12_CLEAR_FLAG_DEPTH, pipelineState.depthClearValue, pipelineState.stencilClearValue, 1, &clearDepthRectangle );
    //    }

    //    cmdList->OMSetRenderTargets( renderTargetCount, rtvs, FALSE, ( hasDsv ) ? &dsv : nullptr );
    //}
}

void RenderDevice::destroyPipelineState( PipelineState* pipelineState )
{
#define DUSK_RELEASE_IF_AVAILABLE( x )\
    if ( x != nullptr ) {\
        x->Release();\
        x = nullptr;\
    }

    DUSK_RELEASE_IF_AVAILABLE( pipelineState->pso );
    DUSK_RELEASE_IF_AVAILABLE( pipelineState->rootSignature );
    DUSK_RELEASE_IF_AVAILABLE( pipelineState->rootSignatureCache );

#undef DUSK_RELEASE_IF_AVAILABLE

    destroyPipelineStateCache( pipelineState );

    dk::core::free( memoryAllocator, pipelineState );
}

void RenderDevice::getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize )
{
    ID3D10Blob* psoCache = nullptr;
    pipelineState->pso->GetCachedBlob( &psoCache );
    
    size_t psoCacheSize = psoCache->GetBufferSize();
    size_t rootSignatureSize = pipelineState->rootSignatureCache->GetBufferSize();

    // We cache both pso and root signature in the same binary blob
    // First PSO binary; then root signature size and then the root signature
    // This is D3D12 specific so it has to be done at low level (to avoid API specific code at higher level)
    // NOTE The pso binary size will be written at higher level
    binaryDataSize = ( psoCacheSize + rootSignatureSize + sizeof( size_t ) * 2 );

    // TODO Might need to be bigger once asynchronous pso caching/fetching is implemented (to allow multi threaded allocations)
    // TODO It could be interesting to store root signature cache as individual binaries (since PSO might share the same signature...)
    *binaryData = pipelineStateCacheAllocator->allocate( binaryDataSize );

    // PSO Binary
    memcpy( *binaryData, &psoCacheSize, sizeof( size_t ) );
    memcpy( static_cast< u8* >( *binaryData ) + sizeof( size_t ), psoCache->GetBufferPointer(), psoCacheSize );

    // Root signature binary
    memcpy( static_cast< u8* >( *binaryData ) + sizeof( size_t ) + psoCacheSize, &rootSignatureSize, sizeof( size_t ) );
    memcpy( static_cast< u8* >( *binaryData ) + sizeof( size_t ) * 2 + psoCacheSize, pipelineState->rootSignatureCache->GetBufferPointer(), rootSignatureSize );

    psoCache->Release();
}

void RenderDevice::destroyPipelineStateCache( PipelineState* pipelineState )
{
    pipelineStateCacheAllocator->clear();
}

void CommandList::begin()
{
    ID3D12GraphicsCommandList* cmdList = nativeCommandList->graphicsCmdList;

    PipelineState* pipelineState = nativeCommandList->BindedPipelineState;

    if ( pipelineState != nullptr ) {
        cmdList->Reset( *nativeCommandList->allocator, pipelineState->pso );

        if ( commandListType == CommandList::Type::GRAPHICS ) {
            cmdList->IASetPrimitiveTopology( pipelineState->primitiveTopology );
            cmdList->SetGraphicsRootSignature( pipelineState->rootSignature );
        } else {
            cmdList->SetComputeRootSignature( pipelineState->rootSignature );
        }
    } else {
        cmdList->Reset( *nativeCommandList->allocator, nullptr );
    }
}

void CommandList::bindPipelineState( PipelineState* pipelineState )
{
    nativeCommandList->BindedPipelineState = pipelineState;
}

void CommandList::bindConstantBuffer( const dkStringHash_t hashcode, Buffer* buffer )
{

}

void CommandList::bindImage( const dkStringHash_t hashcode, Image* image, const ImageViewDesc viewDescription )
{

}

void CommandList::bindBuffer( const dkStringHash_t hashcode, Buffer* buffer, const eViewFormat viewFormat )
{

}

void CommandList::bindSampler( const dkStringHash_t hashcode, Sampler* sampler )
{

}
#endif
