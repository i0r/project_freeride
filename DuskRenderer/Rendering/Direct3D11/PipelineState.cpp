/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#include <Shared.h>

#if DUSK_D3D11
#include <Rendering/RenderDevice.h>
#include <Rendering/CommandList.h>

#include "RenderDevice.h"
#include "CommandList.h"
#include "ComparisonFunctions.h"
#include "Sampler.h"
#include "Buffer.h"
#include "Image.h"
#include "PipelineState.h"
#include "ResourceAllocationHelpers.h"

#include "PipelineState.h"

#include <Maths/Helpers.h>

#include <d3d11.h>
#include <d3dcompiler.h>

struct Shader
{
    // We need to keep the bytecode somewhere in order to create the IA signature
    void*                       bytecode;
    size_t                      bytecodeSize;

    union {
        ID3D11VertexShader*     vertexShader;
        ID3D11HullShader*       tesselationControlShader;
        ID3D11DomainShader*     tesselationEvalShader;
        ID3D11PixelShader*      pixelShader;
        ID3D11ComputeShader*    computeShader;
    };
    
    // Misc Infos
    struct
    {
        std::string name;
        u32 bindingIndex;

        PipelineState::InternalResourceType type;
        size_t resourceSize;

        struct {
            size_t offset;
            size_t range;
        } rangeBinding;
    } resourcesReflectionInfos[96];

    u32 reflectedResourceCount;
};

Shader* RenderDevice::createShader( const eShaderStage stage, const void* bytecode, const size_t bytecodeSize )
{
    ID3D11Device* nativeDevice = renderContext->PhysicalDevice;
    HRESULT operationResult = S_OK;

    Shader* shader = dk::core::allocate<Shader>( memoryAllocator );
    shader->bytecode = nullptr;
    shader->bytecodeSize = 0;

    switch ( stage ) {
    case SHADER_STAGE_VERTEX:
        operationResult = nativeDevice->CreateVertexShader( bytecode, bytecodeSize, NULL, &shader->vertexShader );

        // Needed for input layout creation
        shader->bytecode = dk::core::allocateArray<uint8_t>( memoryAllocator, bytecodeSize );
        shader->bytecodeSize = bytecodeSize;

        memcpy( shader->bytecode, bytecode, bytecodeSize * sizeof( uint8_t ) );
        break;

    case SHADER_STAGE_PIXEL:
        operationResult = nativeDevice->CreatePixelShader( bytecode, bytecodeSize, NULL, &shader->pixelShader );
        break;

    case SHADER_STAGE_TESSELATION_CONTROL:
        operationResult = nativeDevice->CreateHullShader( bytecode, bytecodeSize, NULL, &shader->tesselationControlShader );
        break;

    case SHADER_STAGE_TESSELATION_EVALUATION:
        operationResult = nativeDevice->CreateDomainShader( bytecode, bytecodeSize, NULL, &shader->tesselationEvalShader );
        break;

    case SHADER_STAGE_COMPUTE:
        operationResult = nativeDevice->CreateComputeShader( bytecode, bytecodeSize, NULL, &shader->computeShader );
        break;
    }

    if ( FAILED( operationResult ) ) {
        DUSK_LOG_ERROR( "Failed to load precompiled shader (error code: 0x%x)\n", operationResult );
        return nullptr;
    }

    ID3D11ShaderReflection* reflector = nullptr;
    HRESULT opResult = D3DReflect( bytecode, bytecodeSize, IID_PPV_ARGS( &reflector ) );
    DUSK_ASSERT( SUCCEEDED( opResult ), "Shader Reflection FAILED! (error code 0x%x)", opResult )

    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc( &shaderDesc );

    for ( UINT i = 0; i < shaderDesc.BoundResources; i++ ) {
        D3D11_SHADER_INPUT_BIND_DESC shaderInputDesc;
        reflector->GetResourceBindingDesc( i, &shaderInputDesc );

        // TODO Support dynamic/static sampler (right now we assume that any sampler bound to a shader stage is static
        if ( shaderInputDesc.Type == D3D_SIT_SAMPLER ) {
            continue;
        }

        auto& reflectedRes = shader->resourcesReflectionInfos[shader->reflectedResourceCount++];
        reflectedRes.name = shaderInputDesc.Name;
        reflectedRes.bindingIndex = shaderInputDesc.BindPoint;

        switch ( shaderInputDesc.Type ) {
        case D3D_SIT_CBUFFER:
            reflectedRes.type = PipelineState::InternalResourceType::RESOURCE_TYPE_CBUFFER_VIEW;
            break;

        case D3D_SIT_TEXTURE:
        case D3D_SIT_TBUFFER:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
            reflectedRes.type = PipelineState::InternalResourceType::RESOURCE_TYPE_SHADER_RESOURCE_VIEW;
            break;

        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
        case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            reflectedRes.type = PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW;
            break;
        }
    }

    reflector->Release();

    return shader;
}

void RenderDevice::destroyShader( Shader* shader )
{
    if ( shader->vertexShader != nullptr ) {
        shader->vertexShader->Release();
        shader->vertexShader = nullptr;
    }

    if ( shader->bytecode != nullptr ) {
        dk::core::freeArray<uint8_t>( memoryAllocator, ( uint8_t* )shader->bytecode );
    }

    dk::core::free( memoryAllocator, shader );
}

static constexpr D3D11_PRIMITIVE_TOPOLOGY _D3D11_PRIMITIVE_TOPOLOGY[ePrimitiveTopology::PRIMITIVE_TOPOLOGY_COUNT] =
{
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
};

static constexpr D3D11_BLEND D3D11_BLEND_SOURCE[eBlendSource::BLEND_SOURCE_COUNT] =
{
    D3D11_BLEND_ZERO,
    D3D11_BLEND_ONE,

    D3D11_BLEND_SRC_COLOR,
    D3D11_BLEND_INV_SRC_COLOR,

    D3D11_BLEND_SRC_ALPHA,
    D3D11_BLEND_INV_SRC_ALPHA,

    D3D11_BLEND_DEST_ALPHA,
    D3D11_BLEND_INV_DEST_ALPHA,

    D3D11_BLEND_DEST_COLOR,
    D3D11_BLEND_INV_DEST_COLOR,

    D3D11_BLEND_SRC_ALPHA_SAT,

    D3D11_BLEND_BLEND_FACTOR,
    D3D11_BLEND_INV_BLEND_FACTOR,
};

static constexpr D3D11_BLEND_OP D3D11_BLEND_OPERATION[eBlendOperation::BLEND_OPERATION_COUNT] =
{
    D3D11_BLEND_OP_ADD,
    D3D11_BLEND_OP_SUBTRACT,
    D3D11_BLEND_OP_MIN,
    D3D11_BLEND_OP_MAX,
};

static constexpr D3D11_STENCIL_OP D3D111_STENCIL_OPERATION[eStencilOperation::STENCIL_OPERATION_COUNT] =
{
    D3D11_STENCIL_OP_KEEP,
    D3D11_STENCIL_OP_ZERO,
    D3D11_STENCIL_OP_REPLACE,
    D3D11_STENCIL_OP_INCR,
    D3D11_STENCIL_OP_INCR_SAT,
    D3D11_STENCIL_OP_DECR,
    D3D11_STENCIL_OP_DECR_SAT,
    D3D11_STENCIL_OP_INVERT
};

static constexpr D3D11_FILL_MODE D3D11_FM[eFillMode::FILL_MODE_COUNT] =
{
    D3D11_FILL_SOLID,
    D3D11_FILL_WIREFRAME,
};

static constexpr D3D11_CULL_MODE D3D11_CM[eCullMode::CULL_MODE_COUNT] =
{
    D3D11_CULL_NONE,
    D3D11_CULL_FRONT,
    D3D11_CULL_BACK
};

ID3D11InputLayout* CreateInputLayout( ID3D11Device* nativeDevice, const InputLayoutEntry* inputLayout, const void* shaderBytecode, const size_t shaderBytecodeSize )
{
    D3D11_INPUT_ELEMENT_DESC layoutDescription[8];

    u32 i = 0;
    for ( ; i < 8; i++ ) {
        if ( inputLayout[i].semanticName == nullptr ) {
            break;
        }

        layoutDescription[i] = {
            inputLayout[i].semanticName,
            inputLayout[i].index,
            static_cast< DXGI_FORMAT >( inputLayout[i].format ),
            inputLayout[i].vertexBufferIndex,
            ( inputLayout[i].needPadding ) ? D3D11_APPEND_ALIGNED_ELEMENT : inputLayout[i].offsetInBytes,
            ( inputLayout[i].instanceCount == 0 ) ? D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA,
            inputLayout[i].instanceCount
        };
    }

    if ( i == 0 ) {
        return nullptr;
    }

    ID3D11InputLayout* inputLayoutObject = nullptr;
    nativeDevice->CreateInputLayout( layoutDescription, i, shaderBytecode, ( UINT )shaderBytecodeSize, &inputLayoutObject );

    return inputLayoutObject;
}

ID3D11BlendState* CreateBlendState( ID3D11Device* nativeDevice, const BlendStateDesc& description )
{
    if ( !description.EnableBlend ) {
        return nullptr;
    }

    D3D11_BLEND_DESC blendDesc = { 0 };
    blendDesc.AlphaToCoverageEnable = description.EnableAlphaToCoverage;
    blendDesc.IndependentBlendEnable = FALSE;

    for ( i32 i = 0; i < 8; i++ ) {
        blendDesc.RenderTarget[i].BlendEnable = description.EnableBlend;
        blendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_SOURCE[description.BlendConfColor.Source];
        blendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_SOURCE[description.BlendConfColor.Destination];
        blendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OPERATION[description.BlendConfColor.Operation];
        blendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_SOURCE[description.BlendConfAlpha.Source];
        blendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_SOURCE[description.BlendConfAlpha.Destination];
        blendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OPERATION[description.BlendConfAlpha.Operation];
        blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    ID3D11BlendState* blendState;
    nativeDevice->CreateBlendState( &blendDesc, &blendState );
    return blendState;
}

ID3D11DepthStencilState* CreateDepthStencilState( ID3D11Device* nativeDevice, const DepthStencilStateDesc& description )
{
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable = description.EnableDepthTest;
    depthStencilDesc.DepthWriteMask = ( description.EnableDepthWrite ) 
        ? D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL
        : D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_FUNCTION[description.DepthComparisonFunc];
    depthStencilDesc.StencilEnable = description.EnableStencilTest;
    depthStencilDesc.StencilReadMask = description.StencilReadMask;
    depthStencilDesc.StencilWriteMask = description.StencilWriteMask;

    depthStencilDesc.FrontFace.StencilFailOp = D3D111_STENCIL_OPERATION[description.Front.FailOperation];
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D111_STENCIL_OPERATION[description.Front.ZFailOperation];
    depthStencilDesc.FrontFace.StencilPassOp = D3D111_STENCIL_OPERATION[description.Front.PassOperation];
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_FUNCTION[description.Front.ComparisonFunction];

    depthStencilDesc.BackFace.StencilFailOp = D3D111_STENCIL_OPERATION[description.Back.FailOperation];
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D111_STENCIL_OPERATION[description.Back.ZFailOperation];
    depthStencilDesc.BackFace.StencilPassOp = D3D111_STENCIL_OPERATION[description.Back.PassOperation];
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_FUNCTION[description.Back.ComparisonFunction];

    ID3D11DepthStencilState* depthStencilState;
    HRESULT operationResult = nativeDevice->CreateDepthStencilState( &depthStencilDesc, &depthStencilState );

    DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "CreateDepthStencilState failed!" );

    return depthStencilState;
}

ID3D11RasterizerState* CreateRasterizerState( ID3D11Device* nativeDevice, const RasterizerStateDesc& description )
{
    const D3D11_RASTERIZER_DESC rasterDesc =
    {
        D3D11_FM[description.FillMode],
        D3D11_CM[description.CullMode],
        static_cast< BOOL >( description.UseTriangleCCW ),
        ( INT )description.DepthBias,          // INT DepthBias;
        description.DepthBiasClamp,       // f32 DepthBiasClamp;
        description.SlopeScale,       // f32 SlopeScaledDepthBias;
        TRUE,       // BOOL DepthClipEnable;
        0,          // BOOL ScissorEnable;
        0,          // BOOL MultisampleEnable;
        0,          // BOOL AntialiasedLineEnable;
    };

    ID3D11RasterizerState* rasterizerState;
    HRESULT operationResult = nativeDevice->CreateRasterizerState( &rasterDesc, &rasterizerState );

    DUSK_DEV_ASSERT( SUCCEEDED( operationResult ), "CreateRasterizerState returned an error: 0x%x\n", operationResult );

    return rasterizerState;
}

void FillStageBinding( Shader* shader, const eShaderStage shaderStage, PipelineState* pipelineState )
{
    if ( shader == nullptr ) {
        return;
    }

    i32& resourceToBindCount = pipelineState->resourceCount;

#define DUSK_RESOURCE_STAGE_BIND( stage, index, resourceInternalType, bindType )\
if ( shaderStage == SHADER_STAGE_##stage ) {\
    resourceEntry->stageBindings[index].##resourceInternalType = &pipelineState->##bindType##.##stage##[reflectedRes.bindingIndex];\
    resourceEntry->stageBindings[index].RegisterIndex = reflectedRes.bindingIndex;\
    resourceEntry->stageBindings[index].ShaderStageIndex = index;\
    resourceEntry->activeBindings[resourceEntry->activeStageCount++] = &resourceEntry->stageBindings[index];\
}

    for ( u32 i = 0; i < shader->reflectedResourceCount; i++ ) {
        auto& reflectedRes = shader->resourcesReflectionInfos[i];

        PipelineState::ResourceEntry* resourceEntry = &pipelineState->resources[resourceToBindCount];

        // Since we might bind the same resource to multiple stages at once, we need an initial lookup to check if the
        // current resource has already been reflected for a different stage
        dkStringHash_t resourceHashcode = dk::core::CRC32( reflectedRes.name );
        auto it = pipelineState->bindingSet.find( resourceHashcode );
        if ( it == pipelineState->bindingSet.end() ) {
            resourceEntry->type = reflectedRes.type;
            pipelineState->bindingSet.insert( std::make_pair( resourceHashcode, &pipelineState->resources[resourceToBindCount] ) );

            resourceToBindCount++;
        } else {
            resourceEntry = it->second;
        }

        switch ( reflectedRes.type ) {
        case PipelineState::InternalResourceType::RESOURCE_TYPE_CBUFFER_VIEW: {
            DUSK_RESOURCE_STAGE_BIND( VERTEX, 0, cbufferView, constantBuffers )
            DUSK_RESOURCE_STAGE_BIND( PIXEL, 1, cbufferView, constantBuffers )
            DUSK_RESOURCE_STAGE_BIND( TESSELATION_CONTROL, 2, cbufferView, constantBuffers )
            DUSK_RESOURCE_STAGE_BIND( TESSELATION_EVALUATION, 3, cbufferView, constantBuffers )
            DUSK_RESOURCE_STAGE_BIND( COMPUTE, 4, cbufferView, constantBuffers )
        } break;

        case PipelineState::InternalResourceType::RESOURCE_TYPE_SHADER_RESOURCE_VIEW: {
            DUSK_RESOURCE_STAGE_BIND( VERTEX, 0, shaderResourceView, shaderResourceViews )
            DUSK_RESOURCE_STAGE_BIND( PIXEL, 1, shaderResourceView, shaderResourceViews )
            DUSK_RESOURCE_STAGE_BIND( TESSELATION_CONTROL, 2, shaderResourceView, shaderResourceViews )
            DUSK_RESOURCE_STAGE_BIND( TESSELATION_EVALUATION, 3, shaderResourceView, shaderResourceViews )
            DUSK_RESOURCE_STAGE_BIND( COMPUTE, 4, shaderResourceView, shaderResourceViews )
        } break;
        
        case PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW: {
            if ( shaderStage == SHADER_STAGE_COMPUTE ) {
                resourceEntry->stageBindings[4].unorderedAccessView = &pipelineState->uavBuffers[reflectedRes.bindingIndex];
                resourceEntry->stageBindings[4].RegisterIndex = reflectedRes.bindingIndex;
                resourceEntry->stageBindings[4].ShaderStageIndex = 4;
                resourceEntry->activeBindings[resourceEntry->activeStageCount++] = &resourceEntry->stageBindings[4];
            }
        } break;

        default:
            DUSK_DEV_ASSERT( false, "Invalid API usage!" );
            break;
        }
    }

#undef DUSK_RESOURCE_STAGE_BIND
}

PipelineState* RenderDevice::createPipelineState( const PipelineStateDesc& description )
{
    PipelineState* pipelineState = dk::core::allocate<PipelineState>( memoryAllocator );

    // Resource List
    pipelineState->resourceCount = 0;
    pipelineState->uavBuffersBindCount = 0;

    pipelineState->PrimitiveTopology = _D3D11_PRIMITIVE_TOPOLOGY[description.PrimitiveTopology];

#define BIND_IF_AVAILABLE( stage ) pipelineState->stage = ( description.stage != nullptr ) ? description.stage->stage : nullptr;

    const bool isComputePSO = ( pipelineState->computeShader != nullptr );
    if ( description.PipelineType == PipelineStateDesc::GRAPHICS ) {
        BIND_IF_AVAILABLE( vertexShader )
        BIND_IF_AVAILABLE( tesselationControlShader )
        BIND_IF_AVAILABLE( tesselationEvalShader )
        BIND_IF_AVAILABLE( pixelShader )

        memset( pipelineState->clearRtv, 0, sizeof( bool ) * 8 );
        memset( pipelineState->rtvFormat, 0, sizeof( DXGI_FORMAT ) * 8 );

        pipelineState->clearDsv = false;
        pipelineState->dsvFormat = DXGI_FORMAT_UNKNOWN;

        const FramebufferLayoutDesc& fboLayout = description.FramebufferLayout;

        const i32 attachmentCount = fboLayout.getAttachmentCount();
        pipelineState->rtvCount = attachmentCount;

        for ( i32 i = 0; i < attachmentCount; i++ ) {
            pipelineState->clearRtv[i] = ( fboLayout.Attachments[i].targetState == FramebufferLayoutDesc::CLEAR );
            pipelineState->rtvFormat[i] = static_cast< DXGI_FORMAT >( fboLayout.Attachments[i].viewFormat );
        }

        if ( fboLayout.depthStencilAttachment.bindMode != FramebufferLayoutDesc::UNUSED ) {
            pipelineState->clearDsv = ( fboLayout.depthStencilAttachment.targetState == FramebufferLayoutDesc::CLEAR );
            pipelineState->dsvFormat = static_cast< DXGI_FORMAT >( fboLayout.depthStencilAttachment.viewFormat );
        }

        if ( pipelineState->vertexShader != nullptr ) {
            pipelineState->inputLayout = CreateInputLayout( renderContext->PhysicalDevice, description.InputLayout.Entry, description.vertexShader->bytecode, description.vertexShader->bytecodeSize );
        }

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

        // Blend State
        pipelineState->BlendState = CreateBlendState( renderContext->PhysicalDevice, description.BlendState );
        pipelineState->BlendStateKey = description.BlendState;
        pipelineState->blendMask = 0xffffffff;

        // DepthStencilState
        pipelineState->DepthStencilState = CreateDepthStencilState( renderContext->PhysicalDevice, description.DepthStencilState );
        pipelineState->DepthStencilStateKey = description.DepthStencilState;
        pipelineState->stencilRef = static_cast< UINT >( description.DepthStencilState.StencilRefValue );

        // RasterizerState
        pipelineState->RasterizerState = CreateRasterizerState( renderContext->PhysicalDevice, description.RasterizerState );
        pipelineState->RaterizerStateKey = description.RasterizerState;

        // Shaders
        FillStageBinding( description.vertexShader, SHADER_STAGE_VERTEX, pipelineState );
        FillStageBinding( description.tesselationEvalShader, SHADER_STAGE_VERTEX, pipelineState );
        FillStageBinding( description.tesselationControlShader, SHADER_STAGE_VERTEX, pipelineState );
        FillStageBinding( description.pixelShader, SHADER_STAGE_PIXEL, pipelineState );
    } else {
        BIND_IF_AVAILABLE( computeShader )
        FillStageBinding( description.computeShader, SHADER_STAGE_COMPUTE, pipelineState );
    }

    // Create static samplers
    for ( i32 i = 0; i < description.StaticSamplers.StaticSamplerCount; i++ ) {
        const SamplerDesc& samplerDesc = description.StaticSamplers.StaticSamplersDesc[i];

        D3D11_SAMPLER_DESC nativeSamplerDesc = {
            D3D11_SAMPLER_FILTER[samplerDesc.filter],
            D3D11_SAMPLER_ADDRESS[samplerDesc.addressU],
            D3D11_SAMPLER_ADDRESS[samplerDesc.addressV],
            D3D11_SAMPLER_ADDRESS[samplerDesc.addressW]
        };

        if ( samplerDesc.filter == SAMPLER_FILTER_ANISOTROPIC_8 )
            nativeSamplerDesc.MaxAnisotropy = 8;
        else if ( samplerDesc.filter == SAMPLER_FILTER_ANISOTROPIC_16 )
            nativeSamplerDesc.MaxAnisotropy = 16;

        nativeSamplerDesc.MinLOD = static_cast< f32 >( samplerDesc.minLOD );
        nativeSamplerDesc.MaxLOD = static_cast< f32 >( samplerDesc.maxLOD );
        nativeSamplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNCTION[samplerDesc.comparisonFunction];

        for ( i32 i = 0; i < 4; i++ ) {
            nativeSamplerDesc.BorderColor[i] = samplerDesc.borderColor[i];
        }

        ID3D11Device* nativeDevice = renderContext->PhysicalDevice;
        nativeDevice->CreateSamplerState( &nativeSamplerDesc, &pipelineState->staticSamplers[i] );
    }
    pipelineState->staticSamplerCount = description.StaticSamplers.StaticSamplerCount;

    return pipelineState;
#undef BIND_IF_AVAILABLE
}

void CommandList::prepareAndBindResourceList( const PipelineState* pipelineState )
{
    DUSK_UNUSED_VARIABLE( pipelineState );

    CommandPacket::ArgumentLessPacket* commandPacket = dk::core::allocate<CommandPacket::ArgumentLessPacket>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_PREPARE_AND_BIND_RESOURCES;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void RenderDevice::destroyPipelineState( PipelineState* pipelineState )
{
#define D3D11_RELEASE( obj ) if ( obj != nullptr ) { obj->Release(); obj = nullptr; }

    D3D11_RELEASE( pipelineState->RasterizerState );
    D3D11_RELEASE( pipelineState->DepthStencilState );
    D3D11_RELEASE( pipelineState->BlendState );
    D3D11_RELEASE( pipelineState->inputLayout );

    for ( u32 i = 0; i < pipelineState->staticSamplerCount; i++ ) {
        D3D11_RELEASE( pipelineState->staticSamplers[i] );
    }

    pipelineState->vertexShader = nullptr;
    pipelineState->tesselationControlShader = nullptr;
    pipelineState->tesselationEvalShader = nullptr;
    pipelineState->pixelShader = nullptr;
    pipelineState->computeShader = nullptr;
#undef D3D11_RELEASE

    dk::core::free( memoryAllocator, pipelineState );
}

void RenderDevice::getPipelineStateCache( PipelineState* pipelineState, void** binaryData, size_t& binaryDataSize )
{

}

void RenderDevice::destroyPipelineStateCache( PipelineState* pipelineState )
{

}

void CommandList::begin()
{

}

void CommandList::bindPipelineState( PipelineState* pipelineState )
{
    CommandPacket::BindPipelineState* commandPacket = dk::core::allocate<CommandPacket::BindPipelineState>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_BIND_PIPELINE_STATE;
    commandPacket->PipelineStateObject = pipelineState;

    // We cache the active PSO so that we don't have to wait command replay to resolve dependencies/resources using the 
    // active pipeline state.
    nativeCommandList->BindedPipelineState = pipelineState;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::bindConstantBuffer( const dkStringHash_t hashcode, Buffer* buffer )
{
    CommandPacket::BindConstantBuffer* commandPacket = dk::core::allocate<CommandPacket::BindConstantBuffer>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_BIND_CBUFFER;
    commandPacket->BufferObject = buffer;
    commandPacket->ObjectHashcode = hashcode;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::bindImage( const dkStringHash_t hashcode, Image* image, const eViewFormat viewFormat )
{
    CommandPacket::BindResource* commandPacket = dk::core::allocate<CommandPacket::BindResource>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_BIND_IMAGE;
    commandPacket->ImageObject = image;
    commandPacket->ObjectHashcode = hashcode;
    commandPacket->ViewKey = 0ull;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::bindBuffer( const dkStringHash_t hashcode, Buffer* buffer, const eViewFormat viewFormat )
{
    DUSK_DEV_ASSERT( buffer, "Buffer is null!" );

    CommandPacket::BindResource* commandPacket = dk::core::allocate<CommandPacket::BindResource>( nativeCommandList->CommandPacketAllocator );
    commandPacket->Identifier = CPI_BIND_BUFFER;
    commandPacket->BufferObject = buffer;
    commandPacket->ObjectHashcode = hashcode;
    commandPacket->ViewKey = 0ull;

    nativeCommandList->Commands.push( reinterpret_cast<u32*>( commandPacket ) );
}

void CommandList::bindSampler( const dkStringHash_t hashcode, Sampler* sampler )
{

}

void PrepareAndBindResources_Replay( RenderContext* renderContext, const PipelineState* pipelineState )
{
    ID3D11DeviceContext* deviceContext = renderContext->ImmediateContext;

    FlushUAVRegisterUpdate( renderContext );
    FlushSRVRegisterUpdate( renderContext );
    FlushCBufferRegisterUpdate( renderContext );

    if ( pipelineState->computeShader != nullptr ) {
        deviceContext->CSSetShader( pipelineState->computeShader, nullptr, 0 );

        deviceContext->CSSetSamplers( 0, pipelineState->staticSamplerCount, pipelineState->staticSamplers );
    } else {
        deviceContext->VSSetShader( pipelineState->vertexShader, nullptr, 0 );
        deviceContext->HSSetShader( pipelineState->tesselationControlShader, nullptr, 0 );
        deviceContext->DSSetShader( pipelineState->tesselationEvalShader, nullptr, 0 );
        deviceContext->PSSetShader( pipelineState->pixelShader, nullptr, 0 );

        if ( pipelineState->vertexShader != nullptr ) {
            deviceContext->VSSetSamplers( 0, pipelineState->staticSamplerCount, pipelineState->staticSamplers );
        }

        if ( pipelineState->tesselationControlShader != nullptr ) {
            deviceContext->HSSetSamplers( 0, pipelineState->staticSamplerCount, pipelineState->staticSamplers );
        }

        if ( pipelineState->tesselationEvalShader != nullptr ) {
            deviceContext->DSSetSamplers( 0, pipelineState->staticSamplerCount, pipelineState->staticSamplers );
        }

        if ( pipelineState->pixelShader != nullptr ) {
            deviceContext->PSSetSamplers( 0, pipelineState->staticSamplerCount, pipelineState->staticSamplers );
        }
    }
}

void UpdateSRVRegister( RenderContext* renderContext, const i32 shaderStageIndex, const u32 srvRegisterIndex, ID3D11ShaderResourceView* srv )
{
    renderContext->SrvRegisters[shaderStageIndex][srvRegisterIndex] = srv;
    renderContext->SrvRegisterUpdateStart[shaderStageIndex] = Min( renderContext->SrvRegisterUpdateStart[shaderStageIndex], srvRegisterIndex );

    i32 registersDiff = ( srvRegisterIndex - renderContext->SrvRegisterUpdateStart[shaderStageIndex] );
    i32 updateCount = dk::maths::abs( registersDiff ) + 1;
    renderContext->SrvRegisterUpdateCount[shaderStageIndex] = Max( renderContext->SrvRegisterUpdateCount[shaderStageIndex], updateCount );
}

void UpdateUAVRegister( RenderContext* renderContext, const u32 uavRegisterIndex, ID3D11UnorderedAccessView* uav )
{
    renderContext->CsUavRegisters[uavRegisterIndex] = uav;
    renderContext->CsUavRegisterUpdateStart = Min( renderContext->CsUavRegisterUpdateStart, uavRegisterIndex );

    i32 registersDiff = ( uavRegisterIndex - renderContext->CsUavRegisterUpdateStart );
    i32 updateCount = dk::maths::abs( registersDiff ) + 1;
    renderContext->CsUavRegisterUpdateCount = Max( renderContext->CsUavRegisterUpdateCount, updateCount );
}

void UpdateCBufferRegister( RenderContext* renderContext, const i32 shaderStageIndex, const u32 cbufferRegisterIndex, ID3D11Buffer* cbuffer )
{
    renderContext->CBufferRegisters[shaderStageIndex][cbufferRegisterIndex] = cbuffer;
    renderContext->CBufferRegisterUpdateStart[shaderStageIndex] = Min( renderContext->CBufferRegisterUpdateStart[shaderStageIndex], cbufferRegisterIndex );

    i32 registersDiff = ( cbufferRegisterIndex - renderContext->CBufferRegisterUpdateStart[shaderStageIndex] );
    i32 updateCount = dk::maths::abs( registersDiff ) + 1;
    renderContext->CBufferRegisterUpdateCount[shaderStageIndex] = Max( renderContext->CBufferRegisterUpdateCount[shaderStageIndex], updateCount );
}

bool ClearImageUAVRegister( RenderContext* renderContext, Image* image )
{
    bool needFlush = false;

    i32 uavRegister = image->UAVRegisterIndex;

    if ( uavRegister != ~0 ) {
        needFlush = true;
        UpdateUAVRegister( renderContext, uavRegister, nullptr );

        image->UAVRegisterIndex = ~0;
    }
    return needFlush;
}

bool ClearImageSRVRegister( RenderContext* renderContext, Image* image )
{
    bool needFlush = false;

    for ( i32 i = 0; i < eShaderStage::SHADER_STAGE_COUNT; i++ ) {
        i32 srvRegister = image->SRVRegisterIndex[i];

        if ( srvRegister != ~0 ) {
            needFlush = true;
            UpdateSRVRegister( renderContext, i, srvRegister, nullptr );

            image->SRVRegisterIndex[i] = ~0;
        }
    }

    return needFlush;
}

bool ClearBufferUAVRegister( RenderContext* renderContext, Buffer* buffer )
{
    bool needFlush = false;

    i32 uavRegister = buffer->UAVRegisterIndex;

    if ( uavRegister != ~0 ) {
        needFlush = true;
        UpdateUAVRegister( renderContext, uavRegister, nullptr );

        buffer->UAVRegisterIndex = ~0;
    }

    return needFlush;
}

bool ClearBufferSRVRegister( RenderContext* renderContext, Buffer* buffer )
{
    bool needFlush = false;

    for ( i32 i = 0; i < eShaderStage::SHADER_STAGE_COUNT; i++ ) {
        i32 srvRegister = buffer->SRVRegisterIndex[i];

        if ( srvRegister != ~0 ) {
            needFlush = true;
            UpdateSRVRegister( renderContext, i, srvRegister, nullptr );

            buffer->SRVRegisterIndex[i] = ~0;
        }
    }

    return needFlush;
}

void ClearFBO( RenderContext* renderContext )
{
    // Update all FBO attachment states (so that we don't clear the framebuffer twice...).
    for ( i32 i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++ ) {
        if ( renderContext->FramebufferAttachment[i] != nullptr ) {
            renderContext->FramebufferAttachment[i]->IsBindedToFBO = false;
            renderContext->FramebufferAttachment[i] = nullptr;
        }
    }
    if ( renderContext->FramebufferDepthBuffer != nullptr ) {
        renderContext->FramebufferDepthBuffer->IsBindedToFBO = false;
        renderContext->FramebufferDepthBuffer = nullptr;
    }

    // Clear framebuffer
    ID3D11RenderTargetView* RenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    renderContext->ImmediateContext->OMSetRenderTargets( D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, RenderTargets, DepthStencilView );
}

void ClearImageFBOAttachment( RenderContext* renderContext, Image* image )
{
    if ( image->IsBindedToFBO ) {
        ClearFBO( renderContext );
    }
}

void BindBuffer_Replay( RenderContext* renderContext, const dkStringHash_t hashcode, Buffer* buffer )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceEntry*>& bindingSet = renderContext->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    if ( it == bindingSet.end() ) {
        return;
    }

    //const DXGI_FORMAT nativeViewFormat = static_cast< DXGI_FORMAT >( viewFormat );

    PipelineState::ResourceEntry* resource = it->second;
    if ( resource == nullptr ) {
        return;
    }
    
    ClearBufferUAVRegister( renderContext, buffer );
    FlushUAVRegisterUpdate( renderContext );

    ClearBufferSRVRegister( renderContext, buffer );
    FlushSRVRegisterUpdate( renderContext );

    if ( resource->type == PipelineState::InternalResourceType::RESOURCE_TYPE_SHADER_RESOURCE_VIEW ) {
        ID3D11ShaderResourceView* srv = buffer->DefaultShaderResourceView;

        for ( i32 i = 0; i < resource->activeStageCount; i++ ) {
            PipelineState::ResourceEntry::StageBinding* stageBinding = resource->activeBindings[i];

            const u32 shaderStageIndex = stageBinding->ShaderStageIndex;
            const u32 srvRegisterIndex = stageBinding->RegisterIndex;

            // Update RenderContext register data
            RenderContext::RegisterData& registerData = renderContext->SrvRegistersInfo[shaderStageIndex][srvRegisterIndex];
            
            if ( registerData.ResourceType == RenderContext::RegisterData::Type::BUFFER_RESOURCE ) {
                registerData.BufferObject->SRVRegisterIndex[shaderStageIndex] = ~0;
            } else if ( registerData.ResourceType == RenderContext::RegisterData::Type::IMAGE_RESOURCE ) {
                registerData.ImageObject->SRVRegisterIndex[shaderStageIndex] = ~0;
            }

            registerData.BufferObject = buffer;
            registerData.ResourceType = RenderContext::RegisterData::Type::BUFFER_RESOURCE;

            // Update Persistent register view
            UpdateSRVRegister( renderContext, shaderStageIndex, srvRegisterIndex, srv );

            buffer->SRVRegisterIndex[shaderStageIndex] = srvRegisterIndex;
        }
    } else if ( resource->type == PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW ) {
        ID3D11UnorderedAccessView* uav = buffer->DefaultUnorderedAccessView;

        for ( i32 i = 0; i < resource->activeStageCount; i++ ) {
            PipelineState::ResourceEntry::StageBinding* stageBinding = resource->activeBindings[i];

            // Update RenderContext register data
            RenderContext::RegisterData& registerData = renderContext->CsUavRegistersInfo[stageBinding->RegisterIndex];

            if ( registerData.ResourceType == RenderContext::RegisterData::Type::BUFFER_RESOURCE ) {
                registerData.BufferObject->UAVRegisterIndex = ~0;
            } else if ( registerData.ResourceType == RenderContext::RegisterData::Type::IMAGE_RESOURCE ) {
                registerData.ImageObject->UAVRegisterIndex = ~0;
            }

            registerData.BufferObject = buffer;
            registerData.ResourceType = RenderContext::RegisterData::Type::BUFFER_RESOURCE;

            UpdateUAVRegister( renderContext, stageBinding->RegisterIndex, uav );

            buffer->UAVRegisterIndex = stageBinding->RegisterIndex;
        }
    }
}

void BindCBuffer_Replay( RenderContext* renderContext, const dkStringHash_t hashcode, Buffer* buffer )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceEntry*>& bindingSet = renderContext->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    DUSK_DEV_ASSERT( ( it != bindingSet.end() ), "Unknown resource hashcode! (shader source might have been updated?)" )
    if ( it == bindingSet.end() || it->second == nullptr ) {
        return;
    }

    PipelineState::ResourceEntry* resource = it->second;

    for ( i32 i = 0; i < resource->activeStageCount; i++ ) {
        PipelineState::ResourceEntry::StageBinding* stageBinding = resource->activeBindings[i];
        UpdateCBufferRegister( renderContext, stageBinding->ShaderStageIndex, stageBinding->RegisterIndex, buffer->BufferObject );

        buffer->CbufferRegisterIndex[stageBinding->ShaderStageIndex] = stageBinding->RegisterIndex;
    }
}

void BindImage_Replay( RenderContext* renderContext, const dkStringHash_t hashcode, Image* image )
{
    std::unordered_map<dkStringHash_t, PipelineState::ResourceEntry*>& bindingSet = renderContext->BindedPipelineState->bindingSet;

    auto it = bindingSet.find( hashcode );
    if ( it == bindingSet.end() ) {
        return;
    }

    PipelineState::ResourceEntry* resource = it->second;
    if ( resource == nullptr ) {
        return;
    }

    ClearImageFBOAttachment( renderContext, image );

    ClearImageUAVRegister( renderContext, image );
    FlushUAVRegisterUpdate( renderContext );

    ClearImageSRVRegister( renderContext, image );
    FlushSRVRegisterUpdate( renderContext );

    if ( resource->type == PipelineState::InternalResourceType::RESOURCE_TYPE_SHADER_RESOURCE_VIEW ) {
        // TODO Custom Image view support
        ID3D11ShaderResourceView* srv = image->DefaultShaderResourceView;

        for ( i32 i = 0; i < resource->activeStageCount; i++ ) {
            PipelineState::ResourceEntry::StageBinding* stageBinding = resource->activeBindings[i];

            const u32 shaderStageIndex = stageBinding->ShaderStageIndex;
            const u32 srvRegisterIndex = stageBinding->RegisterIndex;

            // Update RenderContext register data
            RenderContext::RegisterData& registerData = renderContext->SrvRegistersInfo[shaderStageIndex][srvRegisterIndex];

            if ( registerData.ResourceType == RenderContext::RegisterData::Type::BUFFER_RESOURCE ) {
                registerData.BufferObject->SRVRegisterIndex[shaderStageIndex] = ~0;
            } else if ( registerData.ResourceType == RenderContext::RegisterData::Type::IMAGE_RESOURCE ) {
                registerData.ImageObject->SRVRegisterIndex[shaderStageIndex] = ~0;
            }

            registerData.ImageObject = image;
            registerData.ResourceType = RenderContext::RegisterData::Type::IMAGE_RESOURCE;

            UpdateSRVRegister( renderContext, stageBinding->ShaderStageIndex, stageBinding->RegisterIndex, srv );

            image->SRVRegisterIndex[stageBinding->ShaderStageIndex] = stageBinding->RegisterIndex;
        }
    } else if ( resource->type == PipelineState::InternalResourceType::RESOURCE_TYPE_UNORDERED_ACCESS_VIEW ) {
        // TODO Custom Image view support
        ID3D11UnorderedAccessView* uav = image->DefaultUnorderedAccessView;

        for ( i32 i = 0; i < resource->activeStageCount; i++ ) {
            PipelineState::ResourceEntry::StageBinding* stageBinding = resource->activeBindings[i];

            // Update RenderContext register data
            RenderContext::RegisterData& registerData = renderContext->CsUavRegistersInfo[stageBinding->RegisterIndex];

            if ( registerData.ResourceType == RenderContext::RegisterData::Type::BUFFER_RESOURCE ) {
                registerData.BufferObject->UAVRegisterIndex = ~0;
            } else if ( registerData.ResourceType == RenderContext::RegisterData::Type::IMAGE_RESOURCE ) {
                registerData.ImageObject->UAVRegisterIndex = ~0;
            }

            registerData.ImageObject = image;
            registerData.ResourceType = RenderContext::RegisterData::Type::IMAGE_RESOURCE;

            UpdateUAVRegister( renderContext, stageBinding->RegisterIndex, uav );

            image->UAVRegisterIndex = stageBinding->RegisterIndex;
        }
    }
}

void FlushSRVRegisterUpdate( RenderContext* renderContext )
{
#define UPDATE_SRV_REGISTER( stage, index )\
 if ( renderContext->SrvRegisterUpdateStart[index] != ~0 ) {\
        renderContext->ImmediateContext->stage##SetShaderResources( renderContext->SrvRegisterUpdateStart[index], renderContext->SrvRegisterUpdateCount[index], &renderContext->SrvRegisters[index][renderContext->SrvRegisterUpdateStart[index]] );\
        renderContext->SrvRegisterUpdateCount[index] = 0;\
        renderContext->SrvRegisterUpdateStart[index] = ~0;\
 }\

    UPDATE_SRV_REGISTER( VS, 0 );
    UPDATE_SRV_REGISTER( PS, 1 );
    UPDATE_SRV_REGISTER( HS, 2 );
    UPDATE_SRV_REGISTER( DS, 3 );
    UPDATE_SRV_REGISTER( CS, 4 );

#undef UPDATE_SRV_REGISTER
}

void FlushCBufferRegisterUpdate( RenderContext* renderContext )
{
#define UPDATE_CBUFFER_REGISTER( stage, index )\
 if ( renderContext->CBufferRegisterUpdateStart[index] != ~0 ) {\
        renderContext->ImmediateContext->stage##SetConstantBuffers( renderContext->CBufferRegisterUpdateStart[index], renderContext->CBufferRegisterUpdateCount[index], &renderContext->CBufferRegisters[index][renderContext->CBufferRegisterUpdateStart[index]] );\
        renderContext->CBufferRegisterUpdateCount[index] = 0;\
        renderContext->CBufferRegisterUpdateStart[index] = ~0;\
 }\

    UPDATE_CBUFFER_REGISTER( VS, 0 );
    UPDATE_CBUFFER_REGISTER( PS, 1 );
    UPDATE_CBUFFER_REGISTER( HS, 2 );
    UPDATE_CBUFFER_REGISTER( DS, 3 );
    UPDATE_CBUFFER_REGISTER( CS, 4 );

#undef UPDATE_CBUFFER_REGISTER
}

void FlushUAVRegisterUpdate( RenderContext* renderContext )
{
    if ( renderContext->CsUavRegisterUpdateStart != ~0 ) {
        renderContext->ImmediateContext->CSSetUnorderedAccessViews( renderContext->CsUavRegisterUpdateStart, renderContext->CsUavRegisterUpdateCount, &renderContext->CsUavRegisters[renderContext->CsUavRegisterUpdateStart], nullptr );
        renderContext->CsUavRegisterUpdateCount = 0;
        renderContext->CsUavRegisterUpdateStart = ~0;
    }
}

void BindPipelineState_Replay( RenderContext* renderContext, PipelineState* pipelineState )
{
    static constexpr FLOAT BLEND_FACTORS[4] = { 1.0F, 1.0F, 1.0F, 1.0F };

    // Apply context states
    if ( pipelineState->computeShader == nullptr ) {
        if ( renderContext->BindedPipelineState != nullptr && renderContext->BindedPipelineState->computeShader == nullptr ) {
            // Update context state after making sure we don't reapply the same state.
            if ( pipelineState->BlendStateKey != renderContext->BindedPipelineState->BlendStateKey ) {
                renderContext->ImmediateContext->OMSetBlendState( pipelineState->BlendState, BLEND_FACTORS, pipelineState->blendMask );
            }

            if ( pipelineState->DepthStencilStateKey != renderContext->BindedPipelineState->DepthStencilStateKey ) {
                renderContext->ImmediateContext->OMSetDepthStencilState( pipelineState->DepthStencilState, pipelineState->stencilRef );
            }

            if ( pipelineState->RaterizerStateKey != renderContext->BindedPipelineState->RaterizerStateKey ) {
                renderContext->ImmediateContext->RSSetState( pipelineState->RasterizerState );
            }
        } else {
            // If no PSO has been binded so far, apply the pipeline states without checking
            renderContext->ImmediateContext->OMSetBlendState( pipelineState->BlendState, BLEND_FACTORS, pipelineState->blendMask );
            renderContext->ImmediateContext->OMSetDepthStencilState( pipelineState->DepthStencilState, pipelineState->stencilRef );
            renderContext->ImmediateContext->RSSetState( pipelineState->RasterizerState );
        }
    }

    renderContext->ImmediateContext->IASetPrimitiveTopology( pipelineState->PrimitiveTopology );
    renderContext->ImmediateContext->IASetInputLayout( pipelineState->inputLayout );

    renderContext->BindedPipelineState = pipelineState;
}
#endif
